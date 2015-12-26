#pragma once

#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <QAction>
#include <QList>

#include <vector>
#include <iscore_lib_base_export.h>

class QObject;
namespace iscore {
class Document;
class MenubarManager;
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
class ISCORE_LIB_BASE_EXPORT UndoApplicationPlugin final : public iscore::GUIApplicationContextPlugin
{
    public:
        UndoApplicationPlugin(const iscore::ApplicationContext& app, QObject* parent);
        ~UndoApplicationPlugin();

        void populateMenus(MenubarManager*) override;
        std::vector<OrderedToolbar> makeToolbars() override;

    private slots:
        void on_documentChanged(
                iscore::Document* olddoc,
                iscore::Document* newdoc) override;

    private:
        // Connections to keep for the running document.
        QList<QMetaObject::Connection> m_connections;

        QAction m_undoAction;
        QAction m_redoAction;
};
}
