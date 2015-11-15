#pragma once
#include <core/application/ApplicationComponents.hpp>

namespace iscore
{
struct ApplicationContext
{
        ApplicationContext(iscore::ApplicationComponents& c):
            components{c}
        {

        }

        iscore::ApplicationComponents& components;
};
}
