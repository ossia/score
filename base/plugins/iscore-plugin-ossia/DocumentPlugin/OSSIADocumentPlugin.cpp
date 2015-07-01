#include "OSSIADocumentPlugin.hpp"
#include "OSSIAAutomationElement.hpp"

#include "OSSIAConstraintElement.hpp"
#include "OSSIAEventElement.hpp"
#include "OSSIATimeNodeElement.hpp"

#include "OSSIABaseScenarioElement.hpp"
#include "OSSIAScenarioElement.hpp"

#include "Document/BaseElement/BaseScenario.hpp"
#include <Process/ScenarioModel.hpp>
#include <core/document/DocumentModel.hpp>


OSSIADocumentPlugin::OSSIADocumentPlugin(iscore::DocumentModel* doc, QObject* parent):
    iscore::DocumentDelegatePluginModel{"OSSIADocumentPlugin", parent}
{
    auto scenarios = doc->findChildren<ScenarioModel*>("ScenarioModel");

    // TODO refactor this logic with NetworkDocumentPlugin.
    for(ScenarioModel* scenario: scenarios)
    {
        if(scenario->pluginModelList->canAdd(OSSIAScenarioElement::staticPluginId()))
            scenario->pluginModelList->add(
                        makeElementPlugin(scenario,
                                          OSSIAScenarioElement::staticPluginId(),
                                          scenario));
    }


    auto baseElement = doc->findChild<BaseScenario*>("BaseScenario");
    m_base = static_cast<OSSIABaseScenarioElement*>(makeElementPlugin(baseElement,
                               OSSIABaseScenarioElement::staticPluginId(),
                               baseElement));
    baseElement->pluginModelList.add(m_base);


}

OSSIABaseScenarioElement *OSSIADocumentPlugin::baseScenario() const
{
    return m_base;
}

QList<iscore::ElementPluginModelType> OSSIADocumentPlugin::elementPlugins() const
{
    return {/*OSSIAConstraintElement::staticPluginId(),
            OSSIAEventElement::staticPluginId(),
            OSSIATimeNodeElement::staticPluginId(),*/
            OSSIAScenarioElement::staticPluginId(),
            OSSIABaseScenarioElement::staticPluginId()};

}

iscore::ElementPluginModel* OSSIADocumentPlugin::makeElementPlugin(
        const QObject* element,
        iscore::ElementPluginModelType type,
        QObject* parent)
{
    // TODO find a way to switch on-off the automatic creation in ElementPluginModelList?
    // Or just don't present Event/Timenode/Constraint here in elementPlugins?
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
        case OSSIABaseScenarioElement::staticPluginId():
        {
            if(element->objectName() == QString{"BaseScenario"})
            {
                auto plug = new OSSIABaseScenarioElement(static_cast<const BaseScenario*>(element), parent);

                return plug;
            }
            break;
        }
        default:
            break;
    }


    return nullptr;
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
