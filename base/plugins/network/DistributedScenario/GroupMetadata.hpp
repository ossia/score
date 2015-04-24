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

        GroupMetadata(id_type<Group> id, QObject* parent);
        GroupMetadata* clone(QObject* parent) const override;

        template<typename DeserializerVisitor>
        GroupMetadata(DeserializerVisitor&& vis, QObject* parent) :
            iscore::ElementPluginModel{parent}
        {
            vis.writeTo(*this);
        }


        QString plugin() const override;

        virtual void serialize(const VisitorVariant&) const override;

        const auto& id() const
        { return m_id; }

    signals:
        void groupChanged(id_type<Group>);

    public slots:
        void setGroup(const id_type<Group>& id);
};
