// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ApplicationPlugin.hpp"
#include <Process/ApplicationPlugin.hpp>
#include <Process/Layer/LayerContextMenu.hpp>
#include <Process/LayerPresenter.hpp>
#include <Recording/Session/RecordMenus.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>
#include <Scenario/Process/ScenarioView.hpp>
#include <QMenu>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <Process/LiveTake.hpp>

#include <Curve/Settings/CurveSettingsModel.hpp>

#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Engine/ApplicationPlugin.hpp>
#include <LocalTree/LocalTreeDocumentPlugin.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>
#include <Recording/Record/RecordTools.hpp>
#include <Recording/Session/RecordSession.hpp>
#include <Recording/Settings/RecordingSettings.hpp>
#include <Recording/Takes/KeepTakes.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <score/actions/ActionManager.hpp>
#include <score/actions/Toolbar.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/widgets/HelpInteraction.hpp>
#include <score/widgets/SetIcons.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/document/Document.hpp>

#include <ossia-qt/invoke.hpp>
#include <ossia/detail/algorithms.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/parameter.hpp>

#include <QAction>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolBar>
#include <QApplication>
#include <QCursor>
#include <QToolTip>

namespace Recording
{
ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& ctx)
    : score::GUIApplicationPlugin{ctx}
{
  using namespace Scenario;
  // Since we have declared the dependency, we can assume
  // that ScenarioApplicationPlugin is instantiated already.
  auto& scenario_plugin = ctx.guiApplicationPlugin<ScenarioApplicationPlugin>();
  // The Record menu of a scenario's background: the same as the node view's
  // and the explorer's, the addresses going into a new box at the point
  if(ctx.applicationSettings.gui)
  {
    auto& procs = ctx.applicationPlugin<Process::ApplicationPlugin>();
    procs.layerContextMenuRegistrar()
        .menu<ContextMenus::ScenarioModelContextMenu>()
        .functions.push_back([this](
                                 QMenu& menu, QPoint, QPointF scenept,
                                 const Process::LayerContext& lctx) {
      auto* pres = dynamic_cast<Scenario::ScenarioPresenter*>(&lctx.presenter);
      if(!pres)
        return;
      auto& scenar = const_cast<Scenario::ProcessModel&>(pres->model());
      RecordMenuTarget target;
      target.scenario = &scenar;
      target.point = Scenario::ConvertToScenarioPoint(
          scenept, pres->zoomRatio(), pres->view().height());
      addRecordEntries(menu, *this, lctx.context, target);
    });
  }
  connect(
      &scenario_plugin.execution(), &ScenarioExecution::stopRecording, this,
      &ApplicationPlugin::stopSession);

  m_ossiaplug = &ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();

  // Arming: the toggles of the processes' headers, and what Capture keeps
  auto& arm = Process::RecordArm::instance();
  arm.enabled = true;
  connect(&arm, &Process::RecordArm::armedChanged, this, [this] {
    m_captureBlocked = nullptr;
    // What is kept follows what is armed
    if(auto* doc = context.currentDocument())
    {
      auto* d = &doc->document;
      stopCapture();
      startCapture(*d);
    }
  });

  m_captureWatch.setInterval(500);
  connect(&m_captureWatch, &QTimer::timeout, this, [this] {
    publishState();
    if(m_capture)
      return;
    for(auto* d : context.documents.documents())
      if(!Process::RecordArm::instance().armedIn(*d).empty())
        startCapture(*d);
  });

  m_blink.setInterval(400);
  connect(&m_blink, &QTimer::timeout, this, [this] {
    if(!m_recordAct)
      return;
    publishState();
    const bool lit = !m_session || !m_session->waiting() || !m_recordAct->isChecked();
    QSignalBlocker _{m_recordAct};
    m_recordAct->setChecked(m_session ? lit : false);
  });

  if(ctx.applicationSettings.gui)
  {
    auto& stop_action = ctx.actions.action<Actions::Stop>();
    m_stopAction = stop_action.action();
    connect(m_stopAction, &QAction::triggered, this, [this] { stopSession(); });
  }
}

ApplicationPlugin::~ApplicationPlugin() = default;

RecordSession* ApplicationPlugin::adopt(
    std::unique_ptr<RecordSession> session, const score::DocumentContext& ctx)
{
  if(session->sources() == 0)
    return nullptr;
  if(!session->start())
  {
    if(!session->error().isEmpty())
      tell(session->error());
    return nullptr;
  }

  m_session = std::move(session);
  m_sessionDocument = &ctx.document;
  showRecording(true);
  // Destroyed once it has placed its takes, out of its own signal
  connect(m_session.get(), &RecordSession::finished, this, [this] {
    QMetaObject::invokeMethod(this, [this] {
      if(m_session && !m_session->running())
      {
        report(m_sessionDocument, m_session->warnings());
        m_session.reset();
        m_sessionDocument = nullptr;
        showRecording(false);
      }
    }, Qt::QueuedConnection);
  });
  return m_session.get();
}

RecordSession* ApplicationPlugin::startSession(
    const score::DocumentContext& ctx, const std::vector<const Process::Port*>& ports,
    RecordOptions options)
{
  if(m_session)
    return nullptr;
  auto session = std::make_unique<RecordSession>(ctx, options);
  for(auto* p : ports)
    if(p)
      session->addSource(*p);
  return adopt(std::move(session), ctx);
}

RecordSession* ApplicationPlugin::recordAddresses(
    Scenario::ProcessModel& scenario, Scenario::Point pt,
    const std::vector<State::Address>& addresses, AddressPlacement placement,
    std::optional<RecordOptions> chosen)
{
  if(m_session)
  {
    tell(tr("A recording is already running"));
    return nullptr;
  }
  if(addresses.empty())
  {
    tell(tr("Select devices or addresses in the device explorer to record them"));
    return nullptr;
  }
  auto& ctx = score::IDocument::documentContext(scenario);
  const auto& curves = context.settings<Curve::Settings::Model>();

  RecordOptions options = chosen ? *chosen : defaultOptions();
  if(!chosen)
    options.start = RecordOptions::Start::FirstEvent;
  // From the first value, recorded from the point: playback stops, and starts
  // from there with the first value (when the settings say so; else when the
  // user plays). Otherwise it records while it plays.
  if(auto* exec = ctx.findPlugin<Execution::DocumentPlugin>();
     exec && exec->isPlaying() && options.start == RecordOptions::Start::FirstEvent)
  {
    m_ossiaplug->execution().request_stop();
    // What the stop reports is not for the session
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    if(exec->isPlaying())
      return nullptr;
  }

  auto session = std::make_unique<RecordSession>(ctx, options);
  for(const auto& addr : addresses)
    session->addAddress(addr);

  CurveOptions c;
  c.stepped = curves.getCurveMode() == Curve::Settings::Mode::Parameter;
  c.simplify = curves.getSimplify();
  c.simplificationRatio = curves.getSimplificationRatio();
  session->placeInNewBox(scenario, pt, placement, c, curves.getPlayWhileRecording());
  return adopt(std::move(session), ctx);
}

std::vector<State::Address>
ApplicationPlugin::selectedAddresses(const score::DocumentContext& ctx) const
{
  if(auto* explorer = Explorer::try_deviceExplorerFromContext(ctx))
    return addressesToRecord(*explorer);
  return {};
}

RecordSession* ApplicationPlugin::recordAddressesInto(
    Scenario::IntervalModel& box, const std::vector<State::Address>& addresses,
    AddressPlacement placement, std::optional<QPointF> nodePosition,
    std::optional<RecordOptions> chosen)
{
  if(m_session)
    return nullptr;
  auto& ctx = score::IDocument::documentContext(box);
  const auto& curves = context.settings<Curve::Settings::Model>();

  RecordOptions options = chosen ? *chosen : defaultOptions();
  if(!chosen)
    options.start = RecordOptions::Start::Now;
  options.nodePosition = nodePosition;
  auto session = std::make_unique<RecordSession>(ctx, options);
  for(const auto& addr : addresses)
    session->addAddress(addr);

  CurveOptions c;
  c.stepped = curves.getCurveMode() == Curve::Settings::Mode::Parameter;
  c.simplify = curves.getSimplify();
  c.simplificationRatio = curves.getSimplificationRatio();
  session->placeInBox(box, placement, c);
  return adopt(std::move(session), ctx);
}

void ApplicationPlugin::stopSession()
{
  if(m_session)
    m_session->stop();
}

bool ApplicationPlugin::recording(const score::DocumentContext& ctx) const noexcept
{
  return m_session && m_session->running() && m_sessionDocument == &ctx.document;
}

RecordOptions ApplicationPlugin::defaultOptions() const
{
  if(auto* s = context.findSettings<Settings::Model>())
    return s->options();
  return RecordOptions{};
}

void ApplicationPlugin::publishState()
{
  for(auto& p : m_published)
  {
    const bool mine = m_session && m_sessionDocument == p.document && m_session->running();
    const bool waiting = mine && m_session->waiting();
    if(p.active && p.active->value() != ossia::value{mine})
      p.active->push_value(mine);
    if(p.waiting && p.waiting->value() != ossia::value{waiting})
      p.waiting->push_value(waiting);
    const bool capturing = m_capture && m_captureDocument == p.document;
    if(p.capturing && p.capturing->value() != ossia::value{capturing})
      p.capturing->push_value(capturing);
  }
}

void ApplicationPlugin::publish(score::Document& doc)
{
  auto* lt = doc.context().findPlugin<LocalTree::DocumentPlugin>();
  if(!lt)
    return;
  auto& root = lt->device().get_root_node();
  auto* rec = root.create_child("recording");
  if(!rec)
    return;
  const QPointer<score::Document> d = &doc;

  // Received on a network thread: done on the GUI thread, as the Record button
  {
    auto* n = rec->create_child("record");
    auto* p = n->create_parameter(ossia::val_type::IMPULSE);
    p->set_access(ossia::access_mode::SET);
    p->add_callback([this, d](const ossia::value&) {
      ossia::qt::run_async(this, [this, d] {
        if(d && !m_session)
          recordSelection(d->context());
      });
    });
  }
  {
    auto* n = rec->create_child("capture");
    auto* p = n->create_parameter(ossia::val_type::IMPULSE);
    p->set_access(ossia::access_mode::SET);
    p->add_callback([this](const ossia::value&) {
      ossia::qt::run_async(this, [this] { capture(); });
    });
  }
  {
    auto* n = rec->create_child("stop");
    auto* p = n->create_parameter(ossia::val_type::IMPULSE);
    p->set_access(ossia::access_mode::SET);
    p->add_callback([this](const ossia::value&) {
      ossia::qt::run_async(this, [this] { stopSession(); });
    });
  }
  Published pub;
  pub.document = &doc;
  {
    auto* n = rec->create_child("active");
    pub.active = n->create_parameter(ossia::val_type::BOOL);
    pub.active->set_value(false);
    pub.active->set_access(ossia::access_mode::GET);
  }
  {
    auto* n = rec->create_child("waiting");
    pub.waiting = n->create_parameter(ossia::val_type::BOOL);
    pub.waiting->set_value(false);
    pub.waiting->set_access(ossia::access_mode::GET);
  }
  {
    // Armed processes being kept for Capture
    auto* n = rec->create_child("capturing");
    pub.capturing = n->create_parameter(ossia::val_type::BOOL);
    pub.capturing->set_value(false);
    pub.capturing->set_access(ossia::access_mode::GET);
  }
  {
    auto* n = rec->create_child("message");
    pub.message = n->create_parameter(ossia::val_type::STRING);
    pub.message->set_value(std::string{});
    pub.message->set_access(ossia::access_mode::GET);
  }
  m_published.push_back(pub);
}

void ApplicationPlugin::on_createdDocument(score::Document& doc)
{
  publish(doc);
  watchPlayback(doc);
}

void ApplicationPlugin::watchPlayback(score::Document& doc)
{
  auto* exec = doc.context().findPlugin<Execution::DocumentPlugin>();
  if(!exec)
    return;
  const QPointer<score::Document> d = &doc;
  // Capture keeps what plays from the moment it plays
  connect(exec, &Execution::DocumentPlugin::started, this, [this, d] {
    QMetaObject::invokeMethod(this, [this, d] {
      if(d)
        startCapture(*d);
    }, Qt::QueuedConnection);
  });
  // Playback also starts again without saying so, e.g. when the audio engine
  // restarts: checked as long as something is armed
  if(!m_captureWatch.isActive())
    m_captureWatch.start();
}

void ApplicationPlugin::startCapture(const score::Document& doc)
{
  if(m_capture)
    return;
  auto* exec = doc.context().findPlugin<Execution::DocumentPlugin>();
  if(!exec || !exec->isPlaying())
  {
    // Tried again at the next playback
    if(m_captureBlocked == &doc)
      m_captureBlocked = nullptr;
    return;
  }
  // What is armed cannot be recorded in this playback: not again and again
  if(m_captureBlocked == &doc)
    return;
  const auto armed = Process::RecordArm::instance().armedIn(doc);
  if(armed.empty())
    return;
  auto opt = defaultOptions();
  opt.capture = true;
  opt.start = RecordOptions::Start::Now;
  opt.startRate = 1.; // whole bars
  opt.length = 0;
  opt.lengthBars = 0.;
  opt.countIn = false;
  opt.muteSource = false;
  opt.target = RecordOptions::Target::ThisBox;
  opt.passes = RecordOptions::Passes::LastOnly;
  opt.passBars = std::max(1, context.settings<Settings::Model>().getCaptureBars());
  auto session = std::make_unique<RecordSession>(doc.context(), opt);
  for(auto* proc : armed)
    for(auto* out : proc->outlets())
      if(out->type() == Process::PortType::Audio || out->type() == Process::PortType::Midi)
        session->addSource(*out);
  if(session->sources() == 0 || !session->start())
    return;
  m_capture = std::move(session);
  m_captureDocument = &doc;
  // Ended by Capture or by playback: done with, out of its own signal
  connect(m_capture.get(), &RecordSession::finished, this, [this] {
    QMetaObject::invokeMethod(this, [this] {
      if(m_capture && !m_capture->running())
      {
        report(m_captureDocument, m_capture->warnings());
        auto* doc = m_captureDocument;
        if(!m_capture->linkedAny())
          m_captureBlocked = doc;
        m_capture.reset();
        m_captureDocument = nullptr;
        // Still playing: what comes next is kept too
        for(auto* d : context.documents.documents())
          if(d == doc)
            startCapture(*d);
      }
    }, Qt::QueuedConnection);
  });
}

void ApplicationPlugin::stopCapture()
{
  if(!m_capture)
    return;
  // Ended without Capture: nothing is placed
  auto c = std::move(m_capture);
  m_captureDocument = nullptr;
  c->stop();
}

void ApplicationPlugin::capture()
{
  if(!m_capture)
  {
    tell(tr("Arm processes and play: Capture places the last bars they played"));
    return;
  }
  m_capture->capture();
}

void ApplicationPlugin::on_loadedDocument(score::Document& doc)
{
  watchPlayback(doc);
  if(ossia::none_of(m_published, [&](const Published& p) { return p.document == &doc; }))
    publish(doc);
}

void ApplicationPlugin::report(const score::Document* doc, const QStringList& warnings)
{
  if(warnings.isEmpty())
    return;
  for(const auto& w : warnings)
    qWarning() << "Recording:" << w;
  const QString text = warnings.join('\n');
  tell(text);
  for(auto& p : m_published)
    if(p.document == doc && p.message)
      p.message->push_value(text.toStdString());
}

void ApplicationPlugin::tell(const QString& why) const
{
  if(context.applicationSettings.gui)
    QToolTip::showText(QCursor::pos(), why);
}

void ApplicationPlugin::showRecording(bool b)
{
  publishState();
  // While the takes wait (for playback, the grid or a first value), the
  // button blinks; it stays lit while they record
  if(b && !m_blink.isActive())
    m_blink.start();
  else if(!b)
    m_blink.stop();
  if(m_recordAct && m_recordAct->isChecked() != b)
  {
    QSignalBlocker _{m_recordAct};
    m_recordAct->setChecked(b);
  }
}

score::GUIElements ApplicationPlugin::makeGUIElements()
{
  score::GUIElements e;
  auto bar = new QToolBar{tr("Recording")};
  m_recordAct = new QAction{tr("Record"), bar};
  m_recordAct->setCheckable(true);
  m_recordAct->setObjectName("RecordSelection");
  score::setHelp(
      m_recordAct,
      tr("Record the ports selected, or the audio and MIDI outputs of the processes "
         "selected, as the Recording settings say; again to stop."));
  setIcons(
      m_recordAct, QStringLiteral(":/icons/control_record_on.png"),
      QStringLiteral(":/icons/control_record_hover.png"),
      QStringLiteral(":/icons/control_record_off.png"),
      QStringLiteral(":/icons/control_record_disabled.png"));
  connect(m_recordAct, &QAction::triggered, this, [this] { recordSelection(); });
  bar->addAction(m_recordAct);
  m_captureAct = new QAction{tr("Capture"), bar};
  m_captureAct->setObjectName("Capture");
  score::setHelp(
      m_captureAct,
      tr("Capture: the last whole bars the armed processes played become loops in their "
         "boxes, as many as the Recording settings say."));
  setIcons(
      m_captureAct, QStringLiteral(":/icons/snapshot_on.png"),
      QStringLiteral(":/icons/snapshot_hover.png"),
      QStringLiteral(":/icons/snapshot_off.png"),
      QStringLiteral(":/icons/snapshot_disabled.png"));
  connect(m_captureAct, &QAction::triggered, this, [this] { capture(); });
  bar->addAction(m_captureAct);
  e.toolbars.emplace_back(
      bar, StringKey<score::Toolbar>("Recording"), Qt::BottomToolBarArea, 210);
  return e;
}

void ApplicationPlugin::recordSelection()
{
  if(auto* doc = context.currentDocument())
    recordSelection(*doc);
  else if(m_session)
    stopSession();
  else
    showRecording(false);
}

std::vector<const Process::Port*>
ApplicationPlugin::portsToRecord(const score::DocumentContext& ctx, bool* armed) const
{
  std::vector<const Process::Port*> ports;
  for(const auto& obj : ctx.selectionStack.currentSelection())
  {
    // A port selected along with its process is recorded once
    const auto add = [&](const Process::Port* port) {
      if(!ossia::contains(ports, port))
        ports.push_back(port);
    };
    if(auto* port = qobject_cast<const Process::Port*>(obj.data()))
    {
      add(port);
    }
    else if(auto* proc = qobject_cast<const Process::ProcessModel*>(obj.data()))
    {
      for(auto* out : proc->outlets())
        if(out->type() == Process::PortType::Audio || out->type() == Process::PortType::Midi)
          add(out);
    }
  }
  if(armed)
    *armed = ports.empty();
  // Nothing selected: the armed processes
  if(ports.empty())
    for(auto* proc : Process::RecordArm::instance().armedIn(ctx.document))
      for(auto* out : proc->outlets())
        if(out->type() == Process::PortType::Audio || out->type() == Process::PortType::Midi)
          ports.push_back(out);
  return ports;
}

void ApplicationPlugin::recordSelection(
    const score::DocumentContext& ctx, std::optional<QPointF> nodePosition,
    std::optional<RecordOptions> chosen)
{
  if(m_session)
  {
    stopSession();
    showRecording(m_session && m_session->running());
    return;
  }
  auto* doc = &ctx;
  const auto ports = portsToRecord(ctx);
  auto opt = chosen ? *chosen : defaultOptions();
  opt.nodePosition = nodePosition;
  if(ports.empty())
  {
    tell(tr("Select or arm ports or processes to record their outputs"));
    showRecording(false);
  }
  else if(!startSession(*doc, ports, opt))
  {
    tell(tr("Nothing in the selection can be recorded"));
    showRecording(false);
  }
}

void ApplicationPlugin::endSessionOf(score::Document& doc)
{
  if(!m_session || m_sessionDocument != &doc)
    return;
  m_session->stopNow();
  report(m_sessionDocument, m_session->warnings());
  m_session.reset();
  m_sessionDocument = nullptr;
  showRecording(false);
}

void ApplicationPlugin::on_documentSaveAs(score::Document& doc, const QString& newFileName)
{
  // The takes being written are finished and placed: they move with the rest
  endSessionOf(doc);
  keepTakesWithProject(doc, newFileName);
}

void ApplicationPlugin::on_documentCloseRequested(score::Document& doc)
{
  // The takes are placed before the user is asked whether to save
  endSessionOf(doc);
}

void ApplicationPlugin::on_closeDocument(score::Document& doc)
{
  ossia::remove_erase_if(m_published, [&](const Published& p) { return p.document == &doc; });
  if(m_capture && m_captureDocument == &doc)
    stopCapture();
  // Closed without being asked to: the session refers to the document
  endSessionOf(doc);
}
}
