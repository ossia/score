#pragma once
#include <vector>
#include <memory>
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
void make_metadata_node(
        ModelMetadata& metadata,
        OSSIA::Node& parent,
        std::vector<std::unique_ptr<BaseProperty>>& properties,
        QObject* context);
}
}
