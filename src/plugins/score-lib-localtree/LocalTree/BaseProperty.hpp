#pragma once
#include <score_lib_localtree_export.h>

namespace ossia::net
{
class parameter_base;
}

namespace LocalTree
{
struct SCORE_LIB_LOCALTREE_EXPORT BaseProperty
{
  ossia::net::parameter_base& addr;
  explicit BaseProperty(ossia::net::parameter_base& a) : addr{a} { }
  virtual ~BaseProperty();
};
}
