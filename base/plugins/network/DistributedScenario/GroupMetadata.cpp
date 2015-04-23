#include "GroupMetadata.hpp"


GroupMetadata::GroupMetadata(id_type<Group> id, QObject* parent):
    iscore::ElementPluginModel{parent},
    m_id{id}
{

}

iscore::ElementPluginModel *GroupMetadata::clone(QObject *parent) const
{
    auto grp = new GroupMetadata{this->id(), parent};
    return grp;
}

QString GroupMetadata::plugin() const
{
    return staticPluginName();
}

void GroupMetadata::serialize(const VisitorVariant& vis) const
{
    if(vis.identifier == DataStream::type())
    {
        static_cast<DataStream::Reader*>(vis.visitor)->readFrom(*this);
        return;
    }
    else if(vis.identifier == JSON::type())
    {
        static_cast<JSON::Reader*>(vis.visitor)->readFrom(*this);
        return;
    }

    Q_ASSERT(false);
}

void GroupMetadata::setGroup(const id_type<Group>& id)
{
    if(id != m_id)
    {
        m_id = id;
        emit groupChanged();
    }
}
