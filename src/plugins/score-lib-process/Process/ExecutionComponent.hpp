#pragma once
#include <score/model/Component.hpp>

#include <ossia/detail/thread.hpp>

namespace Execution
{
struct Context;
using Component = score::GenericComponent<const Context>;
}
