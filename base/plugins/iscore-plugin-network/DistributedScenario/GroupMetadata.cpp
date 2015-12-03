#include <iscore/serialization/VisitorCommon.hpp>

#include "GroupMetadata.hpp"

struct VisitorVariant;

GroupMetadata::GroupMetadata(
        const QObject* element,
        Id<Group> id,
        QObject* parent):
    iscore::ElementPluginModel{parent},
    m_element{element},
    m_id{id}
{

}

GroupMetadata::~GroupMetadata()
{

}

GroupMetadata* GroupMetadata::clone(const QObject *element, QObject *parent) const
{
    auto grp = new GroupMetadata{element, this->group(), parent};
    return grp;
}

int GroupMetadata::elementPluginId() const
{
    return 1; // TODO put this in a header
}

void GroupMetadata::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

void GroupMetadata::setGroup(const Id<Group>& id)
{
    if(id != m_id)
    {
        m_id = id;
        emit groupChanged(id);
    }
}
