#pragma once
#include <ossia/network/base/parameter.hpp>

#include <LocalTree/BaseProperty.hpp>
#include <score_lib_localtree_export.h>

namespace LocalTree
{
class SCORE_LIB_LOCALTREE_EXPORT BaseCallbackWrapper : public BaseProperty
{
public:
  using BaseProperty::BaseProperty;
  ~BaseCallbackWrapper() override = default;

  ossia::net::parameter_base::callback_index callbackIt{};
};
}
