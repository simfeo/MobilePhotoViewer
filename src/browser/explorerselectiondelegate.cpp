#include "explorerselectiondelegate.h"
#include <QPainter>

void ExplorerSelectionDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    if (opt.state & QStyle::State_Selected)
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        // Translucent colors for Windows Explorer dark theme
        QColor fillColor(0, 120, 215, 35);     // Light blue background fill
        QColor borderColor(0, 120, 215, 150);  // Brighter blue outline border

        // 1. Draw the selection background fill
        painter->setPen(Qt::NoPen);
        painter->setBrush(fillColor);
        painter->drawRect(opt.rect);

        // 2. Draw the thin outer border line
        painter->setPen(QPen(borderColor, 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(opt.rect.adjusted(0, 0, -1, -1));

        painter->restore();

        // Clear the default selected state so Qt doesn't paint its solid background over ours
        opt.state &= ~QStyle::State_Selected;
        QStyledItemDelegate::paint(painter, opt, index);
    }
    else
    {
        QStyledItemDelegate::paint(painter, opt, index);
    }
}

