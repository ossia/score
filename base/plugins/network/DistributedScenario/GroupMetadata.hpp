#pragma once
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>

class Group;

// Goes into the constraints, events, etc.
class GroupMetadata : public iscore::ElementPluginModel
{
        Q_OBJECT

    id_type<Group> m_id;
    public:
        static constexpr const char* staticPluginName() { return "Network"; }

        GroupMetadata(id_type<Group> id);

        QString plugin() const override;

        virtual void serialize(SerializationIdentifier identifier,
                               void* data) const override;

        const auto& id() const
        { return m_id; }

    signals:
        void groupChanged();

    public slots:
        void setGroup(const id_type<Group>& id);
};
