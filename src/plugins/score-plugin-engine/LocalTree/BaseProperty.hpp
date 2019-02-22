#pragma once
#include <score/tools/Todo.hpp>
namespace ossia::net
{
class parameter_base;
}

namespace LocalTree
{
struct BaseProperty
{
  ossia::net::parameter_base& addr;
  explicit BaseProperty(ossia::net::parameter_base& a) : addr{a}
  {
  }
  virtual ~BaseProperty();
};
}
