#pragma once

#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <QAction>
#include <QList>

#include <vector>

class QObject;
namespace iscore {
class Application;
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
class UndoApplicationPlugin final : public iscore::GUIApplicationContextPlugin
{
    public:
        UndoApplicationPlugin(iscore::Application& app, QObject* parent);
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
