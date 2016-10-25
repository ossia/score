#pragma once
#include <core/application/ApplicationInterface.hpp>
#include <core/application/ApplicationSettings.hpp>

namespace iscore {
class Settings;
class View;
class Presenter;
}  // namespace iscore

class QApplication;

class TestApplication final :
        public QObject,
        public iscore::ApplicationInterface
{
    public:
        TestApplication(int& argc, char** argv);
        ~TestApplication();

        const iscore::ApplicationContext& context() const override;
        const iscore::ApplicationComponents& components() const override
        { return context().components; }

        int exec();

        // Base stuff.
        QApplication* m_app;
        std::unique_ptr<iscore::Settings> m_settings; // Global settings

        // MVP
        iscore::View* m_view {};
        iscore::Presenter* m_presenter {};

        iscore::ApplicationSettings m_applicationSettings;
};
