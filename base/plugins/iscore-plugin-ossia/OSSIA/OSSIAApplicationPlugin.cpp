#include <Editor/TimeConstraint.h>
#include <Editor/StateElement.h>
#include <Network/Device.h>
#include <Network/Protocol/Local.h>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Network/Protocol/OSC.h>
#include <OSSIA/Executor/BaseScenarioElement.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <QAction>
#include <QVariant>
#include <QVector>

#include "Editor/Value.h"

#include "Network/Address.h"
#include "Network/Node.h"
#include "OSSIAApplicationPlugin.hpp"
#include <Process/TimeValue.hpp>
#include <OSSIA/Executor/ConstraintElement.hpp>
#include <OSSIA/Executor/StateElement.hpp>
#include <Network/Protocol/Minuit.h>

#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/tools/Todo.hpp>

namespace iscore {
class MenubarManager;
}  // namespace iscore
struct VisitorVariant;
#if defined(__APPLE__) && defined(ISCORE_DEPLOYMENT_BUILD)
#include <TTFoundationAPI.h>
#include <TTModular.h>
#include <QFileInfo>
#include <QDir>
#endif
#include <OSSIA/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <OSSIA/Executor/ContextMenu/PlayContextMenu.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <algorithm>
#include <vector>

OSSIAApplicationPlugin::OSSIAApplicationPlugin(const iscore::ApplicationContext& ctx):
    iscore::GUIApplicationContextPlugin {ctx, "OSSIAApplicationPlugin", nullptr}
{
// Here we try to load the extensions first because of buggy behaviour in TTExtensionLoader and API.
#if defined(__APPLE__) && defined(ISCORE_DEPLOYMENT_BUILD)
    auto contents = QFileInfo(qApp->applicationDirPath()).dir().path() + "/Frameworks/jamoma/extensions";
    TTFoundationInit(contents.toUtf8().constData(), true);
    TTModularInit(contents.toUtf8().constData(), true);
#endif
    auto localDevice = OSSIA::Local::create();
    m_localDevice = OSSIA::Device::create(localDevice, "i-score");

    setupOSSIACallbacks();

    // Two parts :
    // One that maintains the devices for each document
    // (and disconnects / reconnects them when the current document changes)
    // Also during execution, one shouldn't be able to switch document.

    // Another part that, at execution time, creates structures corresponding
    // to the Scenario plug-in with the OSSIA API.

    auto& ctrl = ctx.components.applicationPlugin<Scenario::ScenarioApplicationPlugin>();
    auto acts = ctrl.actions();
    for(const auto& act : acts)
    {
        if(act->objectName() == "Play")
        {
            connect(act, &QAction::toggled,
                    this, [&] (bool b)
            { on_play(b); });
        }
        else if(act->objectName() == "Stop")
        {
            connect(act, &QAction::triggered,
                    this, &OSSIAApplicationPlugin::on_stop);
        }
        else if(act->objectName() == "StopAndInit")
        {
            connect(act, &QAction::triggered,
                    this, &OSSIAApplicationPlugin::on_init);
        }
    }
    auto playCM = new RecreateOnPlay::PlayContextMenu{&ctrl};
    ctrl.pluginActions().push_back(playCM);

    con(playCM->playFromHereAction(), &QAction::triggered,
            this, [=] ()
    {
        auto t = playCM->playFromHereAction().data().value<::TimeValue>();
        on_play(true, t);
    });

    con(ctrl, &Scenario::ScenarioApplicationPlugin::playAtDate,
        this, [=] (const TimeValue& t)
    {
        on_play(true, t);
    });
}

OSSIAApplicationPlugin::~OSSIAApplicationPlugin()
{
    // TODO doesn't handle the case where
    // two scenarios are playing in two ducments (we have to
    // stop them both)

    // TODO check the deletion order.
    // Maybe we should have a dependency graph of some kind ??
    if(auto doc = currentDocument())
    if(auto pm = doc->context().findPlugin<RecreateOnPlay::DocumentPlugin>())
    if(auto scenar = pm->baseScenario())
    if(auto cstr = scenar->baseConstraint())
    {
        cstr->stop();
    }

    auto& children = m_localDevice->children();
    while(!children.empty())
        m_localDevice->erase(children.end() - 1);

    OSSIA::CleanupProtocols();
}


RecreateOnPlay::ConstraintElement &OSSIAApplicationPlugin::baseConstraint() const
{
    return *currentDocument()->context().plugin<RecreateOnPlay::DocumentPlugin>().baseScenario()->baseConstraint();
}

void OSSIAApplicationPlugin::populateMenus(iscore::MenubarManager* menu)
{
}

bool OSSIAApplicationPlugin::handleStartup()
{
    if(!context.documents.documents().empty())
    {
        if(context.applicationSettings.autoplay)
        {
            // TODO what happens if we load multiple documents ?
            on_play(true);
            return true;
        }
    }

    return false;
}

void OSSIAApplicationPlugin::on_newDocument(iscore::Document* doc)
{
    doc->model().addPluginModel(new Ossia::LocalTree::DocumentPlugin{m_localDevice,*doc, &doc->model()});
    doc->model().addPluginModel(new RecreateOnPlay::DocumentPlugin{*doc, &doc->model()});
}

void OSSIAApplicationPlugin::on_loadedDocument(iscore::Document *doc)
{
    on_newDocument(doc);
}

void OSSIAApplicationPlugin::on_documentChanged(
        iscore::Document* olddoc,
        iscore::Document* newdoc)
{
    if(olddoc)
    {
        // Disable the local tree for this document by removing
        // the node temporarily
        /*
        auto& doc_plugin = olddoc->context().plugin<DeviceDocumentPlugin>();
        doc_plugin.setConnection(false);
        */
    }

    if(newdoc)
    {
        // Enable the local tree for this document.

        /*
        auto& doc_plugin = newdoc->context().plugin<DeviceDocumentPlugin>();
        doc_plugin.setConnection(true);
        */
    }
}

void OSSIAApplicationPlugin::on_play(bool b, ::TimeValue t)
{
    // TODO have a on_exit handler to properly stop the scenario.
    if(auto doc = currentDocument())
    {
        auto plugmodel = doc->context().findPlugin<RecreateOnPlay::DocumentPlugin>();
        if(!plugmodel)
            return;
        auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(&doc->model().modelDelegate());
        if(!scenar)
            return;

        if(b)
        {
            if(m_playing)
            {
                if(auto bs = plugmodel->baseScenario())
                {
                    auto& cstr = *bs->baseConstraint();
                    emit requestResume();
                    cstr.OSSIAConstraint()->resume();
                }
            }
            else
            {
                // Here we stop the listening when we start playing the scenario.
                // Get all the selected nodes
                auto explorer = Explorer::try_deviceExplorerFromObject(*doc);
                // Disable listening for everything
                if(explorer)
                    explorer->deviceModel().listening().stop();

                plugmodel->reload(scenar->baseScenario());
                auto& cstr = *plugmodel->baseScenario()->baseConstraint();

                emit requestPlay();
                cstr.play(t);
            }

            m_playing = true;
        }
        else
        {
            if(auto bs = plugmodel->baseScenario())
            {
                auto& cstr = *bs->baseConstraint();
                emit requestPause();
                cstr.OSSIAConstraint()->pause();
            }
        }
    }
}

void OSSIAApplicationPlugin::on_stop()
{
    if(auto doc = currentDocument())
    {
        auto plugmodel = doc->context().findPlugin<RecreateOnPlay::DocumentPlugin>();
        if(!plugmodel)
            return;

        if(plugmodel && plugmodel->baseScenario())
        {
            m_playing = false;
            auto& cstr = *plugmodel->baseScenario()->baseConstraint();

            emit requestStop();
            cstr.stop();

            plugmodel->clear();
        }

        // If we can we resume listening
        if(!context.documents.preparingNewDocument())
        {
            auto explorer = Explorer::try_deviceExplorerFromObject(*doc);
            if(explorer)
                explorer->deviceModel().listening().restore();
        }
    }
}

void OSSIAApplicationPlugin::on_init()
{
    if(auto doc = currentDocument())
    {
        auto plugmodel = doc->context().findPlugin<RecreateOnPlay::DocumentPlugin>();
        if(!plugmodel)
            return;

        auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(&doc->model().modelDelegate());
        if(!scenar)
            return;

        auto explorer = Explorer::try_deviceExplorerFromObject(*doc);
        // Disable listening for everything
        if(explorer)
            explorer->deviceModel().listening().stop();

        plugmodel->reload(scenar->baseScenario());

        auto& st = *plugmodel->baseScenario()->startState();
        st.OSSIAState()->launch();

        plugmodel->clear();

        // If we can we resume listening
        if(!context.documents.preparingNewDocument())
        {
            auto explorer = Explorer::try_deviceExplorerFromObject(*doc);
            if(explorer)
                explorer->deviceModel().listening().restore();
        }
    }
}

void OSSIAApplicationPlugin::setupOSSIACallbacks()
{
    if(!m_localDevice)
        return;
    auto& dev = *m_localDevice;
    auto& children = dev.children();
    {
        auto end = children.cend();
        auto local_play_node = *(m_localDevice->emplace(end, "play"));
        auto local_play_address = local_play_node->createAddress(OSSIA::Value::Type::BOOL);
        local_play_address->setValue(new OSSIA::Bool{false});
        local_play_address->addCallback([&] (const OSSIA::Value* v) {
            qDebug("play");
            if (v->getType() == OSSIA::Value::Type::BOOL)
            {
                on_play(static_cast<const OSSIA::Bool*>(v)->value);
            }
        });
    }
    {
        auto end = children.cend();
        auto local_stop_node = *(m_localDevice->emplace(end, "stop"));
        auto local_stop_address = local_stop_node->createAddress(OSSIA::Value::Type::IMPULSE);
        local_stop_address->setValue(new OSSIA::Impulse{});
        local_stop_address->addCallback([&] (const OSSIA::Value*) {
            qDebug("stop");
            on_stop();
        });

    }

    auto remote_protocol = OSSIA::Minuit::create("127.0.0.1", 9999, 6666);
    m_remoteDevice = OSSIA::Device::create(remote_protocol, "i-score-remote");
}

