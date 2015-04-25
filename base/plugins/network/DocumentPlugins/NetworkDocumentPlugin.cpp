#include "NetworkDocumentPlugin.hpp"
#include "Repartition/session/Session.hpp"
#include "DistributedScenario/GroupManager.hpp"

#include "source/Document/Constraint/ConstraintModel.hpp"
#include "source/Document/Event/EventModel.hpp"
#include "source/Document/TimeNode/TimeNodeModel.hpp"

#include <core/document/DocumentModel.hpp>

#include "DistributedScenario/GroupMetadataWidget.hpp"


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
            constraint->pluginModelList().add(makeElementPlugin(constraint, &constraint->pluginModelList()));
    }
    auto events = doc->findChildren<EventModel*>("EventModel");
    for(EventModel* event : events)
    {
        if(event->pluginModelList().canAdd(metadataName()))
        {
            event->pluginModelList().add(makeElementPlugin(event, &event->pluginModelList()));
        }
    }
}

QWidget *NetworkDocumentPlugin::makeElementPluginWidget(
        const iscore::ElementPluginModel *var,
        QWidget* widg) const
{
    return new GroupMetadataWidget(static_cast<const GroupMetadata&>(*var), m_groups, widg);
}

void NetworkDocumentPlugin::setupGroupPlugin(GroupMetadata* plug)
{
    connect(m_groups, &GroupManager::groupRemoved,
            plug, [=] (const id_type<Group>& id)
    { if(plug->group() == id) plug->setGroup(m_groups->defaultGroup()); });
}

iscore::ElementPluginModel*
NetworkDocumentPlugin::makeElementPlugin(
        const QObject* element,
        QObject* parent)
{
    if((element->metaObject()->className() == QString{"ConstraintModel"})
    || (element->metaObject()->className() == QString{"EventModel"}))
    {
        auto plug = new GroupMetadata{element, m_groups->defaultGroup(), parent};

        setupGroupPlugin(plug);

        return plug;
    }

    return nullptr;
}

iscore::ElementPluginModel*
NetworkDocumentPlugin::makeElementPlugin(
        const QObject* element,
        const VisitorVariant& vis,
        QObject* parent)
{
    GroupMetadata* plug{};
    if(element->staticMetaObject.className() == QString{"ConstraintModel"}
    || element->staticMetaObject.className() == QString{"EventModel"})
    {
        switch(vis.identifier)
        {
            case DataStream::type():
                plug = new GroupMetadata{
                            element,
                            *static_cast<DataStream::Deserializer*>(vis.visitor),
                            parent};
                break;
            case JSON::type():
                plug = new GroupMetadata{
                            element,
                            *static_cast<JSON::Deserializer*>(vis.visitor),
                            parent};
                break;
        }

        if(plug)
            setupGroupPlugin(plug);
    }

    return plug;
}


iscore::ElementPluginModel *NetworkDocumentPlugin::cloneElementPlugin(
        const QObject* element,
        iscore::ElementPluginModel* elt,
        QObject *parent)
{
    if(elt->plugin() == GroupMetadata::staticPluginName())
    {
        auto newelt = static_cast<GroupMetadata*>(elt)->clone(element, parent);
        setupGroupPlugin(newelt);
        return newelt;
    }

    return nullptr;
}
