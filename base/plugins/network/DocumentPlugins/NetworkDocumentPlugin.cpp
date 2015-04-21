#include "NetworkDocumentPlugin.hpp"
#include "Repartition/session/Session.hpp"

#include "source/Document/Constraint/ConstraintModel.hpp"
#include "source/Document/Event/EventModel.hpp"
#include "source/Document/TimeNode/TimeNodeModel.hpp"


NetworkDocumentPlugin::NetworkDocumentPlugin(NetworkPluginPolicy *policy, iscore::Document *doc):
    iscore::DocumentDelegatePluginModel{"NetworkDocumentPlugin", doc->model()},
    m_policy{policy},
    m_groups{new GroupManager{this}}
{
    // Base group set-up
    auto baseGroup = new Group{"Default", id_type<Group>{0}, groupManager()};
    baseGroup->addClient(m_policy->session()->localClient().id());
    groupManager()->addGroup(baseGroup);

    // Setup of pre-existing scenario elements.
    auto constraints = doc->findChildren<ConstraintModel*>("ConstraintModel");
    for(ConstraintModel* constraint : constraints)
    {
        constraint->metadata.addPluginMetadata(this->makeMetadata("ConstraintModel"));
    }
    auto events = doc->findChildren<EventModel*>("EventModel");
    for(EventModel* event : events)
    {
        event->metadata.addPluginMetadata(this->makeMetadata("EventModel"));
    }
}

bool NetworkDocumentPlugin::canMakeMetadataWidget(const QVariant& var) const
{
    return var.canConvert<GroupMetadata>();
}

#include <QVBoxLayout>
#include <QLabel>
class GroupMetadataWidget : public QWidget
{
    public:
        GroupMetadataWidget(const GroupMetadata& groupmetadata)
        {
            this->setLayout(new QVBoxLayout);
            this->layout()->addWidget(new QLabel{QString::number(groupmetadata.id.val().get())});
        }
};

QWidget *NetworkDocumentPlugin::makeMetadataWidget(const QVariant& var) const
{
    return new GroupMetadataWidget(var.value<GroupMetadata>());
}

QJsonObject NetworkDocumentPlugin::toJson() const
{
    return {};
}

QByteArray NetworkDocumentPlugin::toByteArray() const
{
    return {};
}

bool NetworkDocumentPlugin::canMakeMetadata(const QString &str) const
{
    return str == "ConstraintModel" || str == "EventModel";
}

QVariant NetworkDocumentPlugin::makeMetadata(const QString &str) const
{
    if(str == "ConstraintModel" || str == "EventModel")
    {
        return QVariant::fromValue(GroupMetadata{m_groups->groups()[0]->id()});
    }

    Q_ASSERT(false);
}
