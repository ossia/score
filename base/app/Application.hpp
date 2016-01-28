#pragma once
#include <core/application/ApplicationInterface.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <core/plugin/PluginManager.hpp>
#include <iscore/tools/NamedObject.hpp>
#include <QApplication>
#include <memory>

namespace iscore {
class Settings;
}  // namespace iscore

namespace iscore
{
    class Presenter;
    class View;
}

/**
     * @brief Application
     *
     * This class is the main object in i-score. It is the
     * parent of every other object created.
     * It does instantiate the rest of the software (MVP, settings, plugins).
     */
class Application final :
        public NamedObject,
        public iscore::ApplicationInterface
{
        Q_OBJECT
        friend class ChildEventFilter;
    public:
        Application(
                int& argc,
                char** argv);

        Application(
                const iscore::ApplicationSettings& appSettings,
                int& argc,
                char** argv);

        Application(const Application&) = delete;
        Application& operator= (const Application&) = delete;
        ~Application();

        int exec()
        { return m_app->exec(); }

        iscore::Settings* settings() const
        { return m_settings.get(); }

        const iscore::ApplicationContext& context() const override;
        void init(); // m_applicationSettings has to be set.

    private:
        void initDocuments();
        void loadPluginData();

        // Base stuff.
        QApplication* m_app;
        std::unique_ptr<iscore::Settings> m_settings; // Global settings

        // MVP
        iscore::View* m_view {};
        iscore::Presenter* m_presenter {};

        iscore::ApplicationSettings m_applicationSettings;
};



