#pragma once

#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>

namespace iscore
{
    class UndoControl : public iscore::PluginControlInterface
    {
        public:
            UndoControl(QObject* parent);
            ~UndoControl();

            void populateMenus(MenubarManager*) override;
            void populateToolbars() override;


        protected:
            void on_presenterChanged() override;

        private slots:
            void on_documentChanged(Document*);

        private:
        // Connections to keep for the running document.
        QList<QMetaObject::Connection> m_connections;

        QAction* m_undoAction{new QAction{this}};
        QAction* m_redoAction{new QAction{this}};
    };
}
