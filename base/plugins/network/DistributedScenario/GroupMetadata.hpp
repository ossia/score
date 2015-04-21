#pragma once
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>

class Group;

// Goes into the constraints, events, etc.
class GroupMetadata : public iscore::ElementPluginModel
{
        friend QDataStream &operator<<(QDataStream &out, const GroupMetadata &myObj)
        {
            Serializer<DataStream> s{out.device()};
            s.readFrom(myObj.m_id);
            return out;
        }

        friend QDataStream &operator>>(QDataStream &in, GroupMetadata &myObj)
        {
            Deserializer<DataStream> s{in.device()};
            s.writeTo(myObj.m_id);
            return in;
        }

    id_type<Group> m_id;
    public:
        static constexpr const char* staticPluginName() { return "Network"; }

        GroupMetadata(id_type<Group> id):
            m_id{id}
        {

        }

        QString plugin() const override
        { return staticPluginName(); }

        const auto& id() const
        { return m_id; }

    signals:
        void groupChanged();

    public slots:
        void setGroup(const id_type<Group>& id)
        {
            m_id = id;
            emit groupChanged();
        }
};
