#include "MetadataParameters.hpp"
#include <Engine/LocalTree/GetProperty.hpp>
#include <Engine/LocalTree/Property.hpp>
#include <Process/ModelMetadata.hpp>
#include <ossia/editor/state/state_element.hpp>
namespace Engine
{
namespace LocalTree
{
ISCORE_PLUGIN_ENGINE_EXPORT
void make_metadata_node(
        ModelMetadata& metadata,
        ossia::net::node_base& parent,
        std::vector<std::unique_ptr<BaseProperty>>& properties,
        QObject* context)
{

    properties.push_back(
    add_getProperty<QString>(parent, "name", &metadata,
                             &ModelMetadata::getName,
                             &ModelMetadata::NameChanged,
                             context));

    properties.push_back(
    add_property<QString>(parent, "comment", &metadata,
                          &ModelMetadata::getComment,
                          &ModelMetadata::setComment,
                          &ModelMetadata::CommentChanged,
                          context));

    properties.push_back(
    add_property<QString>(parent, "label", &metadata,
                          &ModelMetadata::getLabel,
                          &ModelMetadata::setLabel,
                          &ModelMetadata::LabelChanged,
                          context));
}
}
}
