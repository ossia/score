#pragma once
#include <boost/optional/optional.hpp>
#include <QObject>

#include <QString>

#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class Group;
struct VisitorVariant;

// Goes into the constraints, events, etc.
class GroupMetadata : public iscore::ElementPluginModel
{
        Q_OBJECT

    public:
        static constexpr iscore::ElementPluginModelType staticPluginId() { return 1; }

        GroupMetadata(
                const QObject* element,
                Id<Group> id,
                QObject* parent);
        ~GroupMetadata();

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
        void serialize(const VisitorVariant&) const override;

        const auto& group() const
        { return m_id; }

        QString elementName() const
        { return m_element->staticMetaObject.className(); }

        const QObject* element() const
        { return m_element; }

    signals:
        void groupChanged(Id<Group>);

    public slots:
        void setGroup(const Id<Group>& id);

    private:
        const QObject* const m_element;
        Id<Group> m_id;
};
