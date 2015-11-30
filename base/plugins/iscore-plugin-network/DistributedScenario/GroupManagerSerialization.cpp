
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <sys/types.h>
#include <algorithm>
#include <vector>

#include "Group.hpp"
#include "GroupManager.hpp"

template <typename T> class Reader;
template <typename T> class Writer;
template <typename model> class IdentifiedObject;


template<>
void Visitor<Reader<DataStream>>::readFrom(const GroupManager& elt)
{
    readFrom(static_cast<const IdentifiedObject<GroupManager>&>(elt));
    const auto& groups = elt.groups();
    m_stream << (int32_t) groups.size();
    for(const auto& group : groups)
    {
        readFrom(*group);
    }

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(GroupManager& elt)
{
    int32_t size;
    m_stream >> size;
    for(auto i = size; i --> 0; )
    {
        elt.addGroup(new Group{*this, &elt});
    }
    checkDelimiter();
}


template<>
void Visitor<Reader<JSONObject>>::readFrom(const GroupManager& elt)
{
    readFrom(static_cast<const IdentifiedObject<GroupManager>&>(elt));
    m_obj["GroupList"] = toJsonArray(elt.groups());
}


template<>
void Visitor<Writer<JSONObject>>::writeTo(GroupManager& elt)
{
    auto arr = m_obj["GroupList"].toArray();
    for(const auto& json_vref : arr)
    {
        Deserializer<JSONObject> deserializer {json_vref.toObject()};
        elt.addGroup(new Group{deserializer, &elt});
    }
}
