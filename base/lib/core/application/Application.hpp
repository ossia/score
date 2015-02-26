#pragma once
#include <core/plugin/PluginManager.hpp>
#include <core/settings/Settings.hpp>
#include <tools/NamedObject.hpp>

#include <vector>
#include <memory>
#include <QApplication>

namespace iscore
{
    class Model;
    class Presenter;
    class View;
    class ChildEventFilter;

    /**
     * @brief Application
     *
     * This class is the main object in i-score. It is the
     * parent of every other object created.
     * It does instantiate the rest of the software (MVP, settings, plugins).
     */
    class Application : public NamedObject
    {
            Q_OBJECT
            friend class ChildEventFilter;
        public:
            Application(int& argc, char** argv);
            Application(const Application&) = delete;
            Application& operator= (const Application&) = delete;
            ~Application();

            int exec()
            {
                return m_app->exec();
            }
            View* view()
            {
                return m_view;
            }
            Settings* settings()
            {
                return m_settings.get();
            }

        public slots:
            /**
             * @brief addAutoconnection
             *
             * Allows to add a connection at runtime.
             * When called with a new connection, the effect
             * will be retroactive : if previous objects can been
             * linked by the new connection, they will be.
             */
            void addAutoconnection(Autoconnect);

        private:
            void loadPluginData();

            void doConnections();
            void doConnections(QObject*);

            // Base stuff.
            QApplication* m_app;
            std::unique_ptr<Settings> m_settings; // Global settings

            // MVP
            Model* m_model {};
            View* m_view {};
            Presenter* m_presenter {};

            // Data
            PluginManager m_pluginManager {this};
    };
}
