#include "AutomationExecutor.hpp"

#include <Automation/AutomationModel.hpp>
// Device stuff
#include <Plugin/PanelBase/DeviceExplorerPanelModel.hpp>
#include <Plugin/Panel/DeviceExplorerModel.hpp>
#include <Plugin/DeviceExplorerPlugin.hpp>
#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <DeviceExplorer/Node/Node.hpp>

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

AutomationExecutor::AutomationExecutor(AutomationModel& model):
    m_model{model}
{
    auto doc = iscore::IDocument::documentFromObject(model);
    auto plug = static_cast<DeviceDocumentPlugin*>(doc->model()->pluginModel("DeviceDocumentPlugin"));

//    auto addr_split = model.address().split("/");
//    m_dev = &plug->list().device(addr_split[1]);

    // Remove the device name
//    addr_split.removeFirst();
/*
    m_demodel = getNodeFromString(static_cast<DeviceExplorerPanelModel*>(doc->model()->panel("DeviceExplorerPanelModel"))->deviceExplorer()->rootNode(),
                                  QStringList(msg));
*/
//    addr_split.removeFirst();
//    m.address = addr_split.join("/").prepend("/");
}

void AutomationExecutor::start()
{

}


void AutomationExecutor::stop()
{

}

void AutomationExecutor::onTick(const TimeValue& time)
{
    //m.value = m_model.value(time) * (m_demodel->maxValue() - m_demodel->minValue()) + m_demodel->minValue();
    m.value = m_model.value(time) * (m_model.max() - m_model.min()) + m_model.min();
    m_dev->sendMessage(m);
}

