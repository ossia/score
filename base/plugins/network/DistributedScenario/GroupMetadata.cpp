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

void GroupMetadata::serialize(SerializationIdentifier identifier, void* data) const
{
    if(identifier == DataStream::type())
    {
        static_cast<Visitor<Reader<DataStream>>*>(data)->readFrom(*this);

    }
    else if(identifier == JSON::type())
    {
        static_cast<Visitor<Reader<JSON>>*>(data)->readFrom(*this);
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
