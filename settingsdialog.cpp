#include "settingsdialog.h"
#include "appsettings.h"
#include "folderbrowserpanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QDialogButtonBox>
#include <QFileDialog>

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));

    // ---- Startup group ----
    QGroupBox* startupBox = new QGroupBox(tr("Startup"), this);
    QVBoxLayout* startupLayout = new QVBoxLayout(startupBox);

    m_useLastFolder = new QCheckBox(tr("Start from the last opened folder"), startupBox);
    startupLayout->addWidget(m_useLastFolder);

    QHBoxLayout* folderRow = new QHBoxLayout;
    folderRow->addWidget(new QLabel(tr("Start folder:"), startupBox));
    m_startFolder = new QLineEdit(startupBox);
    folderRow->addWidget(m_startFolder, 1);
    QPushButton* browse = new QPushButton(tr("Browse..."), startupBox);
    folderRow->addWidget(browse);
    startupLayout->addLayout(folderRow);

    // ---- Thumbnails group ----
    QGroupBox* thumbBox = new QGroupBox(tr("Thumbnails"), this);
    QFormLayout* thumbLayout = new QFormLayout(thumbBox);

    m_thumbSize = new QSlider(Qt::Horizontal, thumbBox);
    m_thumbSize->setMinimum(FolderBrowserPanel::kThumbMin);
    m_thumbSize->setMaximum(FolderBrowserPanel::kThumbMax);
    m_thumbSize->setSingleStep(FolderBrowserPanel::kThumbStep);
    m_thumbSize->setPageStep(FolderBrowserPanel::kThumbStep);
    m_thumbSize->setTickInterval(FolderBrowserPanel::kThumbStep);
    m_thumbSize->setTickPosition(QSlider::TicksBelow);
    m_thumbValue = new QLabel(thumbBox);

    QHBoxLayout* thumbRow = new QHBoxLayout;
    thumbRow->addWidget(m_thumbSize, 1);
    thumbRow->addWidget(m_thumbValue);
    thumbLayout->addRow(tr("Default size:"), thumbRow);

    // ---- Buttons ----
    QDialogButtonBox* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->addWidget(startupBox);
    root->addWidget(thumbBox);
    root->addStretch();
    root->addWidget(buttons);

    connect(browse, &QPushButton::clicked, this, &SettingsDialog::browseForStartFolder);
    connect(m_useLastFolder, &QCheckBox::toggled, this, &SettingsDialog::updateStartFolderEnabled);
    connect(m_thumbSize, &QSlider::valueChanged, this,
            [this](int px) { m_thumbValue->setText(tr("%1 px").arg(px)); });
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    load();
}

void SettingsDialog::load()
{
    m_useLastFolder->setChecked(AppSettings::useLastFolder());
    m_startFolder->setText(AppSettings::startFolder());
    m_thumbSize->setValue(AppSettings::defaultThumbSize());
    m_thumbValue->setText(tr("%1 px").arg(m_thumbSize->value()));
    updateStartFolderEnabled();
}

void SettingsDialog::updateStartFolderEnabled()
{
    // When starting from the last folder, the fixed path is ignored.
    m_startFolder->setEnabled(!m_useLastFolder->isChecked());
}

void SettingsDialog::browseForStartFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Choose Start Folder"), m_startFolder->text());
    if (!dir.isEmpty())
        m_startFolder->setText(dir);
}

void SettingsDialog::save()
{
    AppSettings::setUseLastFolder(m_useLastFolder->isChecked());
    AppSettings::setStartFolder(m_startFolder->text());
    AppSettings::setDefaultThumbSize(m_thumbSize->value());
    accept();
}
