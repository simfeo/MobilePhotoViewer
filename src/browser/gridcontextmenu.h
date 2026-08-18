#ifndef GRIDCONTEXTMENU_H
#define GRIDCONTEXTMENU_H

#include <QMenu>

class QAction;

// The thumbnail grid's right-click menu. Shows file operations when an item is
// clicked, or paste/refresh on empty space. The actions themselves are owned by
// FolderBrowserPanel (and shared with the Edit menu); this menu only arranges
// and shows them.
class GridContextMenu : public QMenu
{
    Q_OBJECT
public:
    struct Actions
    {
        QAction* cut            = nullptr;
        QAction* copy           = nullptr;
        QAction* paste          = nullptr;
        QAction* remove         = nullptr;   // "delete" is a keyword
        QAction* rename         = nullptr;
        QAction* copyPath       = nullptr;
        QAction* showInExplorer = nullptr;
        QAction* refresh        = nullptr;
    };

    explicit GridContextMenu(const Actions& actions, QWidget* parent = nullptr);

    void popupForItem(const QPoint& globalPos);                  // clicked a file/folder
    void popupForEmpty(const QPoint& globalPos, bool canPaste);  // clicked empty space

private:
    Actions m_actions;
};

#endif // GRIDCONTEXTMENU_H
