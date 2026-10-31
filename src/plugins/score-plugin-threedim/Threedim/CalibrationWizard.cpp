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
#include <Threedim/StructuredLightCalibration.hpp>

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

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

#include <mutex>

namespace Threedim
{
struct CalibrationWizard::OutletTap
{
  std::mutex mutex;
  std::string status;
  float progress{-1.f};
  int version{};
};

namespace
{
constexpr const char* slcUuid = "3f8c2a61-7d4e-4b19-9a05-c6e1d2b8f47a";
constexpr const char* blendUuid = "a0d6f1c8-2b47-4e93-8f15-7c3e9b2d6a41";

UuidKey<Process::ProcessModel> processKey(const char* uuid)
{
  return UuidKey<Process::ProcessModel>{
      score::uuids::string_generator::compute(uuid, uuid + 36)};
}

Process::ProcessModelFactory*
findFactory(const score::DocumentContext& ctx, const char* uuid)
{
  const auto key = processKey(uuid);
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

template <typename Pred>
Process::ProcessModel* findProcess(
    const Scenario::IntervalModel& itv, const char* uuid, const Pred& pred)
{
  const auto key = processKey(uuid);
  for(auto& p : itv.processes)
    if(p.concreteKey() == key && pred(p))
      return &p;
  return nullptr;
}

bool isTexture(const ossia::net::node_base* n)
{
  const auto* p = n ? n->get_parameter() : nullptr;
  return p && p->get_type() == ossia::parameter_type::TEXTURE;
}

std::string_view slcModeName(int index)
{
  using Mode = decltype(std::declval<StructuredLightCalibration::ins&>().mode);
  static constexpr Mode::range r{};
  return r.values[std::clamp(index, 0, int(std::size(r.values)) - 1)];
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

bool stoppedStatus(std::string_view s)
{
  return s.find("Failed") != std::string_view::npos || s.starts_with("Aborted");
}
}

CalibrationWizard::CalibrationWizard(score::Document& doc, QWidget* parent)
    : QDialog{parent}
    , m_doc{&doc}
    , m_session{[this](const QString& device, const Gfx::WindowSettings& ws) {
      return updateDevice(device, ws);
    }}
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
  m_refreshBtn = new QPushButton{tr("Refresh")};
  m_setupBtn = new QPushButton{tr("Set up")};
  m_startBtn = new QPushButton{tr("Start")};
  m_abortBtn = new QPushButton{tr("Abort")};
  buttons->addWidget(m_refreshBtn);
  buttons->addWidget(m_setupBtn);
  buttons->addWidget(m_startBtn);
  buttons->addWidget(m_abortBtn);
  lay->addLayout(buttons);
  m_progress = new QProgressBar;
  m_progress->setRange(0, 1000);
  lay->addWidget(m_progress);
  m_status = new QLabel;
  m_status->setWordWrap(true);
  lay->addWidget(m_status);
  m_log = new QPlainTextEdit;
  m_log->setReadOnly(true);
  m_log->setMinimumHeight(120);
  lay->addWidget(m_log);

  connect(m_refreshBtn, &QPushButton::clicked, this, &CalibrationWizard::refreshDevices);
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
  refreshDevices();
  updateButtons();
}

CalibrationWizard::~CalibrationWizard()
{
  if(m_session.active())
    stopRun(tr("The calibration window was closed"));
}

void CalibrationWizard::done(int r)
{
  if(m_session.active())
    stopRun(tr("The calibration window was closed"));
  QDialog::done(r);
}

void CalibrationWizard::detach()
{
  m_timer.stop();
  m_session.drop();
  m_slc = nullptr;
  m_blendProc = nullptr;
  m_doc = nullptr;
  m_phase = Phase::Idle;
  updateButtons();
}

const score::DocumentContext* CalibrationWizard::context() const
{
  return m_doc ? &m_doc->context() : nullptr;
}

void CalibrationWizard::log(const QString& s)
{
  m_log->appendPlainText(s);
}

void CalibrationWizard::updateButtons()
{
  const bool active = m_session.active();
  m_setupBtn->setEnabled(!active && m_doc);
  m_device->setEnabled(!active);
  m_startBtn->setEnabled(active && m_phase == Phase::Ready);
  m_abortBtn->setEnabled(active);
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
    auto* od = d->getDevice();
    if(!od)
      continue;
    auto& root = od->get_root_node();
    if(isTexture(&root))
      m_camera->addItem(s.name + ":/");
    for(const char* child : {"ir", "rgb"})
      if(isTexture(ossia::net::find_node(root, std::string("/") + child)))
        m_camera->addItem(s.name + ":/" + child);
    for(const auto& c : root.children())
    {
      const QString name = QString::fromStdString(c->get_name());
      if(!isTexture(c.get()) || name == "ir" || name == "rgb")
        continue;
      if(name == "positions")
        m_positions->addItem(s.name + ":/positions");
      else
        m_camera->addItem(s.name + ":/" + name);
    }
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

bool CalibrationWizard::updateDevice(const QString& device, const Gfx::WindowSettings& ws)
{
  auto* ctx = context();
  if(!ctx)
    return false;
  auto& plug = ctx->plugin<Explorer::DeviceDocumentPlugin>();
  auto* d = plug.list().findDevice(device);
  if(!d)
    return false;
  auto s = d->settings();
  s.deviceSpecificSettings = QVariant::fromValue(ws);
  CommandDispatcher<> disp{ctx->commandStack};
  disp.submit(new Explorer::Command::UpdateDeviceSettings{plug, device, s});
  return true;
}

bool CalibrationWizard::setUp()
{
  auto* ctx = context();
  if(!ctx)
    return false;
  if(m_session.active())
  {
    log(tr("%1 is already set up: start, or abort first.").arg(m_session.device()));
    return false;
  }
  const QString deviceName = m_device->currentText();
  const auto ws = deviceSettings(deviceName);
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
  auto& itv = root.baseInterval();
  Scenario::Command::Macro m{
      new Scenario::Command::AddMultipleProcessesToMultipleIntervalsMacro, *ctx};

  auto drivesDevice = [&](const Process::ProcessModel& p) {
    for(const char* name : {"Canvas", "Pattern"})
      if(auto* o = findPort(p.outlets(), QString::fromLatin1(name));
         o && o->address().address.device == deviceName)
        return true;
    return false;
  };
  if(!m_slc)
    m_slc = findProcess(itv, slcUuid, drivesDevice);
  if(m_slc)
    log(tr("Reusing %1.").arg(m_slc->metadata().getName()));
  else
    m_slc = m.createProcessInNewSlot(itv, slcFactory->concreteKey(), {});
  if(!m_slc)
  {
    log(tr("Could not create Structured Light Calibration."));
    return false;
  }
  {
    const auto key = processKey(slcUuid);
    int others = 0;
    for(auto& p : itv.processes)
      others += &p != m_slc.data() && p.concreteKey() == key && drivesDevice(p);
    if(others > 0)
      log(tr("%1 other Structured Light Calibration process(es) also drive %2: remove "
             "them, their idle pattern covers this one's.")
              .arg(others)
              .arg(deviceName));
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
                   ? ossia::value{std::string(slcModeName(m_mode->currentIndex()))}
                   : ossia::value{m_mode->currentIndex()});
  set(*m_slc, QStringLiteral("Settle (ms)"), m_settle->value());
  set(*m_slc, QStringLiteral("Save file"),
      QString(m_plan.names[0] + QStringLiteral(".json")).toStdString());
  set(*m_slc, QStringLiteral("Auto save"), true);

  auto* canvas = findPort(m_slc->outlets(), QStringLiteral("Canvas"));
  if(canvas)
  {
    set(*m_slc, QStringLiteral("Canvas layout"), m_plan.canvasLayout.toStdString());
    set(*m_slc, QStringLiteral("Canvas size"),
        ossia::vec2f{float(m_plan.canvasSize.width()), float(m_plan.canvasSize.height())});
    address(canvas, deviceName + QStringLiteral(":/"));
  }
  else if(m_plan.projectors == 1)
    address(findPort(m_slc->outlets(), QStringLiteral("Pattern")), deviceName + ":/");
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

  const bool photometric = m_photometric->isChecked();
  set(*m_slc, QStringLiteral("Response levels"), photometric ? 8 : 0);
  set(*m_slc, QStringLiteral("Surface compensation"), photometric);
  set(*m_slc, QStringLiteral("Exposures"), std::string{});
  set(*m_slc, QStringLiteral("Capture exposure"), 0.f);
  if(photometric)
  {
    const QString cam = m_camera->currentText();
    const QString dev = cam.section(':', 0, 0);
    if(cam.endsWith(QStringLiteral(":/ir")))
      log(tr("Photometric calibration needs a colour camera, e.g. %1:/rgb.").arg(dev));
    ossia::net::node_base* exposure{};
    ossia::net::node_base* autoExposure{};
    auto& plug = ctx->plugin<Explorer::DeviceDocumentPlugin>();
    if(auto* d = plug.list().findDevice(dev); d && d->getDevice())
    {
      auto& droot = d->getDevice()->get_root_node();
      exposure = ossia::net::find_node(droot, "/controls/color/exposure");
      autoExposure = ossia::net::find_node(droot, "/controls/color/auto_exposure");
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

  QStringList files;
  for(const auto& n : m_plan.names)
    files.push_back(n + QStringLiteral(".json"));
  const QString fileList = files.join(QStringLiteral("; "));
  if(!m_blendProc)
    m_blendProc = findProcess(itv, blendUuid, [&](const Process::ProcessModel& p) {
      auto* in
          = qobject_cast<Process::ControlInlet*>(findPort(p.inlets(), "Calibration files"));
      return in
             && QString::fromStdString(ossia::convert<std::string>(in->value()))
                    .contains(files[0]);
    });
  if(m_blend->isChecked())
  {
    if(!m_blendProc)
      if(auto* bf = findFactory(*ctx, blendUuid))
        m_blendProc = m.createProcessInNewSlot(itv, bf->concreteKey(), {});
    if(m_blendProc)
      set(*m_blendProc, QStringLiteral("Calibration files"), fileList.toStdString());
  }
  else if(m_blendProc)
  {
    if(auto* parent = qobject_cast<Scenario::IntervalModel*>(m_blendProc->parent()))
      m.removeProcess(*parent, m_blendProc->id());
    m_blendProc = nullptr;
  }
  m.commit();

  if(!m_session.begin(deviceName, *ws))
  {
    log(tr("Could not turn off the output stage of %1.").arg(deviceName));
    return false;
  }

  log(tr("Set up %1 projector(s) on %2; the output stage of %2 is off until the end.")
          .arg(m_plan.projectors)
          .arg(deviceName));
  for(const auto& w : m_plan.warnings)
    log(QStringLiteral("- ") + w);
  m_phase = Phase::Ready;
  updateButtons();
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

void CalibrationWizard::requestTap(
    Process::ProcessModel* p, const std::shared_ptr<OutletTap>& tap)
{
  auto* ctx = context();
  if(!ctx || !p || !tap)
    return;
  auto* plug = ctx->findPlugin<Execution::DocumentPlugin>();
  if(!plug || !plug->isPlaying())
    return;
  auto& exec = plug->context();
  struct Read
  {
    std::weak_ptr<ossia::graph_node> node;
    ossia::outlet* port{};
  } reads[2];
  int k = 0;
  for(const char* name : {"Status", "Progress"})
  {
    auto* out = findPort(p->outlets(), QString::fromLatin1(name));
    if(auto it = exec.setup.outlets.find(out);
       out && it != exec.setup.outlets.end() && it->second.first && it->second.second)
      reads[k] = {it->second.first, it->second.second};
    k++;
  }
  if(!reads[0].port && !reads[1].port)
    return;
  exec.executionQueue.enqueue([tap, r0 = reads[0], r1 = reads[1]] {
    auto last = [](const Read& r) -> ossia::value {
      auto n = r.node.lock();
      auto* vp = n && r.port ? r.port->target<ossia::value_port>() : nullptr;
      if(!vp || vp->get_data().empty())
        return {};
      return vp->get_data().back().value;
    };
    const ossia::value status = last(r0), progress = last(r1);
    if(!status.valid() && !progress.valid())
      return;
    std::lock_guard l{tap->mutex};
    if(auto* s = status.target<std::string>())
      tap->status = *s;
    if(progress.valid())
      tap->progress = ossia::convert<float>(progress);
    tap->version++;
  });
}

ossia::value
CalibrationWizard::inletValue(Process::ProcessModel* p, const QString& inlet) const
{
  if(!p)
    return {};
  if(auto* in = qobject_cast<Process::ControlInlet*>(findPort(p->inlets(), inlet)))
    return in->value();
  return {};
}

double CalibrationWizard::estimateMs() const
{
  auto num = [&](const char* name, float def) {
    const auto v = inletValue(m_slc, QString::fromLatin1(name));
    return v.valid() ? ossia::convert<float>(v) : def;
  };
  const bool phase = num("Phase shift", 1.f) != 0.f;
  const auto plan
      = m_session.original()
            ? planCalibration(
                  *m_session.original(), m_name->text(), int(num("Min stripe px", 2.f)),
                  phase ? int(num("Phase steps", 4.f)) : 0)
            : m_plan;

  CaptureTiming t;
  t.settleMs = num("Settle (ms)", float(m_settle->value()));
  t.framesToAverage = int(num("Frames to average", 2.f));
  t.exposureSettleMs = num("Exposure settle (ms)", 1500.f);
  const float capture = num("Capture exposure", 0.f);
  std::vector<float> exposures;
  const auto ev = inletValue(m_slc, QStringLiteral("Exposures"));
  for(const auto& s : QString::fromStdString(ev.valid() ? ossia::convert<std::string>(ev) : "")
                          .split(QRegularExpression("[\\s,;]+"), Qt::SkipEmptyParts))
    if(const float v = s.toFloat(); v > 0.f)
      exposures.push_back(v);
  const int levels = int(num("Response levels", 0.f));
  const bool comp = num("Surface compensation", 0.f) != 0.f;
  const int mix = int(num("Colour mixing levels", 5.f));
  for(int k = 0; k < plan.projectors; k++)
  {
    const auto extras
        = StructuredLightCalibration::makeExtras(levels, comp, exposures, k, mix);
    t.extraFrames.push_back(int(extras.size()));
    int changes = (k == 0 && capture > 0.f) ? 1 : 0;
    float current = capture;
    for(const auto& e : extras)
      if(const float x = e.exposure > 0.f ? e.exposure : capture; x != current)
      {
        changes++;
        current = x;
      }
    if(current != capture)
      changes++;
    t.exposureChanges.push_back(changes);
  }
  return estimateCaptureMs(plan, t) + 5000.;
}

void CalibrationWizard::start()
{
  auto* ctx = context();
  if(!ctx || !m_slc || m_phase != Phase::Ready)
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
  m_startMs = m_runStartMs = QDateTime::currentMSecsSinceEpoch();
  m_estimateMs = estimateMs();
  m_slcTap = std::make_shared<OutletTap>();
  m_blendTap = std::make_shared<OutletTap>();
  m_seenTap = 0;
  m_seenRunning = false;
  m_progress->setValue(0);
  m_status->clear();
  m_timer.start();
  updateButtons();
  log(tr("Starting: about %1 s.").arg(int(m_estimateMs / 1000.)));
}

void CalibrationWizard::stopRun(const QString& why)
{
  m_timer.stop();
  const bool wasActive = m_session.active();
  pushValue(m_slc, QStringLiteral("Abort"), ossia::impulse{});
  const QString device = m_session.device();
  const bool restored = m_session.restore();
  m_phase = Phase::Idle;
  updateButtons();
  if(!wasActive)
    log(why);
  else if(restored)
    log(tr("%1; %2 is back to its settings.").arg(why, device));
  else
    log(tr("%1; %2 could not be put back to its settings.").arg(why, device));
}

void CalibrationWizard::abort()
{
  if(!m_session.active() && m_phase != Phase::Starting && m_phase != Phase::Capturing
     && m_phase != Phase::Blending)
    return;
  stopRun(tr("Aborted"));
}

void CalibrationWizard::poll()
{
  if(!context())
  {
    m_timer.stop();
    return;
  }
  if(!m_slc)
  {
    stopRun(tr("The Structured Light Calibration process was removed"));
    return;
  }
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  auto readTap = [&](OutletTap& tap, std::string& status, float& progress) {
    std::lock_guard l{tap.mutex};
    status = tap.status;
    progress = tap.progress;
    return tap.version;
  };
  switch(m_phase)
  {
    case Phase::Starting:
      if(pushValue(m_slc, QStringLiteral("Start"), ossia::impulse{}))
      {
        m_phase = Phase::Capturing;
        m_startMs = m_lastChangeMs = now;
        log(tr("Projecting the patterns."));
      }
      else if(now - m_startMs > 10000)
        stopRun(tr("The score did not start playing"));
      break;
    case Phase::Capturing: {
      requestTap(m_slc, m_slcTap);
      std::string status;
      float progress{};
      const int version = readTap(*m_slcTap, status, progress);
      if(version != m_seenTap)
      {
        if(QString::fromStdString(status) != m_status->text())
        {
          m_status->setText(QString::fromStdString(status));
          m_lastChangeMs = now;
        }
        if(status.starts_with("Capturing"))
          m_seenRunning = true;
        m_seenTap = version;
      }
      if(version > 0 && stoppedStatus(status) && (m_seenRunning || now - m_startMs > 3000))
      {
        stopRun(tr("Structured Light Calibration stopped: %1")
                    .arg(QString::fromStdString(status)));
        return;
      }
      const double p = m_seenRunning && progress >= 0.f
                           ? double(progress)
                           : (now - m_startMs) / std::max(1., m_estimateMs);
      m_progress->setValue(int(std::min(0.99, p) * 1000));

      const auto r = readCalibrationResults(m_plan.names, m_runStartMs);
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
      else if(version > 0 && now - m_lastChangeMs > 300000)
        stopRun(tr("Structured Light Calibration made no progress for 5 minutes"));
      else if(now - m_startMs > 3 * m_estimateMs + 300000)
        stopRun(tr("No result: look at the Status of the Structured Light Calibration "
                   "process (camera address, camera running, projector visible)"));
      break;
    }
    case Phase::Blending: {
      requestTap(m_blendProc, m_blendTap);
      std::string status;
      float progress{};
      const int version = readTap(*m_blendTap, status, progress);
      int ready = 0;
      for(const auto& n : m_plan.names)
        if(QFileInfo fi{n + QStringLiteral(".alpha.pfm")};
           fi.isFile() && fi.lastModified().toMSecsSinceEpoch() + 1000 >= m_blendMs)
          ready++;
      if(ready == m_plan.projectors)
        finish();
      else if(!m_blendProc)
      {
        log(tr("The Projector Blend Maps process was removed; assigning the calibration "
               "without blend maps."));
        finish();
      }
      else if(version > 0 && status.starts_with("Failed") && now - m_blendMs > 3000)
      {
        log(tr("Projector Blend Maps: %1; assigning the calibration without new blend "
               "maps.")
                .arg(QString::fromStdString(status)));
        finish();
      }
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
  const QString device = m_session.device();
  const bool ok = m_session.finish(m_plan.names, m_runStartMs);
  m_phase = Phase::Done;
  updateButtons();
  if(ok)
    log(tr("Done: the outputs of %1 follow %2; a new run updates them live.")
            .arg(device, m_plan.names.join(QStringLiteral(", "))));
  else
    log(tr("Done, but %1 could not be updated: assign %2 in its settings.")
            .arg(device, m_plan.names.join(QStringLiteral(", "))));
}

score::GUIElements CalibrationApplicationPlugin::makeGUIElements()
{
  score::GUIElements e;
  auto& menus = context.menus.get();
  if(auto it = menus.find(score::Menus::View()); it != menus.end())
  {
    m_action = new QAction{QObject::tr("Projector calibration..."), it->second.menu()};
    m_action->setEnabled(context.docManager.currentDocument() != nullptr);
    QObject::connect(m_action, &QAction::triggered, m_action, [this] {
      auto* doc = context.docManager.currentDocument();
      if(!doc)
        return;
      if(m_wizard && (m_wizard->document() == doc || m_wizard->busy()))
      {
        m_wizard->show();
        m_wizard->raise();
        m_wizard->activateWindow();
        return;
      }
      if(m_wizard)
        m_wizard->close();
      m_wizard = new CalibrationWizard{*doc, context.mainWindow};
      m_wizard->setAttribute(Qt::WA_DeleteOnClose);
      m_wizard->show();
    });
    it->second.menu()->addAction(m_action);
  }
  return e;
}

void CalibrationApplicationPlugin::on_documentChanged(
    score::Document* olddoc, score::Document* newdoc)
{
  if(m_action)
    m_action->setEnabled(newdoc != nullptr);
  if(m_wizard && !m_wizard->busy() && m_wizard->document() != newdoc)
    m_wizard->close();
}

void CalibrationApplicationPlugin::on_closeDocument(score::Document& doc)
{
  if(m_wizard && m_wizard->document() == &doc)
  {
    m_wizard->detach();
    m_wizard->close();
  }
}
}
