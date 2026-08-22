#include "gridcontextmenu.h"

#include <QAction>

GridContextMenu::GridContextMenu(const Actions& actions, QWidget* parent)
    : QMenu(parent), m_actions(actions)
{
}

void GridContextMenu::popupForItem(const QPoint& globalPos)
{
    // The actions belong to FolderBrowserPanel, so clear() only detaches them
    // (it deletes only actions parented to this menu) -- safe to rebuild each time.
    clear();
    addAction(m_actions.cut);
    addAction(m_actions.copy);
    addAction(m_actions.remove);
    addAction(m_actions.rename);
    addSeparator();
    addAction(m_actions.copyPath);
    addAction(m_actions.showInExplorer);
    exec(globalPos);
}

void GridContextMenu::popupForEmpty(const QPoint& globalPos, bool canPaste)
{
    clear();
    m_actions.paste->setEnabled(canPaste);
    addAction(m_actions.paste);
    addAction(m_actions.refresh);
    exec(globalPos);
}
