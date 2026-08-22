#include "locationbar.h"

#include <QStackedWidget>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QToolButton>
#include <QLabel>
#include <QDir>
#include <QEvent>
#include <QKeyEvent>

LocationBar::LocationBar(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_stack = new QStackedWidget(this);

    // Page 0: breadcrumb buttons (left-aligned, empty area on the right).
    m_crumbPage   = new QWidget;
    m_crumbLayout = new QHBoxLayout(m_crumbPage);
    m_crumbLayout->setContentsMargins(4, 0, 4, 0);
    m_crumbLayout->setSpacing(1);
    m_crumbPage->installEventFilter(this);   // click empty area -> edit mode

    // Page 1: editable path.
    m_edit = new QLineEdit;
    m_edit->setClearButtonEnabled(true);
    m_edit->setPlaceholderText(tr("Location"));
    m_edit->installEventFilter(this);   // focus-out / Esc -> back to crumbs
    connect(m_edit, &QLineEdit::returnPressed, this, [this]
            {
        const QString p = m_edit->text().trimmed();
        showCrumbs();
        if (!p.isEmpty()){
            emit navigate(p);
} });

    m_stack->addWidget(m_crumbPage);   // index 0
    m_stack->addWidget(m_edit);        // index 1

    QHBoxLayout* outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(m_stack);

    rebuildCrumbs();
}

void LocationBar::setPath(const QString& path)
{
    m_path = path;
    rebuildCrumbs();
    showCrumbs();
}

void LocationBar::rebuildCrumbs()
{
    // Drop the previous buttons/separators.
    QLayoutItem* item;
    while ((item = m_crumbLayout->takeAt(0)) != nullptr)
    {
        if (item->widget())
        {
            item->widget()->deleteLater();
        }
        delete item;
    }

    // Split "D:/a/b" into cumulative targets: "D:/", "D:/a", "D:/a/b".
    const QString     clean = QDir::fromNativeSeparators(QDir::cleanPath(m_path));
    const QStringList parts = clean.split('/', Qt::SkipEmptyParts);

    QString accum;
    for (int i = 0; i < parts.size(); ++i)
    {
        if (i == 0)
        {
            accum = parts.at(0) + '/';   // drive root, e.g. "D:/"
        }
        else
        {
            if (!accum.endsWith('/'))
            {
                accum += '/';
            }
            accum += parts.at(i);
        }
        const QString target = accum;

        if (i > 0)
        {
            m_crumbLayout->addWidget(new QLabel(QString(QChar(0x203A))));   // "›"
        }

        QToolButton* crumb = new QToolButton;
        crumb->setText(parts.at(i));
        crumb->setAutoRaise(true);
        crumb->setCursor(Qt::PointingHandCursor);
        connect(crumb, &QToolButton::clicked, this, [this, target]
                { emit navigate(target); });
        m_crumbLayout->addWidget(crumb);
    }

    m_crumbLayout->addStretch(1);   // clickable empty area -> edit mode
}

void LocationBar::enterEditMode()
{
    m_edit->setText(m_path);
    m_stack->setCurrentWidget(m_edit);
    m_edit->setFocus();
    m_edit->selectAll();
}

void LocationBar::showCrumbs()
{
    m_stack->setCurrentWidget(m_crumbPage);
}

void LocationBar::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        m_edit->setPlaceholderText(tr("Location"));
    }
    QWidget::changeEvent(event);
}

bool LocationBar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_crumbPage && event->type() == QEvent::MouseButtonPress)
    {
        enterEditMode();
        return true;
    }
    if (watched == m_edit)
    {
        if (event->type() == QEvent::FocusOut)
        {
            showCrumbs();   // clicked away: cancel editing
        }
        else if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape)
        {
            showCrumbs();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
