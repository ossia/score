#pragma once
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>

class Group;

// Goes into the constraints, events, etc.
class GroupMetadata : public iscore::ElementPluginModel
{
        Q_OBJECT

    public:
        static constexpr int staticPluginId() { return 1; }

        GroupMetadata(
                const QObject* element,
                id_type<Group> id,
                QObject* parent);
        GroupMetadata* clone(const QObject* element, QObject* parent) const override;

        template<typename DeserializerVisitor>
        GroupMetadata(const QObject* element,
                      DeserializerVisitor&& vis,
                      QObject* parent) :
            iscore::ElementPluginModel{parent},
            m_element{element}
        {
            vis.writeTo(*this);
        }

        int elementPluginId() const override;
        virtual void serialize(const VisitorVariant&) const override;

        const auto& group() const
        { return m_id; }

        QString elementName() const
        { return m_element->staticMetaObject.className(); }

        const QObject* element() const
        { return m_element; }

    signals:
        void groupChanged(id_type<Group>);

    public slots:
        void setGroup(const id_type<Group>& id);

    private:
        const QObject* const m_element;
        id_type<Group> m_id;
};
