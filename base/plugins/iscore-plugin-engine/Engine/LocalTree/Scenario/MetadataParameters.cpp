#include "MetadataParameters.hpp"
#include <ossia/editor/state/state_element.hpp>
#include <Engine/LocalTree/GetProperty.hpp>
#include <Engine/LocalTree/Property.hpp>
#include <iscore/model/ModelMetadata.hpp>
namespace Engine
{
namespace LocalTree
{
ISCORE_PLUGIN_ENGINE_EXPORT
void make_metadata_node(
    iscore::ModelMetadata& metadata,
    ossia::net::node_base& parent,
    std::vector<std::unique_ptr<BaseProperty>>& properties,
    QObject* context)
{

  properties.push_back(add_getProperty<QString>(
      parent, "name", &metadata, &iscore::ModelMetadata::getName,
      &iscore::ModelMetadata::NameChanged, context));

  properties.push_back(add_property<QString>(
      parent, "comment", &metadata, &iscore::ModelMetadata::getComment,
      &iscore::ModelMetadata::setComment,
      &iscore::ModelMetadata::CommentChanged, context));

  properties.push_back(add_property<QString>(
      parent, "label", &metadata, &iscore::ModelMetadata::getLabel,
      &iscore::ModelMetadata::setLabel, &iscore::ModelMetadata::LabelChanged,
      context));
}
}
}
