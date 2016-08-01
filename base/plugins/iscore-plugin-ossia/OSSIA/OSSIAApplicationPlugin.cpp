#include "OSSIAApplicationPlugin.hpp"

#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <OSSIA/Executor/BaseScenarioElement.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>

#include <Process/TimeValue.hpp>
#include <OSSIA/Executor/ConstraintElement.hpp>
#include <OSSIA/Executor/StateElement.hpp>

#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/tools/Todo.hpp>
#include <Scenario/Application/ScenarioActions.hpp>

#include <OSSIA/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <OSSIA/Executor/ContextMenu/PlayContextMenu.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <OSSIA/Executor/ClockManager/ClockManagerFactory.hpp>
#include <algorithm>
#include <vector>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>

#include <OSSIA/Executor/Settings/ExecutorModel.hpp>


#include <ossia/editor/scenario/time_constraint.hpp>
#include <ossia/editor/state/state_element.hpp>
#include <ossia/editor/value/value.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/address.hpp>
#include <ossia/network/base/node.hpp>

#include <QAction>
#include <QVariant>
#include <QVector>

OSSIAApplicationPlugin::OSSIAApplicationPlugin(
        const iscore::GUIApplicationContext& ctx):
    iscore::GUIApplicationContextPlugin {ctx},
    m_playActions{*this, ctx}
{
    // Two parts :
    // One that maintains the devices for each document
    // (and disconnects / reconnects them when the current document changes)
    // Also during execution, one shouldn't be able to switch document.

    // Another part that, at execution time, creates structures corresponding
    // to the Scenario plug-in with the OSSIA API.


    auto& play_action = ctx.actions.action<Actions::Play>();
    connect(play_action.action(), &QAction::triggered,
            this, [&] (bool b)
    {
        on_play(b);
    },
    Qt::QueuedConnection);

    auto& stop_action = ctx.actions.action<Actions::Stop>();
    connect(stop_action.action(), &QAction::triggered,
            this, &OSSIAApplicationPlugin::on_stop,
            Qt::QueuedConnection);

    auto& init_action = ctx.actions.action<Actions::Reinitialize>();
    connect(init_action.action(), &QAction::triggered,
            this, &OSSIAApplicationPlugin::on_init,
            Qt::QueuedConnection);

    auto& ctrl = ctx.components.applicationPlugin<Scenario::ScenarioApplicationPlugin>();
    con(ctrl.execution(), &Scenario::ScenarioExecution::playAtDate,
        this, [=,act=play_action.action()] (const TimeValue& t)
    {
        on_play(true, t);
        act->trigger();
    });

    m_playActions.setupContextMenu(ctrl.layerContextMenuRegistrar());
}

OSSIAApplicationPlugin::~OSSIAApplicationPlugin()
{
    // The scenarios playing should already have been stopped by virtue of
    // aboutToClose.
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
    doc->model().addPluginModel(new Ossia::LocalTree::DocumentPlugin{*doc, &doc->model()});
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
        auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(&doc->model().modelDelegate());
        if(!scenar)
            return;
        on_play(scenar->displayedElements.constraint(), b, t);
    }
}

void OSSIAApplicationPlugin::on_play(Scenario::ConstraintModel& cst, bool b, TimeValue t)
{
    auto doc = currentDocument();
    ISCORE_ASSERT(doc);

    auto plugmodel = doc->context().findPlugin<RecreateOnPlay::DocumentPlugin>();
    if(!plugmodel)
        return;

    if(b)
    {
        if(m_playing)
        {
            ISCORE_ASSERT(bool(m_clock));
            auto bs = plugmodel->baseScenario();
            auto& cstr = *bs->baseConstraint()->OSSIAConstraint();
            if(cstr.paused())
            {
                m_clock->resume();
                m_paused = false;
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

            plugmodel->reload(cst);

            m_clock = makeClock(plugmodel->context());

            connect(plugmodel->baseScenario(), &RecreateOnPlay::BaseScenarioElement::finished,
                    this, [=] () {
                // TODO change the action icon state
                on_stop();
            }, Qt::QueuedConnection);
            m_clock->play(t);
            m_paused = false;
        }

        m_playing = true;
    }
    else
    {
        if(m_clock)
        {
            m_clock->pause();
            m_paused = true;
        }
    }
}

void OSSIAApplicationPlugin::on_record(::TimeValue t)
{
    ISCORE_ASSERT(!m_playing);

    // TODO have a on_exit handler to properly stop the scenario.
    if(auto doc = currentDocument())
    {
        auto plugmodel = doc->context().findPlugin<RecreateOnPlay::DocumentPlugin>();
        if(!plugmodel)
            return;
        auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(&doc->model().modelDelegate());
        if(!scenar)
            return;

        // Listening isn't stopped here.
        plugmodel->reload(scenar->baseConstraint());
        m_clock = makeClock(plugmodel->context());
        m_clock->play(t);

        m_playing = true;
        m_paused = false;
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
            m_paused = false;

            m_clock->stop();
            m_clock.reset();
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

        auto state = iscore::convert::state(scenar->baseScenario().startState(), plugmodel->context());
        state.launch();

        // If we can we resume listening
        if(!context.documents.preparingNewDocument())
        {
            auto explorer = Explorer::try_deviceExplorerFromObject(*doc);
            if(explorer)
                explorer->deviceModel().listening().restore();
        }
    }
}

std::unique_ptr<RecreateOnPlay::ClockManager> OSSIAApplicationPlugin::makeClock(
        const RecreateOnPlay::Context& ctx)
{
    auto& s = context.settings<RecreateOnPlay::Settings::Model>();
  return s.makeClock(ctx);
}
