#include "OSSIAControl.hpp"
#include "DocumentPlugin/OSSIADocumentPlugin.hpp"
#include "DocumentPlugin/OSSIABaseScenarioElement.hpp"

#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <core/document/Document.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Control/ScenarioControl.hpp>

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

#include <Network/Protocol/OSC.h>
#if defined(__APPLE__) && defined(ISCORE_DEPLOYMENT_BUILD)
#include <TTFoundationAPI.h>
#include <TTModular.h>
#endif

#include <core/document/DocumentModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <core/application/Application.hpp>

OSSIAControl::OSSIAControl(iscore::Application& app):
    iscore::PluginControlInterface {app, "OSSIAControl", nullptr}
{
// Here we try to load the extensions first because of buggy behaviour in TTExtensionLoader and API.
#if defined(__APPLE__) && defined(ISCORE_DEPLOYMENT_BUILD)
    auto contents = QFileInfo(qApp->applicationDirPath()).dir().path() + "/Frameworks/jamoma/extensions";
    TTFoundationInit(contents.toUtf8().constData(), true);
    TTModularInit(contents.toUtf8().constData(), true);
#endif
    using namespace OSSIA;
    auto localDevice = OSSIA::Local::create();
    m_localDevice = Device::create(localDevice, "i-score");

    setupOSSIACallbacks();

    // Two parts :
    // One that maintains the devices for each document
    // (and disconnects / reconnects them when the current document changes)
    // Also during execution, one shouldn't be able to switch document.

    // Another part that, at execution time, creates structures corresponding
    // to the Scenario plug-in with the OSSIA API.

    auto ctrl = ScenarioControl::instance();
    auto acts = ctrl->actions();
    for(const auto& act : acts)
    {
        if(act->objectName() == "Play")
        {
            connect(act, &QAction::toggled,
                    this, &OSSIAControl::on_play);
        }
        else if(act->objectName() == "Stop")
        {
            connect(act, &QAction::triggered,
                    this, &OSSIAControl::on_stop);
        }
    }

    con(iscore::Application::instance(), &iscore::Application::autoplay,
        this, [&] () { on_play(true); });
}

OSSIAControl::~OSSIAControl()
{
    // TODO doesn't handle the case where
    // two scenarios are playing in two ducments (we have to
    // stop them both)

    // TODO check the deletion order.
    // Maybe we should have a dependency graph of some kind ??
    if(currentDocument()
    && currentDocument()->model().pluginModel<OSSIADocumentPlugin>()
    && currentDocument()->model().pluginModel<OSSIADocumentPlugin>()->baseScenario())
    {
        baseConstraint().stop();
    }
}


OSSIAConstraintElement &OSSIAControl::baseConstraint() const
{
    return *currentDocument()->model().pluginModel<OSSIADocumentPlugin>()->baseScenario()->baseConstraint();
}

OSSIABaseScenarioElement&OSSIAControl::baseScenario() const
{
    return *currentDocument()->model().pluginModel<OSSIADocumentPlugin>()->baseScenario();
}

void OSSIAControl::populateMenus(iscore::MenubarManager* menu)
{
}

iscore::DocumentDelegatePluginModel*OSSIAControl::loadDocumentPlugin(
        const QString& name,
        const VisitorVariant& var,
        iscore::Document* model)
{
    // We don't have anything to load; it's easier to just recreate.
    return nullptr;
}

void OSSIAControl::on_newDocument(iscore::Document* doc)
{
    doc->model().addPluginModel(new OSSIADocumentPlugin{*doc, &doc->model()});
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

void OSSIAControl::on_play(bool b)
{
    if(auto doc = currentDocument())
    {
        auto& cstr = *doc->model().pluginModel<OSSIADocumentPlugin>()->baseScenario()->baseConstraint();
        if(b)
        {
            if(m_playing)
                cstr.OSSIAConstraint()->resume();
            else
            {
                cstr.recreate();
                cstr.play();

                // Here we stop the listening when we start playing the scenario.
                // Get all the selected nodes
                auto explorer = try_deviceExplorerFromObject(*doc);
                // Disable listening for everything
                if(explorer)
                    m_savedListening = explorer->deviceModel().pauseListening();
            }

            m_playing = true;
        }
        else
        {
            cstr.OSSIAConstraint()->pause();
        }
    }
}

void OSSIAControl::on_stop()
{
    if(auto doc = currentDocument())
    {
        auto& cstr = baseConstraint();
        cstr.stop();
        cstr.clear();
        m_playing = false;

        // If we can we resume listening
        auto explorer = try_deviceExplorerFromObject(*doc);
        if(explorer)
            explorer->deviceModel().resumeListening(m_savedListening);
    }
}

void OSSIAControl::setupOSSIACallbacks()
{
    // TODO in settings allow to set-up the local device's tree. Or maybe just use the device explorer ??
    // TODO OSSIALocalDevice
    auto local_play_node = *(m_localDevice->emplace(m_localDevice->children().cend(), "play"));
    auto local_play_address = local_play_node->createAddress(OSSIA::Value::Type::BOOL);
    auto local_stop_node = *(m_localDevice->emplace(m_localDevice->children().cend(), "stop"));
    auto local_stop_address = local_stop_node->createAddress(OSSIA::Value::Type::IMPULSE);

    local_play_address->addCallback([&] (const OSSIA::Value* v) {
        if (v->getType() == OSSIA::Value::Type::BOOL)
        {
            on_play(static_cast<const OSSIA::Bool*>(v)->value);
        }
    });

    local_stop_address->addCallback([&] (const OSSIA::Value*) {
        on_stop();
    });

    auto remote_protocol = OSSIA::OSC::create("127.0.0.1", 9999, 6666);
    m_remoteDevice = OSSIA::Device::create(remote_protocol, "i-score-remote");
}

