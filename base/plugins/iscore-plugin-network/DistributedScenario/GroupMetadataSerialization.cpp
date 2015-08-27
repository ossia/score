#include "GroupMetadata.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const GroupMetadata& elt)
{
    readFrom(elt.group());
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(GroupMetadata& elt)
{
    Id<Group> id;
    m_stream >> id;
    elt.setGroup(id);

    checkDelimiter();
}


template<>
void Visitor<Reader<JSONObject>>::readFrom(const GroupMetadata& elt)
{
    m_obj["Group"] = toJsonValue(elt.group());
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(GroupMetadata& elt)
{
    elt.setGroup(fromJsonValue<Id<Group>>(m_obj["Group"]));
}
