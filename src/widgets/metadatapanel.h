#ifndef METADATAPANEL_H
#define METADATAPANEL_H

#include <QByteArray>
#include <QObject>
#include <QString>

class QWidget;
class QLabel;
class QTreeWidget;
class QImage;

// Owns the "Preview" and "Metadata" side panels and keeps them in sync with the
// currently selected/opened file. The two widgets live in separate docks, so the
// panel hands them to the host (MainWindow) to place, and refreshes both whenever
// setFile() is called.
class MetadataPanel : public QObject
{
    Q_OBJECT
public:
    explicit MetadataPanel(QObject* parent = nullptr);

    QWidget* previewWidget() const;    // scaled image preview
    QWidget* metadataWidget() const;   // property / value tree

public slots:
    void setFile(const QString& path);   // empty path clears both panels

public:
    // Same, from a file the caller has already read, so it is not read twice.
    void setFileData(const QString& path, const QByteArray& fileData);

protected:
    // QEvent::LanguageChange: QWidget forwards it to every QObject child, so this
    // panel is reached even though it is not a widget itself.
    bool event(QEvent* event) override;

private:
    QImage loadImage(const QString& path, const QByteArray& fileData) const;

    QLabel*      m_preview;
    QTreeWidget* m_metadata;
    QString      m_path;   // current file, so a language change can re-populate
    QByteArray   m_data;   // its bytes, so that re-populating does not read it again
};

#endif   // METADATAPANEL_H
