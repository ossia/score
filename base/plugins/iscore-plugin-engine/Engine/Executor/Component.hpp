#pragma once
#include <iscore/model/Component.hpp>
#include <iscore_plugin_engine_export.h>
namespace Engine
{
namespace Execution
{
class Context;
using Component = iscore::GenericComponent<const Context>;
}
}
