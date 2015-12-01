#pragma once
#include <iscore/application/ApplicationComponents.hpp>

namespace iscore
{
class ApplicationComponents;
class Application;
struct ApplicationContext
{
        explicit ApplicationContext(iscore::Application&);
        explicit ApplicationContext(iscore::Application&, const ApplicationComponents&);
        ApplicationContext(const ApplicationContext&) = default;
        ApplicationContext(ApplicationContext&&) = default;
        ApplicationContext& operator=(const ApplicationContext&) = default;

        iscore::Application& app;
        const iscore::ApplicationComponents& components;
};
}
