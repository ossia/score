#pragma once
#include <vector>
#include <memory>
#include <iscore_plugin_ossia_export.h>
class ModelMetadata;
namespace ossia
{
namespace net
{
class Node;
}
}
class QObject;

namespace Ossia
{
namespace LocalTree
{
class BaseProperty;

ISCORE_PLUGIN_OSSIA_EXPORT
void make_metadata_node(
        ModelMetadata& metadata,
        ossia::net::Node& parent,
        std::vector<std::unique_ptr<BaseProperty>>& properties,
        QObject* context);
}
}
