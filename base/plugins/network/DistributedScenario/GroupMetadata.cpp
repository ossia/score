#include "GroupMetadata.hpp"


GroupMetadata::GroupMetadata(id_type<Group> id):
    m_id{id}
{

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
    m_id = id;
    emit groupChanged();
}
