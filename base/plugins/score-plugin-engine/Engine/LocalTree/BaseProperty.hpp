#pragma once
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/base/node.hpp>
#include <score_plugin_engine_export.h>

namespace Engine
{
namespace LocalTree
{
class SCORE_PLUGIN_ENGINE_EXPORT BaseProperty
{
public:
  ossia::net::node_base& node;
  ossia::net::parameter_base& addr;

  BaseProperty(ossia::net::node_base& n, ossia::net::parameter_base& a)
      : node{n}, addr{a}
  {
  }

  virtual ~BaseProperty();
};
}
}
