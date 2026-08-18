#ifndef LOCATIONBAR_H
#define LOCATIONBAR_H

#include <QWidget>
#include <QString>

class QStackedWidget;
class QWidget;
class QHBoxLayout;
class QLineEdit;

// Explorer / ACDSee style address bar. Normally shows the current path as a row
// of clickable breadcrumb buttons (click a segment to jump to that folder);
// clicking the empty area switches to an editable line where a path can be typed
// and Enter navigates. Either way it emits navigate() with the target folder.
class LocationBar : public QWidget
{
    Q_OBJECT
public:
    explicit LocationBar(QWidget* parent = nullptr);

    void setPath(const QString& path);

signals:
    void navigate(const QString& path);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;   // QEvent::LanguageChange

private:
    void rebuildCrumbs();
    void enterEditMode();
    void showCrumbs();

    QString          m_path;
    QStackedWidget*  m_stack;
    QWidget*         m_crumbPage;
    QHBoxLayout*     m_crumbLayout;
    QLineEdit*       m_edit;
};

#endif // LOCATIONBAR_H
