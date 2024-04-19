#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <Tools/imagewithvideobase.h>


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QDropEvent;
class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QWheelEvent;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    void SetImage(const QString& imgPath);

    // QWidget interface
protected:
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    Ui::MainWindow*     ui;
    QString             m_currentPath;
    QList<QString>      m_supportedFormats;
    std::shared_ptr<ImageWithVideoBase> m_imageManager;
};
#endif // MAINWINDOW_H
