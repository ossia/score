#include "CalibrationWizard.hpp"

#include <State/Address.hpp>

#include <Process/Commands/EditPort.hpp>
#include <Process/Commands/SetControlValue.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionSetup.hpp>
#include <Process/ProcessList.hpp>

#include <Explorer/Commands/Update/UpdateDeviceSettings.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Cohesion/InterpolateMacro.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Engine/ApplicationPlugin.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Gfx/Window/CalibrationSet.hpp>
#include <Threedim/StructuredLight.hpp>

#include <score/actions/MenuManager.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/plugins/UuidKey.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

#include <cmath>

namespace Threedim
{
namespace
{
constexpr const char* slcUuid = "3f8c2a61-7d4e-4b19-9a05-c6e1d2b8f47a";
constexpr const char* blendUuid = "a0d6f1c8-2b47-4e93-8f15-7c3e9b2d6a41";

Process::ProcessModelFactory*
findFactory(const score::DocumentContext& ctx, const char* uuid)
{
  const auto key = UuidKey<Process::ProcessModel>{
      score::uuids::string_generator::compute(uuid, uuid + 36)};
  for(auto& f : ctx.app.interfaces<Process::ProcessFactoryList>())
    if(f.concreteKey() == key)
      return &f;
  return nullptr;
}

template <typename Ports>
auto findPort(const Ports& ports, const QString& name) -> typename Ports::value_type
{
  for(auto* p : ports)
    if(p->name() == name)
      return p;
  return nullptr;
}

ossia::value matched(const ossia::value& current, ossia::value v)
{
  if(!current.valid() || current.get_type() == v.get_type())
    return v;
  switch(current.get_type())
  {
    case ossia::val_type::INT:
    case ossia::val_type::FLOAT:
    case ossia::val_type::BOOL:
    case ossia::val_type::VEC2F:
    case ossia::val_type::VEC3F:
    case ossia::val_type::VEC4F:
    case ossia::val_type::STRING:
      return ossia::convert(v, current.get_type());
    default:
      return v;
  }
}

}

CalibrationWizard::CalibrationWizard(score::Document& doc, QWidget* parent)
    : QDialog{parent}
    , m_doc{&doc}
{
  setWindowTitle(tr("Projector calibration"));
  auto* lay = new QVBoxLayout{this};
  auto* form = new QFormLayout;
  lay->addLayout(form);

  m_device = new QComboBox;
  form->addRow(tr("Output device"), m_device);
  m_info = new QLabel;
  m_info->setWordWrap(true);
  form->addRow(m_info);

  m_camera = new QComboBox;
  m_camera->setEditable(true);
  m_camera->setToolTip(tr("Address of the camera texture, e.g. Cam:/ir for a depth camera, or Cam:/rgb aligned to depth for photometric calibration"));
  form->addRow(tr("Camera"), m_camera);
  m_positions = new QComboBox;
  m_positions->setEditable(true);
  m_positions->setToolTip(tr("Address of the depth camera's positions, e.g. Cam:/positions"));
  form->addRow(tr("Positions"), m_positions);
  m_mode = new QComboBox;
  m_mode->addItem(tr("Depth camera -> XYZ map"));
  m_mode->addItem(tr("Viewer camera -> direction map"));
  form->addRow(tr("Mode"), m_mode);

  m_name = new QLineEdit;
  {
    const auto docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    m_name->setText(QDir{docs}.filePath(QStringLiteral("calibration/room")));
  }
  m_name->setToolTip(
      tr("Output 1 is saved as name.json, output 2 as name_p1.json, ..."));
  auto* browse = new QPushButton{tr("...")};
  browse->setMaximumWidth(30);
  auto* nameRow = new QHBoxLayout;
  nameRow->addWidget(m_name, 1);
  nameRow->addWidget(browse);
  form->addRow(tr("Calibration name"), nameRow);
  connect(browse, &QPushButton::clicked, this, [this] {
    const auto f = QFileDialog::getSaveFileName(
        this, tr("Calibration name"), m_name->text(), tr("Calibrations (*.json)"));
    if(!f.isEmpty())
      m_name->setText(Gfx::calibrationBase(f));
  });

  m_settle = new QSpinBox;
  m_settle->setRange(0, 10000);
  m_settle->setValue(400);
  m_settle->setSuffix(tr(" ms"));
  form->addRow(tr("Settle"), m_settle);
  m_blend = new QCheckBox;
  m_blend->setChecked(true);
  m_blend->setToolTip(tr("Run Projector Blend Maps afterwards for the alpha and beta maps"));
  form->addRow(tr("Blend maps"), m_blend);
  m_photometric = new QCheckBox;
  m_photometric->setToolTip(
      tr("Also measure the projector response and the surface colour, with a colour "
         "camera (e.g. Cam:/rgb aligned to depth) whose exposure can be set"));
  form->addRow(tr("Photometric"), m_photometric);

  auto* buttons = new QHBoxLayout;
  m_setupBtn = new QPushButton{tr("Set up")};
  m_startBtn = new QPushButton{tr("Start")};
  m_abortBtn = new QPushButton{tr("Abort")};
  auto* refreshBtn = new QPushButton{tr("Refresh")};
  buttons->addWidget(refreshBtn);
  buttons->addWidget(m_setupBtn);
  buttons->addWidget(m_startBtn);
  buttons->addWidget(m_abortBtn);
  lay->addLayout(buttons);
  m_progress = new QProgressBar;
  m_progress->setRange(0, 1000);
  lay->addWidget(m_progress);
  m_log = new QPlainTextEdit;
  m_log->setReadOnly(true);
  m_log->setMinimumHeight(120);
  lay->addWidget(m_log);

  connect(refreshBtn, &QPushButton::clicked, this, &CalibrationWizard::refreshDevices);
  connect(m_setupBtn, &QPushButton::clicked, this, [this] { setUp(); });
  connect(m_startBtn, &QPushButton::clicked, this, &CalibrationWizard::start);
  connect(m_abortBtn, &QPushButton::clicked, this, &CalibrationWizard::abort);
  connect(m_device, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
    if(auto ws = deviceSettings(m_device->currentText()))
    {
      const auto p = planCalibration(*ws, m_name->text());
      QString s = tr("%1 projector(s) of %2x%3, %4 patterns each, canvas %5x%6")
                      .arg(p.projectors)
                      .arg(p.projectorSize.width())
                      .arg(p.projectorSize.height())
                      .arg(p.patternsPerProjector)
                      .arg(p.canvasSize.width())
                      .arg(p.canvasSize.height());
      for(const auto& w : p.warnings)
        s += QStringLiteral("\n- ") + w;
      m_info->setText(s);
    }
    else
      m_info->clear();
  });

  m_timer.setInterval(250);
  connect(&m_timer, &QTimer::timeout, this, &CalibrationWizard::poll);
  m_startBtn->setEnabled(false);
  m_abortBtn->setEnabled(false);
  refreshDevices();
}

CalibrationWizard::~CalibrationWizard() = default;

const score::DocumentContext* CalibrationWizard::context() const
{
  return m_doc ? &m_doc->context() : nullptr;
}

void CalibrationWizard::log(const QString& s)
{
  m_log->appendPlainText(s);
}

std::optional<Gfx::WindowSettings>
CalibrationWizard::deviceSettings(const QString& name) const
{
  auto* ctx = context();
  if(!ctx)
    return {};
  auto& plug = ctx->plugin<Explorer::DeviceDocumentPlugin>();
  if(auto* d = plug.list().findDevice(name))
  {
    const auto& s = d->settings();
    if(s.protocol == Gfx::WindowProtocolFactory::static_concreteKey()
       && s.deviceSpecificSettings.canConvert<Gfx::WindowSettings>())
      return s.deviceSpecificSettings.value<Gfx::WindowSettings>();
  }
  return {};
}

void CalibrationWizard::refreshDevices()
{
  auto* ctx = context();
  if(!ctx)
    return;
  const QString dev = m_device->currentText(), cam = m_camera->currentText(),
                pos = m_positions->currentText();
  m_device->clear();
  m_camera->clear();
  m_positions->clear();
  auto& plug = ctx->plugin<Explorer::DeviceDocumentPlugin>();
  for(auto* d : plug.list().devices())
  {
    const auto& s = d->settings();
    if(s.protocol == Gfx::WindowProtocolFactory::static_concreteKey())
    {
      const auto ws = s.deviceSpecificSettings.value<Gfx::WindowSettings>();
      if(ws.mode == Gfx::WindowMode::MultiWindow)
        m_device->addItem(s.name);
      continue;
    }
    bool depth = false;
    if(auto* od = d->getDevice())
    {
      for(const char* child : {"ir", "rgb"})
        if(ossia::net::find_node(od->get_root_node(), std::string("/") + child))
        {
          m_camera->addItem(s.name + ":/" + child);
          depth = true;
        }
      if(ossia::net::find_node(od->get_root_node(), "/positions"))
        m_positions->addItem(s.name + ":/positions");
    }
    if(!depth)
      m_camera->addItem(s.name + ":/");
  }
  if(!dev.isEmpty() && m_device->findText(dev) >= 0)
    m_device->setCurrentText(dev);
  if(!cam.isEmpty())
    m_camera->setCurrentText(cam);
  if(!pos.isEmpty())
    m_positions->setCurrentText(pos);
  if(m_device->count() == 0)
    m_info->setText(tr("No multi-window device: add a Window device in Multi-Window "
                       "Mapping mode with one output per projector."));
}

bool CalibrationWizard::updateDevice(const Gfx::WindowSettings& ws)
{
  auto* ctx = context();
  if(!ctx)
    return false;
  auto& plug = ctx->plugin<Explorer::DeviceDocumentPlugin>();
  auto* d = plug.list().findDevice(m_deviceName);
  if(!d)
    return false;
  auto s = d->settings();
  s.deviceSpecificSettings = QVariant::fromValue(ws);
  CommandDispatcher<> disp{ctx->commandStack};
  disp.submit(new Explorer::Command::UpdateDeviceSettings{plug, m_deviceName, s});
  return true;
}

bool CalibrationWizard::setUp()
{
  auto* ctx = context();
  if(!ctx)
    return false;
  m_deviceName = m_device->currentText();
  const auto ws = deviceSettings(m_deviceName);
  if(!ws)
  {
    log(tr("Pick a multi-window output device."));
    return false;
  }
  if(auto exec = ctx->findPlugin<Execution::DocumentPlugin>(); exec && exec->isPlaying())
  {
    log(tr("Stop the score first: inlet addresses set while playing are not picked up."));
    return false;
  }
  m_plan = planCalibration(*ws, m_name->text());
  if(m_plan.projectors == 0)
  {
    for(const auto& w : m_plan.warnings)
      log(w);
    return false;
  }
  auto* slcFactory = findFactory(*ctx, slcUuid);
  if(!slcFactory)
  {
    log(tr("Structured Light Calibration is not available."));
    return false;
  }
  QDir{}.mkpath(QFileInfo{m_plan.names[0]}.absolutePath());

  auto& root = score::IDocument::get<Scenario::ScenarioDocumentModel>(ctx->document);
  Scenario::Command::Macro m{
      new Scenario::Command::AddMultipleProcessesToMultipleIntervalsMacro, *ctx};
  m_slc = m.createProcessInNewSlot(root.baseInterval(), slcFactory->concreteKey(), {});
  if(!m_slc)
  {
    log(tr("Could not create Structured Light Calibration."));
    return false;
  }

  auto set = [&](Process::ProcessModel& p, const QString& name, ossia::value v) {
    if(auto* in = qobject_cast<Process::ControlInlet*>(findPort(p.inlets(), name)))
      m.setProperty<Process::ControlInlet::p_value>(*in, matched(in->value(), std::move(v)));
    else
      log(tr("no inlet %1 on %2").arg(name, p.metadata().getName()));
  };
  auto address = [&](Process::Port* port, const QString& addr) {
    if(!port || addr.isEmpty())
      return false;
    auto a = State::parseAddressAccessor(addr);
    if(!a)
    {
      log(tr("not an address: %1").arg(addr));
      return false;
    }
    m.setProperty<Process::Port::p_address>(*port, std::move(*a));
    return true;
  };

  const bool depth = m_mode->currentIndex() == 0;
  set(*m_slc, QStringLiteral("Projector size"),
      ossia::vec2f{float(m_plan.projectorSize.width()), float(m_plan.projectorSize.height())});
  set(*m_slc, QStringLiteral("Projector count"), m_plan.projectors);
  if(auto* mode = qobject_cast<Process::ControlInlet*>(
         findPort(m_slc->inlets(), QStringLiteral("Mode"))))
    m.setProperty<Process::ControlInlet::p_value>(
        *mode, mode->value().get_type() == ossia::val_type::STRING
                   ? ossia::value{m_mode->currentText().toStdString()}
                   : ossia::value{m_mode->currentIndex()});
  set(*m_slc, QStringLiteral("Settle (ms)"), m_settle->value());
  set(*m_slc, QStringLiteral("Save file"),
      QString(m_plan.names[0] + QStringLiteral(".json")).toStdString());

  auto* canvas = findPort(m_slc->outlets(), QStringLiteral("Canvas"));
  if(canvas)
  {
    set(*m_slc, QStringLiteral("Canvas layout"), m_plan.canvasLayout.toStdString());
    set(*m_slc, QStringLiteral("Canvas size"),
        ossia::vec2f{float(m_plan.canvasSize.width()), float(m_plan.canvasSize.height())});
    address(canvas, m_deviceName + QStringLiteral(":/"));
  }
  else if(m_plan.projectors == 1)
    address(findPort(m_slc->outlets(), QStringLiteral("Pattern")), m_deviceName + ":/");
  else
  {
    log(tr("This Structured Light Calibration has no Canvas outlet: one projector only."));
    return false;
  }
  if(!address(findPort(m_slc->inlets(), QStringLiteral("Camera")), m_camera->currentText()))
    log(tr("Set the camera address."));
  if(depth
     && !address(
         findPort(m_slc->inlets(), QStringLiteral("Positions")),
         m_positions->currentText()))
    log(tr("Depth mode without a Positions address: the XYZ map stays empty."));

  if(m_photometric->isChecked())
  {
    set(*m_slc, QStringLiteral("Response levels"), 8);
    set(*m_slc, QStringLiteral("Surface compensation"), true);
    const QString cam = m_camera->currentText();
    const QString dev = cam.section(':', 0, 0);
    if(cam.endsWith(QStringLiteral(":/ir")))
      log(tr("Photometric calibration needs a colour camera, e.g. %1:/rgb.").arg(dev));
    ossia::net::node_base* exposure{};
    ossia::net::node_base* autoExposure{};
    auto& plug = ctx->plugin<Explorer::DeviceDocumentPlugin>();
    if(auto* d = plug.list().findDevice(dev); d && d->getDevice())
    {
      auto& root = d->getDevice()->get_root_node();
      exposure = ossia::net::find_node(root, "/controls/color/exposure");
      autoExposure = ossia::net::find_node(root, "/controls/color/auto_exposure");
    }
    const float e = exposure && exposure->get_parameter()
                        ? ossia::convert<float>(exposure->get_parameter()->value())
                        : 0.f;
    if(e > 0.f)
    {
      QStringList bracket;
      for(float f : {0.125f, 0.25f, 0.5f, 1.f, 2.f, 4.f})
        bracket.push_back(QString::number(e * f));
      set(*m_slc, QStringLiteral("Exposures"), bracket.join(' ').toStdString());
      set(*m_slc, QStringLiteral("Capture exposure"), e);
      address(
          findPort(m_slc->outlets(), QStringLiteral("Camera exposure")),
          dev + QStringLiteral(":/controls/color/exposure"));
      if(autoExposure)
        address(
            findPort(m_slc->outlets(), QStringLiteral("Auto exposure")),
            dev + QStringLiteral(":/controls/color/auto_exposure"));
    }
    else
      log(tr("No colour exposure control on %1: the camera is assumed linear. Turn "
             "its auto exposure and auto white balance off.")
              .arg(dev));
  }

  m_blendProc = nullptr;
  if(m_blend->isChecked())
  {
    if(auto* bf = findFactory(*ctx, blendUuid))
    {
      m_blendProc = m.createProcessInNewSlot(root.baseInterval(), bf->concreteKey(), {});
      if(m_blendProc)
      {
        QStringList files;
        for(const auto& n : m_plan.names)
          files.push_back(n + QStringLiteral(".json"));
        set(*m_blendProc, QStringLiteral("Calibration files"),
            files.join(QStringLiteral("; ")).toStdString());
      }
    }
  }
  m.commit();

  m_original = *ws;
  updateDevice(neutralOutputs(*ws));

  log(tr("Set up %1 projector(s) on %2; the output stage of %2 is off until the end.")
          .arg(m_plan.projectors)
          .arg(m_deviceName));
  for(const auto& w : m_plan.warnings)
    log(QStringLiteral("- ") + w);
  m_phase = Phase::Ready;
  m_startBtn->setEnabled(true);
  m_abortBtn->setEnabled(true);
  return true;
}

bool CalibrationWizard::pushValue(
    Process::ProcessModel* p, const QString& inlet, ossia::value v)
{
  auto* ctx = context();
  if(!ctx || !p)
    return false;
  auto* in = findPort(p->inlets(), inlet);
  auto* plug = ctx->findPlugin<Execution::DocumentPlugin>();
  if(!in || !plug || !plug->isPlaying())
    return false;
  auto& exec = plug->context();
  auto it = exec.setup.inlets.find(in);
  if(it == exec.setup.inlets.end() || !it->second.first || !it->second.second
     || !it->second.second->target<ossia::value_port>())
    return false;
  std::weak_ptr<ossia::graph_node> node = it->second.first;
  exec.executionQueue.enqueue(
      [node = std::move(node), port = it->second.second, v = std::move(v)]() mutable {
    if(auto n = node.lock())
      port->target<ossia::value_port>()->write_value(std::move(v), 0);
  });
  return true;
}

void CalibrationWizard::start()
{
  auto* ctx = context();
  if(!ctx || !m_slc)
  {
    log(tr("Set up first."));
    return;
  }
  auto* exec = ctx->findPlugin<Execution::DocumentPlugin>();
  if(!exec || !exec->isPlaying())
  {
    if(auto* engine
       = score::GUIAppContext().findGuiApplicationPlugin<Engine::ApplicationPlugin>())
      engine->execution().request_play_global(true);
  }
  m_phase = Phase::Starting;
  m_startMs = QDateTime::currentMSecsSinceEpoch();
  const double frame = 1000. / 30.;
  m_estimateMs = m_plan.projectors * m_plan.patternsPerProjector
                     * (m_settle->value() + 2 * frame)
                 + 5000.;
  m_progress->setValue(0);
  m_timer.start();
  log(tr("Starting: about %1 s.").arg(int(m_estimateMs / 1000.)));
}

void CalibrationWizard::abort()
{
  m_timer.stop();
  pushValue(m_slc, QStringLiteral("Abort"), ossia::impulse{});
  restoreDevice();
  m_phase = Phase::Idle;
  m_startBtn->setEnabled(false);
  m_abortBtn->setEnabled(false);
  log(tr("Aborted; %1 is back to its settings.").arg(m_deviceName));
}

void CalibrationWizard::restoreDevice()
{
  if(m_original)
    updateDevice(*m_original);
  m_original.reset();
}

void CalibrationWizard::poll()
{
  if(!context() || !m_slc)
  {
    m_timer.stop();
    return;
  }
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  switch(m_phase)
  {
    case Phase::Starting:
      if(pushValue(m_slc, QStringLiteral("Start"), ossia::impulse{}))
      {
        m_phase = Phase::Capturing;
        m_startMs = now;
        log(tr("Projecting the patterns."));
      }
      else if(now - m_startMs > 10000)
      {
        m_timer.stop();
        log(tr("The score did not start playing."));
      }
      break;
    case Phase::Capturing: {
      m_progress->setValue(int(std::min(0.99, (now - m_startMs) / m_estimateMs) * 1000));
      if(now - m_lastSave > 1000)
      {
        pushValue(m_slc, QStringLiteral("Save"), ossia::impulse{});
        m_lastSave = now;
      }
      const auto r = readCalibrationResults(m_plan.names, m_startMs);
      if(r.saved == m_plan.projectors)
      {
        m_progress->setValue(1000);
        log(r.text);
        if(m_blendProc && pushValue(m_blendProc, QStringLiteral("Compute"), ossia::impulse{}))
        {
          m_phase = Phase::Blending;
          m_blendMs = now;
          log(tr("Computing the blend maps."));
        }
        else
          finish();
      }
      else if(now - m_startMs > 3 * m_estimateMs + 60000)
      {
        m_timer.stop();
        log(tr("No result: look at the Status of the Structured Light Calibration process "
               "(camera address, camera running, projector visible)."));
      }
      break;
    }
    case Phase::Blending: {
      int ready = 0;
      for(const auto& n : m_plan.names)
        if(QFileInfo fi{n + QStringLiteral(".alpha.pfm")};
           fi.isFile() && fi.lastModified().toMSecsSinceEpoch() + 1000 >= m_blendMs)
          ready++;
      if(ready == m_plan.projectors)
        finish();
      else if(now - m_blendMs > 120000)
      {
        log(tr("The blend maps were not written; assigning the calibration anyway."));
        finish();
      }
      break;
    }
    default:
      m_timer.stop();
      break;
  }
}

void CalibrationWizard::finish()
{
  m_timer.stop();
  if(m_original)
    updateDevice(assignCalibrations(*m_original, m_plan.names));
  m_original.reset();
  m_phase = Phase::Done;
  m_startBtn->setEnabled(false);
  m_abortBtn->setEnabled(false);
  log(tr("Done: the outputs of %1 follow %2; a new run updates them live.")
          .arg(m_deviceName, m_plan.names.join(QStringLiteral(", "))));
}

score::GUIElements CalibrationApplicationPlugin::makeGUIElements()
{
  score::GUIElements e;
  auto& menus = context.menus.get();
  if(auto it = menus.find(score::Menus::View()); it != menus.end())
  {
    auto* act = new QAction{QObject::tr("Projector calibration..."), it->second.menu()};
    QObject::connect(act, &QAction::triggered, act, [this] {
      auto* doc = context.docManager.currentDocument();
      if(!doc)
        return;
      if(m_wizard)
      {
        m_wizard->raise();
        return;
      }
      m_wizard = new CalibrationWizard{*doc};
      m_wizard->setAttribute(Qt::WA_DeleteOnClose);
      m_wizard->show();
    });
    it->second.menu()->addAction(act);
  }
  return e;
}
}
