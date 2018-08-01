#pragma once
#include <LocalTree/BaseProperty.hpp>
#include <ossia/network/base/parameter.hpp>
#include <score_plugin_engine_export.h>

namespace Engine
{
namespace LocalTree
{
class SCORE_PLUGIN_ENGINE_EXPORT BaseCallbackWrapper
    : public BaseProperty
{
public:
  using BaseProperty::BaseProperty;
  ~BaseCallbackWrapper() override = default;

  ossia::net::parameter_base::callback_index callbackIt{};
};
}
}
