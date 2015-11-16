#pragma once
#include <core/application/ApplicationComponents.hpp>

namespace iscore
{
class Application;
struct ApplicationContext
{
        ApplicationContext(iscore::ApplicationComponents& c):
            components{c}
        {

        }

        iscore::Application& app;
        iscore::ApplicationComponents& components;
};
}
