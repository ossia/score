#include "MetadataParameters.hpp"
#include <OSSIA/LocalTree/GetProperty.hpp>
#include <OSSIA/LocalTree/Property.hpp>
#include <Process/ModelMetadata.hpp>
namespace Ossia
{
namespace LocalTree
{
ISCORE_PLUGIN_OSSIA_EXPORT
void make_metadata_node(
        ModelMetadata& metadata,
        OSSIA::Node& parent,
        std::vector<std::unique_ptr<BaseProperty>>& properties,
        QObject* context)
{

    properties.push_back(
    add_getProperty<QString>(parent, "name", &metadata,
                             &ModelMetadata::name,
                             &ModelMetadata::nameChanged,
                             context));

    properties.push_back(
    add_property<QString>(parent, "comment", &metadata,
                          &ModelMetadata::comment,
                          &ModelMetadata::setComment,
                          &ModelMetadata::commentChanged,
                          context));

    properties.push_back(
    add_property<QString>(parent, "label", &metadata,
                          &ModelMetadata::label,
                          &ModelMetadata::setLabel,
                          &ModelMetadata::labelChanged,
                          context));
}
}
}
