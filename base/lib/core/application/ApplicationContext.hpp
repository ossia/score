#pragma once

namespace iscore
{
class ApplicationComponents;
class Application;
struct ApplicationContext
{
        ApplicationContext(iscore::Application& app);
        ApplicationContext(const ApplicationContext&) = default;
        ApplicationContext(ApplicationContext&&) = default;
        ApplicationContext& operator=(const ApplicationContext&) = default;

        iscore::Application& app;
        const iscore::ApplicationComponents& components;
};
}
