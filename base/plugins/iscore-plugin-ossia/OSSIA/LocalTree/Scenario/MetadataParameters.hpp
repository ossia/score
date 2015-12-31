#pragma once
#include <vector>
#include <memory>
#include <iscore_plugin_ossia_export.h>
class ModelMetadata;
namespace OSSIA
{
class Node;
}
class QObject;
class BaseProperty;

namespace OSSIA
{
namespace LocalTree
{
ISCORE_PLUGIN_OSSIA_EXPORT
void make_metadata_node(
        ModelMetadata& metadata,
        OSSIA::Node& parent,
        std::vector<std::unique_ptr<BaseProperty>>& properties,
        QObject* context);
}
}
