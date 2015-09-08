#include "OSSIAControl.hpp"
#include "DocumentPlugin/OSSIADocumentPlugin.hpp"
#include "DocumentPlugin/OSSIABaseScenarioElement.hpp"

#include <API/Headers/Network/Device.h>
#include <API/Headers/Network/Node.h>
#include <API/Headers/Network/Protocol/Local.h>

#include <API/Headers/Editor/TimeEvent.h>
#include <API/Headers/Editor/TimeConstraint.h>
#include <API/Headers/Editor/TimeProcess.h>
#include <API/Headers/Editor/Expression.h>
#include <API/Headers/Editor/ExpressionNot.h>
#include <API/Headers/Editor/ExpressionAtom.h>
#include <API/Headers/Editor/ExpressionComposition.h>
#include <API/Headers/Editor/Value.h>
#if defined(__APPLE__) && defined(ISCORE_DEPLOYMENT_BUILD)
#include <TTFoundationAPI.h>
#include <TTModular.h>
#endif

#include <core/document/DocumentModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

OSSIAControl::OSSIAControl(iscore::Presenter* pres):
    iscore::PluginControlInterface {pres, "OSSIAControl", nullptr}
{
// Here we try to load the extensions first because of buggy behaviour in TTExtensionLoader and API.
#if defined(__APPLE__) && defined(ISCORE_DEPLOYMENT_BUILD)
    auto contents = QFileInfo(qApp->applicationDirPath()).dir().path() + "/Frameworks/jamoma/extensions";
    TTFoundationInit(contents.toLatin1().constData(), true);
    TTModularInit(contents.toLatin1().constData(), true);
#endif
    using namespace OSSIA;
    auto localDevice = OSSIA::Local::create();
    m_localDevice = Device::create(localDevice, "i-score");
    // Two parts :
    // One that maintains the devices for each document
    // (and disconnects / reconnects them when the current document changes)
    // Also during execution, one shouldn't be able to switch document.

    // Another part that, at execution time, creates structures corresponding
    // to the Scenario plug-in with the OSSIA API.
}

OSSIAControl::~OSSIAControl()
{
    // TODO check the deletion order.
    // Maybe we should have a dependency graph of some kind ??
    baseConstraint().stop();
}


OSSIAConstraintElement &OSSIAControl::baseConstraint() const
{
    return *currentDocument()->model().pluginModel<OSSIADocumentPlugin>()->baseScenario()->baseConstraint();
}

void OSSIAControl::populateMenus(iscore::MenubarManager* menu)
{
    QAction* play = new QAction {tr("Play"), this};
    connect(play, &QAction::triggered,
            [&] ()
    { baseConstraint().constraint()->start(); });

    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::PlayMenu,
                                       play);

    QAction* pause = new QAction {tr("Pause"), this};
    connect(pause, &QAction::triggered,
            [&] ()
    { baseConstraint().constraint()->pause(); });

    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::PlayMenu,
                                       pause);

    QAction* resume = new QAction {tr("Resume"), this};
    connect(resume, &QAction::triggered,
            [&] ()
    { baseConstraint().constraint()->resume(); });

    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::PlayMenu,
                                       resume);

    QAction* stop = new QAction {tr("Stop"), this};
    connect(stop, &QAction::triggered,
            [&] ()
    { baseConstraint().stop(); });

    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::PlayMenu,
                                       stop);
}

iscore::DocumentDelegatePluginModel*OSSIAControl::loadDocumentPlugin(
        const QString& name,
        const VisitorVariant& var,
        iscore::DocumentModel* model)
{
    // We don't have anything to load; it's easier to just recreate.
    return nullptr;
}

void OSSIAControl::on_newDocument(iscore::Document* doc)
{
    doc->model().addPluginModel(new OSSIADocumentPlugin{doc->model(), &doc->model()});
}

void OSSIAControl::on_loadedDocument(iscore::Document *doc)
{
    if(auto plugmodel = doc->model().pluginModel<OSSIADocumentPlugin>())
    {
        plugmodel->reload(doc->model());
    }
    else
    {
        on_newDocument(doc);
    }
}

void OSSIAControl::on_documentChanged()
{
}

