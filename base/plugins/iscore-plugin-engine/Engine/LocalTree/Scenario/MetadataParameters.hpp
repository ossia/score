#pragma once
#include <iscore_plugin_engine_export.h>
#include <memory>
#include <vector>
namespace iscore
{
class ModelMetadata;
}
namespace ossia
{
namespace net
{
class node_base;
}
}
class QObject;

namespace Engine
{
namespace LocalTree
{
class BaseProperty;

ISCORE_PLUGIN_ENGINE_EXPORT
void make_metadata_node(
    iscore::ModelMetadata& metadata,
    ossia::net::node_base& parent,
    std::vector<std::unique_ptr<BaseProperty>>& properties,
    QObject* context);
}
}
