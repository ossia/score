#include "OSSIADocumentPlugin.hpp"
#include "OSSIAAutomationElement.hpp"

#include "OSSIAConstraintElement.hpp"
#include "OSSIAEventElement.hpp"
#include "OSSIATimeNodeElement.hpp"

#include "OSSIAScenarioElement.hpp"

#include <Process/ScenarioModel.hpp>
#include <core/document/DocumentModel.hpp>


OSSIADocumentPlugin::OSSIADocumentPlugin(iscore::DocumentModel* doc, QObject* parent):
    iscore::DocumentDelegatePluginModel{"OSSIADocumentPlugin", parent}
{
    auto scenarios = doc->findChildren<ScenarioModel*>("ScenarioModel");

    // TODO refactor this logic with NetworkDocumentPlugin.
    for(ScenarioModel* scenario: scenarios)
    {
        if(scenario->pluginModelList.canAdd(OSSIAScenarioElement::staticPluginId()))
            scenario->pluginModelList.add(
                        makeElementPlugin(scenario,
                                          OSSIAScenarioElement::staticPluginId(),
                                          scenario));
    }

}

QList<iscore::ElementPluginModelType> OSSIADocumentPlugin::elementPlugins() const
{
    return {OSSIAConstraintElement::staticPluginId(),
            OSSIAEventElement::staticPluginId(),
            OSSIATimeNodeElement::staticPluginId(),
            OSSIAScenarioElement::staticPluginId()};

}

iscore::ElementPluginModel* OSSIADocumentPlugin::makeElementPlugin(
        const QObject* element,
        iscore::ElementPluginModelType type,
        QObject* parent)
{
    switch(type)
    {
        case OSSIAScenarioElement::staticPluginId():
        {
            if(element->metaObject()->className() == QString{"ScenarioModel"})
            {
                auto plug = new OSSIAScenarioElement(static_cast<const ScenarioModel*>(element), parent);

                return plug;
            }
            break;
        }
        default:
            break;
    }


    return nullptr;
    /*
    if(element->metaObject()->className() == QString{"ConstraintModel"})
    {
        auto plug = new OSSIAConstraintElement(static_cast<const ConstraintModel*>(element), parent);

        return plug;
    }
    else if(element->metaObject()->className() == QString{"EventModel"})
    {
        auto plug = new OSSIAEventElement(static_cast<const EventModel*>(element), parent);

        return plug;
    }
    else if(element->metaObject()->className() == QString{"TimeNodeModel"})
    {
        auto plug = new OSSIATimeNodeElement(static_cast<const TimeNodeModel*>(element), parent);

        return plug;
    }
    */
}

iscore::ElementPluginModel* OSSIADocumentPlugin::loadElementPlugin(
        const QObject* element,
        const VisitorVariant&,
        QObject* parent)
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
}

iscore::ElementPluginModel* OSSIADocumentPlugin::cloneElementPlugin(
        const QObject* element,
        iscore::ElementPluginModel* source,
        QObject* parent)
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
}

QWidget* OSSIADocumentPlugin::makeElementPluginWidget(
        const iscore::ElementPluginModel*,
        QWidget* parent) const
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
}

void OSSIADocumentPlugin::serialize(
        const VisitorVariant&) const
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
}
