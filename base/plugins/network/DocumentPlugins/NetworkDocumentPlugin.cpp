#include "NetworkDocumentPlugin.hpp"
#include "Repartition/session/Session.hpp"

#include "source/Document/Constraint/ConstraintModel.hpp"
#include "source/Document/Event/EventModel.hpp"
#include "source/Document/TimeNode/TimeNodeModel.hpp"

#include <core/document/DocumentModel.hpp>


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
        if(constraint->pluginModelList().canAdd(metadataName()))
            constraint->pluginModelList().add(makeElementPlugin("ConstraintModel", &constraint->pluginModelList()));
    }
    auto events = doc->findChildren<EventModel*>("EventModel");
    for(EventModel* event : events)
    {
        if(event->pluginModelList().canAdd(metadataName()))
            event->pluginModelList().add(makeElementPlugin("EventModel", &event->pluginModelList()));
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

QWidget *NetworkDocumentPlugin::makeElementPluginWidget(const iscore::ElementPluginModel *var) const
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

iscore::ElementPluginModel* NetworkDocumentPlugin::makeElementPlugin(const QString &str,
                                                                QObject* parent)
{
    if(str == "ConstraintModel" || str == "EventModel")
    {
        return new GroupMetadata{m_groups->groups()[0]->id(), parent};
        qDebug() << Q_FUNC_INFO << "todo connect";
    }

    return nullptr;
}

iscore::ElementPluginModel*NetworkDocumentPlugin::makeElementPlugin(const QString& str,
                                                               SerializationIdentifier identifier,
                                                               void* data,
                                                               QObject* parent)
{
    if(str == "ConstraintModel" || str == "EventModel")
    {
        if(identifier == DataStream::type())
        {
            return new GroupMetadata(*static_cast<Visitor<Writer<DataStream>>*>(data), parent);
        }
        else if(identifier == JSON::type())
        {
            return new GroupMetadata(*static_cast<Visitor<Writer<JSON>>*>(data), parent);
        }
    }
    return nullptr;
}


iscore::ElementPluginModel *NetworkDocumentPlugin::cloneElementPlugin(iscore::ElementPluginModel* elt, QObject *parent)
{
    if(elt->plugin() == GroupMetadata::staticPluginName())
    {
        qDebug() << Q_FUNC_INFO << "todo connect";
        return elt->clone(parent);
    }

    return nullptr;
}
