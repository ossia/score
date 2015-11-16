#pragma once
#include <core/application/ApplicationComponents.hpp>

namespace iscore
{
class Application;
struct ApplicationContext
{
        ApplicationContext(iscore::Application& app);

        iscore::Application& app;
        const iscore::ApplicationComponents& components;
};
}
