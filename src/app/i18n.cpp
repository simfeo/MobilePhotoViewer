#include "i18n.h"

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

namespace
{
// Owned here rather than parented to the app: apply() may run repeatedly and
// each call must retire the previous pair.
QTranslator* g_qtTranslator  = nullptr;
QTranslator* g_appTranslator = nullptr;

// Load into *slot and install, or leave *slot null when the locale has no
// catalogue (which is normal -- English is the source language, and Qt does not
// ship qtbase_* for every locale). Untranslated strings then fall back to the
// literals in the sources.
void reload(QTranslator** slot, const QLocale& locale, const QString& name, const QString& dir)
{
    auto* translator = new QTranslator;
    if (translator->load(locale, name, "_", dir))
    {
        QCoreApplication::installTranslator(translator);
        *slot = translator;
    }
    else
    {
        delete translator;
    }
}
}   // namespace

void I18n::apply(const QString& code)
{
    const QLocale locale = code.isEmpty() ? QLocale::system() : QLocale(code);

    for (QTranslator** slot : { &g_qtTranslator, &g_appTranslator })
    {
        if (*slot)
        {
            QCoreApplication::removeTranslator(*slot);
            delete *slot;
            *slot = nullptr;
        }
    }

    // Qt's own strings (dialog buttons, etc.), then ours (embedded by
    // qt_add_translations() under :/i18n).
    reload(&g_qtTranslator, locale, "qtbase", QLibraryInfo::path(QLibraryInfo::TranslationsPath));
    reload(&g_appTranslator, locale, "MobilePhotoViewer", ":/i18n");
}
