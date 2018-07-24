#pragma once
namespace ossia::net
{
class parameter_base;
}

namespace Engine::LocalTree
{
struct BaseProperty
{
  ossia::net::parameter_base& addr;
  explicit BaseProperty(ossia::net::parameter_base& a): addr{a} { }
  virtual ~BaseProperty();
};
}
