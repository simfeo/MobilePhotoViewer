#ifndef EXPLORERSELECTIONDELEGATE_H
#define EXPLORERSELECTIONDELEGATE_H

#include <QStyledItemDelegate>

class ExplorerSelectionDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

protected:
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

#endif // EXPLORERSELECTIONDELEGATE_H
