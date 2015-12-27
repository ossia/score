#pragma once
#include <core/application/ApplicationInterface.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <iscore/tools/NamedObject.hpp>

namespace iscore {
class Settings;
class View;
class Presenter;
}  // namespace iscore

class QApplication;

class TestApplication final :
        public NamedObject,
        public iscore::ApplicationInterface
{
    public:
        TestApplication(int& argc, char** argv);
        ~TestApplication();

        const iscore::ApplicationContext& context() const override;

        int exec();

        // Base stuff.
        QApplication* m_app;
        std::unique_ptr<iscore::Settings> m_settings; // Global settings

        // MVP
        iscore::View* m_view {};
        iscore::Presenter* m_presenter {};

        iscore::ApplicationSettings m_applicationSettings;
};
