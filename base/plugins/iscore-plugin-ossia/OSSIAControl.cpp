#include "OSSIAControl.hpp"
#include "DocumentPlugin/OSSIADocumentPlugin.hpp"

#include <API/Headers/Network/Device.h>
#include <API/Headers/Network/Node.h>
#include <API/Headers/Network/Protocol.h>

#include <API/Headers/Editor/Expression.h>
#include <API/Headers/Editor/ExpressionNot.h>
#include <API/Headers/Editor/ExpressionAtom.h>
#include <API/Headers/Editor/ExpressionComposition.h>
#include <API/Headers/Editor/Value.h>

#include <core/document/DocumentModel.hpp>

OSSIAControl::OSSIAControl(iscore::Presenter* pres):
    iscore::PluginControlInterface {pres, "OSSIAControl", nullptr}
{
    using namespace OSSIA;
    Local localDevice;
    m_localDevice = Device::create(localDevice, "i-score");
    // Two parts :
    // One that maintains the devices for each document
    // (and disconnects / reconnects them when the current document changes)
    // Also during execution, one shouldn't be able to switch document.

    // Another part that, at execution time, creates structures corresponding
    // to the Scenario plug-in with the OSSIA API.
}


void OSSIAControl::populateMenus(iscore::MenubarManager*)
{
}

iscore::DocumentDelegatePluginModel*OSSIAControl::loadDocumentPlugin(
        const QString& name,
        const VisitorVariant& var,
        iscore::DocumentModel* parent)
{
    qDebug() << Q_FUNC_INFO << "TODO";
}

void OSSIAControl::on_newDocument(iscore::Document* doc)
{
    doc->model()->addPluginModel(new OSSIADocumentPlugin{doc->model(), doc->model()});
}

void OSSIAControl::on_documentChanged()
{
    qDebug() << Q_FUNC_INFO << "TODO";
}
