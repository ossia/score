#pragma once
#include <QObject>
#include <QAction>
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>

namespace iscore
{
/**
 * @brief The DocumentAction class
 *
 * This class is an action that
 * behaves according to the current document.
 *
 * It should reset itself to a default state
 * when there is a document switch,
 * and disable itself when there is no document.
 */
template<typename Control>
class DocumentAction : public QAction
{
    private:
        // 1 per document
        iscore::Document* m_doc{};
        Control* m_ctrl;
    public:
        DocumentAction(const QString& text, Control* parent):
            QAction{text, parent},
            m_ctrl{parent}
        {
            connect(parent->presenter(), &Presenter::currentDocumentChanged,
                    this, &DocumentAction::on_documentChanged);

        }

        void on_documentChanged(iscore::Document* doc)
        {
            m_doc = doc;
            setEnabled(m_doc);
            setChecked(false);
        }
};
}
