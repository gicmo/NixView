#include "NixProxyModel.hpp"
#include <qdebug.h>
#include "common/Common.hpp"

NixProxyModel::NixProxyModel(QObject *parent)
    :QSortFilterProxyModel(parent)
{
    rough_filter = FILTER_EXP_NONE;
    case_sensitive = false;
}


bool NixProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
    if(block_mode)
        return false;
    if(metadata_only_mode)
        if(check_if_in_data_branch(source_row, source_parent))
            return false;

    if(filter_mode == 1) {
        if(check_children(source_row, source_parent))
            return true;
    } else if(filter_mode == 2) {
        if (check_parent(source_parent))
            return true;
    } else if(filter_mode == 3) {
        if (check_parent(source_parent) || check_children(source_row, source_parent))
            return true;
    }
    return check_entry_row(source_row, source_parent);
}


bool NixProxyModel::check_parent(const QModelIndex &source_parent) const {
    QModelIndex parent = source_parent;
    while (parent.isValid()) {
        if (check_entry_row(parent.row(), parent.parent()))
            return true;
        parent = parent.parent();
    }
    return false;
}


bool NixProxyModel::check_children(int source_row, const QModelIndex &source_parent) const {
    // get source-model index for current row
    QModelIndex current_item = sourceModel()->index(source_row, 0, source_parent);
    if(current_item.isValid()) {
        // if any of children matches the filter, then current index matches the filter as well
        int num_children = sourceModel()->rowCount(current_item) ;
        for(int i = 0; i<num_children; ++i) {
            if(filterAcceptsRow(i, current_item)) {
                return true ;
            }
        }
    }
    return false;
}


bool NixProxyModel::check_entry_row(int source_row, const QModelIndex &source_parent) const {
    NixTreeModel *model = static_cast<NixTreeModel*>(sourceModel());

    bool rough_filter_satisfied;
    (strcmp(rough_filter.toStdString().c_str(), FILTER_EXP_NONE) == 0 || strcmp(rough_filter.toStdString().c_str(), FILTER_EXP_METADATA) == 0)?
                rough_filter_satisfied = true:
                rough_filter_satisfied = false;
    //std::cerr << "check row: "<< source_row << std::endl;
    // Block check
    if (strcmp(rough_filter.toStdString().c_str(), FILTER_EXP_BLOCK) == 0) {
        rough_filter_satisfied = entitiy_check(source_row, source_parent, NixType::NIX_BLOCK);
    } else if (strcmp(rough_filter.toStdString().c_str(), FILTER_EXP_GROUP) == 0) {
        rough_filter_satisfied = entitiy_check(source_row, source_parent, NixType::NIX_GROUP);
    } else if (strcmp(rough_filter.toStdString().c_str(), FILTER_EXP_DATAARRAY) == 0) {
        rough_filter_satisfied = entitiy_check(source_row, source_parent, NixType::NIX_DATA_ARRAY);
    } else if (strcmp(rough_filter.toStdString().c_str(), FILTER_EXP_TAG) == 0) {
        rough_filter_satisfied = entitiy_check(source_row, source_parent, NixType::NIX_TAG);
    } else if (strcmp(rough_filter.toStdString().c_str(), FILTER_EXP_MULTITAG) == 0) {
        rough_filter_satisfied = entitiy_check(source_row, source_parent, NixType::NIX_MTAG);
    } else if (strcmp(rough_filter.toStdString().c_str(), FILTER_EXP_NAME_CONTAINS) == 0) {
        QModelIndex index = model->index(source_row, 0, source_parent);
        return qml_contains_fine_filter(index);
    } else if (strcmp(rough_filter.toStdString().c_str(), FILTER_EXP_NIXTYPE_CONTAINS) == 0) {
        QModelIndex index = model->index(source_row, 1, source_parent);
        return qml_contains_fine_filter(index);
    }

    // check if rough filter satisfied
    if (!rough_filter_satisfied) {
        //std::cerr << "rough filter satisfied: "<< rough_filter_satisfied << std::endl;
        return false;
    }

    // fine filter --> check if entry row contains all fine_filter experessions
    for (int i = 0; i < fine_filter.size(); ++i) {
        QString str = fine_filter[i];
        bool filter_expression_found = false;
        for (int c = 0; c < model->columnCount(); ++c) {
            QModelIndex index = model->index(source_row, c, source_parent);
            filter_expression_found = filter_expression_found || qml_contains_expression(index, str);
            if (filter_expression_found)
                break;
        }
        if (!filter_expression_found)
            return false;
    }
    return true;
}


bool NixProxyModel::qml_contains_fine_filter(QModelIndex qml) const {
    bool match = true;
    int index = 0;
    Qt::CaseSensitivity cs = case_sensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    QString str = "";
    while (match && index < fine_filter.size())
    {
        str = fine_filter[index];
        match = match && sourceModel()->data(qml).toString().contains(str, cs);
        index ++;
    }
    return match;
}


bool NixProxyModel::qml_contains_expression(QModelIndex qml, QString str) const {
    return sourceModel()->data(qml).toString().contains(str, case_sensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
}


bool NixProxyModel::entitiy_check(int source_row, const QModelIndex &source_parent, const NixType &type) const {
    int c = 2; //Storage Type column, see NixDataModel.cpp
    QModelIndex index = sourceModel()->index(source_row, c, source_parent);
    NixTreeModelItem *item = static_cast<NixTreeModelItem*>(index.internalPointer());
    return (item->nixType() == type);
}


bool NixProxyModel::check_if_in_data_branch(int source_row, const QModelIndex &source_parent) const {
    QModelIndex current = sourceModel()->index(source_row, 0, source_parent);
    while (current.isValid()) {
        if (!current.parent().isValid() && strcmp(sourceModel()->data(current).toString().toStdString().c_str(), "Data") == 0)
            return true;
        current = current.parent();
    }
    return false;
}


NixTreeModelItem* NixProxyModel::get_item_from_qml(QModelIndex qml) {
    return static_cast<NixTreeModelItem*>(qml.internalPointer());
}


void NixProxyModel::set_rough_filter(QString exp) {
    rough_filter = exp;

    (strcmp(rough_filter.toStdString().c_str(), FILTER_EXP_METADATA) == 0) ?
                metadata_only_mode = true :
                metadata_only_mode = false;

    refresh();
}


void NixProxyModel::set_fine_filter(QString exp) {
    fine_filter = exp.split(QString(" "), QString::SkipEmptyParts);
    refresh();
}


void NixProxyModel::set_case_sensitivity(bool b) {
    case_sensitive = b;
    refresh();
}


void NixProxyModel::refresh() {
    invalidateFilter();
}
