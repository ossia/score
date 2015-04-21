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
    using namespace std;

    // Base group set-up
    auto baseGroup = new Group{"Default", id_type<Group>{0}, groupManager()};
    baseGroup->addClient(m_policy->session()->localClient().id());
    groupManager()->addGroup(baseGroup);

    // Create it for each constraint / event.
    auto constraints = doc->findChildren<ConstraintModel*>("ConstraintModel");
    for(ConstraintModel* constraint : constraints)
    {
        if(constraint->metadata.canAddMetadata(metadataName()))
            constraint->metadata.addPluginMetadata(this->makeMetadata("ConstraintModel"));
    }
    auto events = doc->findChildren<EventModel*>("EventModel");
    for(EventModel* event : events)
    {
        if(event->metadata.canAddMetadata(metadataName()))
            event->metadata.addPluginMetadata(this->makeMetadata("EventModel"));
    }
}

#include <QVBoxLayout>
#include <QLabel>
class GroupMetadataWidget : public QWidget
{
    public:
        GroupMetadataWidget(const GroupMetadata& groupmetadata)
        {
            this->setLayout(new QVBoxLayout);
            this->layout()->addWidget(new QLabel{QString::number(groupmetadata.id().val().get())});
        }
};

QWidget *NetworkDocumentPlugin::makeMetadataWidget(const iscore::ElementPluginModel *var) const
{
    return new GroupMetadataWidget(static_cast<const GroupMetadata&>(*var));
}

QJsonObject NetworkDocumentPlugin::toJson() const
{
    return {};
}

QByteArray NetworkDocumentPlugin::toByteArray() const
{
    return {};
}

iscore::ElementPluginModel* NetworkDocumentPlugin::makeMetadata(const QString &str) const
{
    if(str == "ConstraintModel" || str == "EventModel")
    {
        return new GroupMetadata{m_groups->groups()[0]->id()};
    }

    return nullptr;
}
