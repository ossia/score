#pragma once
#include <score/model/Component.hpp>

namespace Execution
{
struct Context;
using Component = score::GenericComponent<const Context>;
}
