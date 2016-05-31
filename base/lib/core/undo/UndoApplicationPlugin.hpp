#pragma once

#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <QAction>
#include <QList>

#include <vector>
#include <iscore_lib_base_export.h>

class QObject;
namespace iscore {
class Document;
struct OrderedToolbar;
}  // namespace iscore

namespace iscore
{
/**
 * @brief The UndoApplicationPlugin class
 *
 * Base class for the "fake" undo plugin,
 * which provides a undo panel.
 */
class ISCORE_LIB_BASE_EXPORT UndoApplicationPlugin final :
        public iscore::GUIApplicationContextPlugin
{
    public:
        UndoApplicationPlugin(const iscore::GUIApplicationContext& app);
        ~UndoApplicationPlugin();

    private:
        void on_documentChanged(
                iscore::Document* olddoc,
                iscore::Document* newdoc) override;

        GUIElements makeGUIElements() override;

        // Connections to keep for the running document.
        QList<QMetaObject::Connection> m_connections;

        QAction m_undoAction;
        QAction m_redoAction;
};
}
