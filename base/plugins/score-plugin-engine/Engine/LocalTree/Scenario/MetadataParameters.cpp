// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MetadataParameters.hpp"
#include <ossia/editor/state/state_element.hpp>
#include <Engine/LocalTree/GetProperty.hpp>
#include <Engine/LocalTree/Property.hpp>
#include <score/model/ModelMetadata.hpp>
namespace Engine
{
namespace LocalTree
{
SCORE_PLUGIN_ENGINE_EXPORT
void make_metadata_node(
    score::ModelMetadata& metadata,
    ossia::net::node_base& parent,
    std::vector<std::unique_ptr<BaseProperty>>& properties,
    QObject* context)
{

  properties.push_back(add_getProperty<QString>(
      parent, "name", &metadata, &score::ModelMetadata::getName,
      &score::ModelMetadata::NameChanged, context));

  properties.push_back(add_property<QString>(
      parent, "comment", &metadata, &score::ModelMetadata::getComment,
      &score::ModelMetadata::setComment,
      &score::ModelMetadata::CommentChanged, context));

  properties.push_back(add_property<QString>(
      parent, "label", &metadata, &score::ModelMetadata::getLabel,
      &score::ModelMetadata::setLabel, &score::ModelMetadata::LabelChanged,
      context));
}
}
}
