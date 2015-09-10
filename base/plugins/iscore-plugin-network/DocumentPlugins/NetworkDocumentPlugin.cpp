#include "NetworkDocumentPlugin.hpp"
#include "Repartition/session/Session.hpp"
#include "DistributedScenario/GroupManager.hpp"

#include "source/Document/Constraint/ConstraintModel.hpp"
#include "source/Document/Event/EventModel.hpp"
#include "source/Document/TimeNode/TimeNodeModel.hpp"

#include <core/document/DocumentModel.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>

#include "DistributedScenario/GroupMetadataWidget.hpp"

#include <iscore/serialization/VisitorCommon.hpp>

NetworkDocumentPlugin::NetworkDocumentPlugin(NetworkPluginPolicy *policy,
                                             iscore::DocumentModel *doc):
    iscore::DocumentDelegatePluginModel{"NetworkDocumentPlugin", doc},
    m_policy{policy},
    m_groups{new GroupManager{this}}
{
    m_policy->setParent(this);
    using namespace std;

    // Base group set-up
    auto baseGroup = new Group{"Default", Id<Group>{0}, groupManager()};
    baseGroup->addClient(m_policy->session()->localClient().id());
    groupManager()->addGroup(baseGroup);

    // Create it for each constraint / event.
    auto constraints = doc->findChildren<ConstraintModel*>("ConstraintModel");
    for(ConstraintModel* constraint : constraints)
    {
        for(const auto& plugid : elementPlugins())
        {
            if(constraint->pluginModelList.canAdd(plugid))
                constraint->pluginModelList.add(
                            makeElementPlugin(constraint,
                                              plugid,
                                              constraint));
        }
    }

    auto events = doc->modelDelegate()->findChildren<EventModel*>("EventModel");
    for(EventModel* event : events)
    {
        for(const auto& plugid : elementPlugins())
        {
            if(event->pluginModelList.canAdd(plugid))
            {
                event->pluginModelList.add(
                            makeElementPlugin(event,
                                              plugid,
                                              event));
            }
        }
    }

    // TODO here we have to instantiate Network "OSSIA" policies. OR does it go with GroupMetadata?
}

NetworkDocumentPlugin::NetworkDocumentPlugin(const VisitorVariant &loader,
                                             iscore::DocumentModel *doc):
    iscore::DocumentDelegatePluginModel{"NetworkDocumentPlugin", doc}
{
    deserialize_dyn(loader, *this);
}

void NetworkDocumentPlugin::serialize(const VisitorVariant & vis) const
{
    serialize_dyn(vis, *this);
}

void NetworkDocumentPlugin::setPolicy(NetworkPluginPolicy * pol)
{
    delete m_policy;
    m_policy = pol;

    emit sessionChanged();
}

QList<iscore::ElementPluginModelType> NetworkDocumentPlugin::elementPlugins() const
{
    return {GroupMetadata::staticPluginId()};
}

void NetworkDocumentPlugin::setupGroupPlugin(GroupMetadata* plug)
{
    connect(m_groups, &GroupManager::groupRemoved,
            plug, [=] (const Id<Group>& id)
    { if(plug->group() == id) plug->setGroup(m_groups->defaultGroup()); });
}



////// Methods relative to GroupMetadata //////
iscore::ElementPluginModel*
NetworkDocumentPlugin::makeElementPlugin(
        const QObject* element,
        iscore::ElementPluginModelType type,
        QObject* parent)
{
    switch(type)
    {
        case GroupMetadata::staticPluginId():
        {
            if((element->metaObject()->className() == QString{"ConstraintModel"})
            || (element->metaObject()->className() == QString{"EventModel"}))
            {
                auto plug = new GroupMetadata{element, m_groups->defaultGroup(), parent};

                setupGroupPlugin(plug);

                return plug;
            }

            break;
        }
    }

    return nullptr;
}

iscore::ElementPluginModel*
NetworkDocumentPlugin::loadElementPlugin(
        const QObject* element,
        const VisitorVariant& vis,
        QObject* parent)
{
    if(element->metaObject()->className() == QString{"ConstraintModel"}
    || element->metaObject()->className() == QString{"EventModel"})
    {
        auto plug = deserialize_dyn(vis, [&] (auto&& deserializer)
        { return new GroupMetadata{element, deserializer, parent}; });

        setupGroupPlugin(plug);

        return plug;
    }

    return nullptr;
}


iscore::ElementPluginModel *NetworkDocumentPlugin::cloneElementPlugin(
        const QObject* element,
        iscore::ElementPluginModel* elt,
        QObject *parent)
{
    if(elt->elementPluginId() == GroupMetadata::staticPluginId())
    {
        auto newelt = static_cast<GroupMetadata*>(elt)->clone(element, parent);
        setupGroupPlugin(newelt);
        return newelt;
    }

    return nullptr;
}


////// Method relative to GRoupMetadataWidget //////
QWidget *NetworkDocumentPlugin::makeElementPluginWidget(
        const iscore::ElementPluginModel *var,
        QWidget* widg) const
{
    return new GroupMetadataWidget{
                static_cast<const GroupMetadata&>(*var), m_groups, widg};
}

