#pragma once
#include <core/application/ApplicationSettings.hpp>
#include <core/plugin/PluginManager.hpp>
#include <core/settings/Settings.hpp>
#include <iscore/tools/NamedObject.hpp>

#include <vector>
#include <memory>
#include <QApplication>

namespace iscore
{
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
    class Application final : public NamedObject
    {
            Q_OBJECT
            friend class ChildEventFilter;
        public:
            // Returns the direct child of qApp.
            static Application& instance();

            Application(
                    int& argc,
                    char** argv);

            Application(
                    const ApplicationSettings& appSettings,
                    int& argc,
                    char** argv);

            Application(const Application&) = delete;
            Application& operator= (const Application&) = delete;
            ~Application();

            int exec()
            { return m_app->exec(); }

            Presenter& presenter() const
            { return *m_presenter; }

            View* view() const
            { return m_view; }

            Settings* settings() const
            { return m_settings.get(); }

        signals:
            void autoplay();

        private:
            void init(); // m_applicationSettings has to be set.
            void loadPluginData();

            // Base stuff.
            QApplication* m_app;
            std::unique_ptr<Settings> m_settings; // Global settings

            // MVP
            View* m_view {};
            Presenter* m_presenter {};

            // Data
            PluginLoader m_pluginManager {this};

            ApplicationSettings m_applicationSettings;


    };
}
