#pragma once
#include "BaseProperty.hpp"

namespace Engine
{
namespace LocalTree
{
class ISCORE_PLUGIN_ENGINE_EXPORT BaseCallbackWrapper : public BaseProperty
{
public:
  using BaseProperty::BaseProperty;
  ~BaseCallbackWrapper()
  {
  }

  ossia::net::parameter_base::callback_index callbackIt{};
};
}
}
