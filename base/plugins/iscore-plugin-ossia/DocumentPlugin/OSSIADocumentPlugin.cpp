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
#include "../iscore-plugin-curve/Automation/AutomationModel.hpp"


OSSIADocumentPlugin::OSSIADocumentPlugin(iscore::DocumentModel* doc, QObject* parent):
    iscore::DocumentDelegatePluginModel{"OSSIADocumentPlugin", parent}
{
    auto baseElement = doc->findChild<BaseScenario*>("BaseScenario");
    m_base = new OSSIABaseScenarioElement(baseElement, baseElement);
    baseElement->pluginModelList.add(m_base);
}

OSSIABaseScenarioElement *OSSIADocumentPlugin::baseScenario() const
{
    return m_base;
}

QList<iscore::ElementPluginModelType> OSSIADocumentPlugin::elementPlugins() const
{
    // Note : these are the ones that are automatically added.
    // The constraint, timenode, timeevent are added by Scenario/BaseScenario.
    // TODO this should be apparent in the method / class names
    return {OSSIABaseScenarioElement::staticPluginId()};
}

iscore::ElementPluginModel* OSSIADocumentPlugin::makeElementPlugin(
        const QObject* element,
        iscore::ElementPluginModelType type,
        QObject* parent)
{
    return nullptr;
}

iscore::ElementPluginModel* OSSIADocumentPlugin::loadElementPlugin(
        const QObject* element,
        const VisitorVariant&,
        QObject* parent)
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
    return nullptr;
}

iscore::ElementPluginModel* OSSIADocumentPlugin::cloneElementPlugin(
        const QObject* element,
        iscore::ElementPluginModel* source,
        QObject* parent)
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
    return nullptr;
}

QWidget* OSSIADocumentPlugin::makeElementPluginWidget(
        const iscore::ElementPluginModel*,
        QWidget* parent) const
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
    return nullptr;
}

void OSSIADocumentPlugin::serialize(
        const VisitorVariant&) const
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
}
