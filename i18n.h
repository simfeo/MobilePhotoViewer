#ifndef I18N_H
#define I18N_H

#include <QString>

#include <functional>
#include <utility>
#include <vector>

namespace I18n
{
    // Install the translators for a language ("" = follow the system locale).
    //
    // Safe to call at startup and again at runtime: any previously installed
    // translators are removed first. Qt then posts QEvent::LanguageChange to
    // every top-level widget (QWidget forwards it to its children), which is
    // what drives the live retranslation below.
    void apply(const QString& code);
}

// Collects "re-apply this text" closures so a widget can restore every
// translatable string on QEvent::LanguageChange without having to keep a member
// pointer for each action, menu, label and tooltip it creates.
//
//   m_i18n.add([this, act]{ act->setText(tr("Open File...")); });
//
// add() runs the closure immediately (setting the initial text), so a call site
// reads as a single "this text is translatable" statement rather than a set-now
// plus a register-for-later.
class Retranslator
{
public:
    template <typename F>
    void add(F&& fn)
    {
        fn();
        m_fns.push_back(std::forward<F>(fn));
    }

    void retranslate() const
    {
        for (const auto& fn : m_fns)
            fn();
    }

private:
    std::vector<std::function<void()>> m_fns;
};

#endif // I18N_H
