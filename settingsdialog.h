#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QLineEdit;
class QCheckBox;
class QSlider;
class QLabel;
class QComboBox;

// Preferences window: where the app starts (a fixed folder or the last one
// browsed) and the default thumbnail size. Reads and writes via AppSettings;
// only persists when accepted.
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private slots:
    void browseForStartFolder();
    void save();                       // write fields to AppSettings, then accept()

private:
    void load();                       // pull current values into the fields
    void updateStartFolderEnabled();   // grey out the path when "last folder" wins

    QCheckBox* m_useLastFolder;
    QLineEdit* m_startFolder;
    QLabel*    m_thumbValue;
    QSlider*   m_thumbSize;
    QComboBox* m_language;
};

#endif // SETTINGSDIALOG_H
