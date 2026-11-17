#include "CalibrationWizard.hpp"

#include <Explorer/Commands/Update/UpdateDeviceSettings.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/Window.hpp>
#include <Gfx/TextureCapture.hpp>
#include <Gfx/Window/CalibrationSet.hpp>

#include <score/actions/MenuManager.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSettings>
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


namespace Threedim
{
namespace
{
bool isTexture(const ossia::net::node_base* n)
{
  const auto* p = n ? n->get_parameter() : nullptr;
  return p && p->get_type() == ossia::parameter_type::TEXTURE;
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
  m_settle->setToolTip(tr("Time each pattern is shown before the camera frames are taken"));
  form->addRow(tr("Settle"), m_settle);
  m_fov = new QDoubleSpinBox;
  m_fov->setRange(5., 175.);
  m_fov->setValue(60.);
  m_fov->setSuffix(QStringLiteral("°"));
  m_fov->setToolTip(tr("Horizontal field of view of the camera (a Logitech C930e is "
                       "about 82°): the viewing directions, hence the warp, scale with it"));
  form->addRow(tr("Camera FOV"), m_fov);
  m_level = new QDoubleSpinBox;
  m_level->setRange(0.05, 1.);
  m_level->setSingleStep(0.05);
  m_level->setValue(1.);
  m_level->setToolTip(tr("Brightness of the patterns: lower it when the camera saturates"));
  form->addRow(tr("Pattern level"), m_level);
  m_stripe = new QSpinBox;
  m_stripe->setRange(1, 128);
  m_stripe->setValue(2);
  m_stripe->setSuffix(tr(" px"));
  m_stripe->setToolTip(tr("Finest Gray-code stripe, in projector pixels: raise it when the "
                          "camera cannot resolve the projector pixels"));
  form->addRow(tr("Min stripe"), m_stripe);
  m_frames = new QSpinBox;
  m_frames->setRange(1, 32);
  m_frames->setValue(2);
  m_frames->setToolTip(tr("Camera frames averaged per pattern, against noise"));
  form->addRow(tr("Frames to average"), m_frames);
  m_exposure = new QDoubleSpinBox;
  m_exposure->setRange(0., 10000000.);
  m_exposure->setDecimals(2);
  m_exposure->setToolTip(tr("Exposure set on the camera's controls/color/exposure for the "
                            "run, in its own units; 0 leaves it as it is"));
  form->addRow(tr("Camera exposure"), m_exposure);
  m_blend = new QCheckBox;
  m_blend->setChecked(true);
  m_blend->setToolTip(tr("Compute the blend (alpha) and black-level (beta) maps afterwards"));
  form->addRow(tr("Blend maps"), m_blend);
  m_display = new QComboBox;
  m_display->addItem(tr("Projectors"));
  m_display->addItem(tr("Screens"));
  m_display->setToolTip(
      tr("Projectors: their overlaps are blended and the black level evened out. "
         "Screens (LED walls, monitors): emissive, side by side, nothing blended"));
  form->addRow(tr("Display"), m_display);
  m_flat = new QCheckBox;
  m_flat->setChecked(true);
  m_flat->setToolTip(
      tr("Viewer camera on flat walls and floor (at most two per output): each output's "
         "map is rebuilt from its planes, which fills what the camera did not see and "
         "removes the measurement noise. Turn off for a curved screen"));
  form->addRow(tr("Flat surfaces"), m_flat);
  m_photometric = new QCheckBox;
  m_photometric->setToolTip(
      tr("Also measure the projector response and the surface colour, with a colour "
         "camera (e.g. Cam:/rgb aligned to depth) whose exposure can be set"));
  form->addRow(tr("Photometric"), m_photometric);

  auto* buttons = new QHBoxLayout;
  m_refreshBtn = new QPushButton{tr("Refresh")};
  m_startBtn = new QPushButton{tr("Start")};
  m_abortBtn = new QPushButton{tr("Abort")};
  buttons->addWidget(m_refreshBtn);
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
  connect(m_startBtn, &QPushButton::clicked, this, [this] { start(); });
  connect(m_abortBtn, &QPushButton::clicked, this, &CalibrationWizard::abort);
  connect(m_device, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
    updatePlan();
  });
  connect(m_name, &QLineEdit::editingFinished, this, [this] { updatePlan(); });
  connect(m_mode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int i) {
    m_positions->setEnabled(i == 0);
    m_flat->setEnabled(i == 1);
    updatePlan();
  });
  connect(m_display, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int i) {
    m_blend->setEnabled(i == 0);
  });
  for(auto* sb : {m_settle, m_stripe, m_frames})
    connect(sb, qOverload<int>(&QSpinBox::valueChanged), this, [this] { updatePlan(); });
  connect(m_photometric, &QCheckBox::toggled, this, [this] { updatePlan(); });

  m_mode->setCurrentIndex(1);
  loadOptions();
  m_positions->setEnabled(m_mode->currentIndex() == 0);
  m_flat->setEnabled(m_mode->currentIndex() == 1);
  m_blend->setEnabled(m_display->currentIndex() == 0);
  m_timer.setInterval(15);
  m_timer.setTimerType(Qt::PreciseTimer);
  connect(&m_timer, &QTimer::timeout, this, &CalibrationWizard::poll);
  refreshDevices();
  updateButtons();
}

CalibrationWizard::~CalibrationWizard()
{
  if(busy())
    m_run->abort(tr("The calibration window was closed"));
  m_run.reset();
}

void CalibrationWizard::done(int r)
{
  saveOptions();
  if(busy())
  {
    m_run->abort(tr("The calibration window was closed"));
    for(const auto& l : m_run->takeLog())
      log(l);
  }
  m_run.reset();
  m_timer.stop();
  QDialog::done(r);
}

void CalibrationWizard::detach()
{
  m_timer.stop();
  m_run.reset();
  m_doc = nullptr;
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
  const bool active = busy();
  m_startBtn->setEnabled(!active && m_doc && m_device->count() > 0);
  m_device->setEnabled(!active);
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

score::gfx::MultiWindowNode* CalibrationWizard::multiWindowNode(
    const score::DocumentContext& ctx, const QString& device)
{
  auto& plug = ctx.plugin<Explorer::DeviceDocumentPlugin>();
  auto* d = plug.list().findDevice(device);
  auto* od = d ? d->getDevice() : nullptr;
  if(!od)
    return nullptr;
  auto* p = dynamic_cast<Gfx::gfx_parameter_base*>(od->get_root_node().get_parameter());
  return p ? dynamic_cast<score::gfx::MultiWindowNode*>(p->node) : nullptr;
}

ossia::net::parameter_base*
CalibrationWizard::parameterAt(const score::DocumentContext& ctx, const QString& address)
{
  const int colon = address.indexOf(':');
  if(colon <= 0)
    return nullptr;
  auto& plug = ctx.plugin<Explorer::DeviceDocumentPlugin>();
  auto* d = plug.list().findDevice(address.left(colon));
  auto* od = d ? d->getDevice() : nullptr;
  if(!od)
    return nullptr;
  QString path = address.mid(colon + 1);
  if(path.isEmpty())
    path = QStringLiteral("/");
  auto* n = path == QStringLiteral("/")
                ? &od->get_root_node()
                : ossia::net::find_node(od->get_root_node(), path.toStdString());
  return n ? n->get_parameter() : nullptr;
}

namespace
{
QSize physicalSize(const score::gfx::MultiWindowNode::WindowOutput& wo)
{
  if(wo.swapChain && !wo.swapChain->surfacePixelSize().isEmpty())
    return wo.swapChain->surfacePixelSize();
  if(wo.window)
    return wo.window->size() * wo.window->devicePixelRatio();
  return {};
}
}

CalibrationPlan
CalibrationWizard::livePlan(const Gfx::WindowSettings& ws, QStringList* notes) const
{
  auto physical = ws;
  auto* ctx = context();
  auto* node = ctx ? multiWindowNode(*ctx, m_device->currentText()) : nullptr;
  if(!node)
  {
    if(notes)
      notes->push_back(tr("the device is not connected: its windows must be open"));
  }
  else
  {
    const auto& outs = node->windowOutputs();
    for(int i = 0; i < (int)outs.size() && i < (int)physical.outputs.size(); i++)
      if(const QSize px = physicalSize(outs[i]); !px.isEmpty())
        physical.outputs[i].windowSize = px;
    if(notes && !outs.empty() && !physical.outputs.empty() && !ws.outputs.empty()
       && physical.outputs[0].windowSize != ws.outputs[0].windowSize)
      notes->push_back(tr("windows of %1x%2 points are %3x%4 pixels: the patterns use "
                          "the pixels")
                           .arg(ws.outputs[0].windowSize.width())
                           .arg(ws.outputs[0].windowSize.height())
                           .arg(physical.outputs[0].windowSize.width())
                           .arg(physical.outputs[0].windowSize.height()));
  }
  auto p = planCalibration(physical, m_name->text(), m_stripe->value(), 4);
  p.warnings.removeIf(
      [](const QString& w) { return w.contains(QStringLiteral("canvas pixels")); });
  return p;
}

StructuredLightSession::Settings CalibrationWizard::sessionSettings() const
{
  StructuredLightSession::Settings s;
  const bool depth = m_mode->currentIndex() == 0;
  s.mode = depth ? StructuredLightSession::DepthCamera : StructuredLightSession::ViewerCamera;
  s.settleMs = m_settle->value();
  s.fov = float(m_fov->value());
  s.level = float(m_level->value());
  s.minStripe = m_stripe->value();
  s.frames = m_frames->value();
  const bool photometric = m_photometric->isChecked();
  s.responseLevels = photometric ? 8 : 0;
  s.compensation = photometric;
  s.captureExposure = float(m_exposure->value());
  if(photometric && s.captureExposure > 0.f)
    for(float f : {0.125f, 0.25f, 0.5f, 1.f, 2.f, 4.f})
      s.exposures.push_back(s.captureExposure * f);
  return s;
}

void CalibrationWizard::loadOptions()
{
  QSettings q;
  q.beginGroup(QStringLiteral("Threedim/ProjectorCalibration"));
  if(q.contains("Mode"))
    m_mode->setCurrentIndex(std::clamp(q.value("Mode").toInt(), 0, 1));
  if(q.contains("Name"))
    m_name->setText(q.value("Name").toString());
  m_settle->setValue(q.value("Settle", m_settle->value()).toInt());
  m_fov->setValue(q.value("Fov", m_fov->value()).toDouble());
  m_level->setValue(q.value("Level", m_level->value()).toDouble());
  m_stripe->setValue(q.value("MinStripe", m_stripe->value()).toInt());
  m_frames->setValue(q.value("Frames", m_frames->value()).toInt());
  m_exposure->setValue(q.value("Exposure", m_exposure->value()).toDouble());
  m_blend->setChecked(q.value("Blend", m_blend->isChecked()).toBool());
  m_display->setCurrentIndex(std::clamp(q.value("Display", 0).toInt(), 0, 1));
  m_flat->setChecked(q.value("FlatSurfaces", m_flat->isChecked()).toBool());
  m_photometric->setChecked(q.value("Photometric", m_photometric->isChecked()).toBool());
  if(q.contains("Camera"))
    m_camera->setCurrentText(q.value("Camera").toString());
  if(q.contains("Positions"))
    m_positions->setCurrentText(q.value("Positions").toString());
  if(const auto d = q.value("Device").toString(); m_device->findText(d) >= 0)
    m_device->setCurrentText(d);
}

void CalibrationWizard::saveOptions() const
{
  QSettings q;
  q.beginGroup(QStringLiteral("Threedim/ProjectorCalibration"));
  q.setValue("Mode", m_mode->currentIndex());
  q.setValue("Name", m_name->text());
  q.setValue("Settle", m_settle->value());
  q.setValue("Fov", m_fov->value());
  q.setValue("Level", m_level->value());
  q.setValue("MinStripe", m_stripe->value());
  q.setValue("Frames", m_frames->value());
  q.setValue("Exposure", m_exposure->value());
  q.setValue("Blend", m_blend->isChecked());
  q.setValue("Display", m_display->currentIndex());
  q.setValue("FlatSurfaces", m_flat->isChecked());
  q.setValue("Photometric", m_photometric->isChecked());
  q.setValue("Camera", m_camera->currentText());
  q.setValue("Positions", m_positions->currentText());
  q.setValue("Device", m_device->currentText());
}

void CalibrationWizard::updatePlan()
{
  if(!m_stripe)
    return;
  if(auto ws = deviceSettings(m_device->currentText()))
  {
    QStringList notes;
    const auto p = livePlan(*ws, &notes);
    const auto st = sessionSettings();
    const int extras = int(StructuredLightCore::makeExtras(
                               st.responseLevels, st.compensation, st.exposures, 0,
                               st.mixLevels)
                               .size());
    QString s = tr("%1 projector(s) of %2x%3, %4 patterns each, about %5 s")
                    .arg(p.projectors)
                    .arg(p.projectorSize.width())
                    .arg(p.projectorSize.height())
                    .arg(p.patternsPerProjector + extras)
                    .arg(int(estimateMs(p, st) / 1000.));
    for(const auto& w : p.warnings)
      s += QStringLiteral("\n- ") + w;
    for(const auto& w : notes)
      s += QStringLiteral("\n- ") + w;
    m_info->setText(s);
  }
  else
    m_info->clear();
  updateButtons();
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
  else
    updatePlan();
  updateButtons();
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

double CalibrationWizard::estimateMs(
    const CalibrationPlan& plan, const StructuredLightSession::Settings& s) const
{
  CaptureTiming t;
  t.settleMs = s.settleMs;
  t.framesToAverage = s.frames;
  t.exposureSettleMs = s.exposureSettleMs;
  for(int k = 0; k < plan.projectors; k++)
  {
    const auto extras = StructuredLightCore::makeExtras(
        s.responseLevels, s.compensation, k == 0 ? s.exposures : std::vector<float>{}, k,
        s.mixLevels);
    t.extraFrames.push_back(int(extras.size()));
    int changes = (k == 0 && s.captureExposure > 0.f) ? 1 : 0;
    float current = s.captureExposure;
    for(const auto& e : extras)
      if(const float x = e.exposure > 0.f ? e.exposure : s.captureExposure; x != current)
      {
        changes++;
        current = x;
      }
    if(current != s.captureExposure)
      changes++;
    t.exposureChanges.push_back(changes);
  }
  return estimateCaptureMs(plan, t) + 5000.;
}

bool CalibrationWizard::start()
{
  auto* ctx = context();
  if(!ctx || busy())
    return false;
  const QString deviceName = m_device->currentText();
  const auto ws = deviceSettings(deviceName);
  if(!ws)
  {
    log(tr("Pick a multi-window output device."));
    return false;
  }
  saveOptions();
  CalibrationRun::Setup setup;
  QStringList notes;
  setup.plan = livePlan(*ws, &notes);
  for(const auto& n : notes)
    log(QStringLiteral("- ") + n);
  if(setup.plan.projectors == 0)
  {
    for(const auto& w : setup.plan.warnings)
      log(w);
    return false;
  }
  setup.gfx = &ctx->plugin<Gfx::DocumentPlugin>().context;
  QPointer<score::Document> doc = m_doc;
  setup.node = [doc, deviceName]() -> score::gfx::MultiWindowNode* {
    return doc ? multiWindowNode(doc->context(), deviceName) : nullptr;
  };

  const QString cam = m_camera->currentText();
  if(auto* p = parameterAt(*ctx, cam))
    if(auto port = Gfx::textureProducer(*p))
      setup.camera = *port;
  if(setup.camera.node < 0)
  {
    log(tr("%1 is not the texture of a camera device.").arg(cam));
    return false;
  }
  const bool depth = m_mode->currentIndex() == 0;
  if(depth)
    if(auto* p = parameterAt(*ctx, m_positions->currentText()))
      if(auto port = Gfx::textureProducer(*p))
        setup.positions = *port;

  {
    QStringList devices{cam.section(':', 0, 0)};
    if(depth && !m_positions->currentText().isEmpty())
      if(const QString d = m_positions->currentText().section(':', 0, 0); !devices.contains(d))
        devices.push_back(d);
    setup.cameraActive = [doc, devices](bool on) {
      if(!doc)
        return;
      const auto& c = doc->context();
      if(auto* exec = c.findPlugin<Execution::DocumentPlugin>(); exec && exec->isPlaying())
        return;
      auto& plug = c.plugin<Explorer::DeviceDocumentPlugin>();
      for(const auto& name : devices)
        if(auto* d = plug.list().findDevice(name); d && d->getDevice())
        {
          auto& proto = d->getDevice()->get_protocol();
          if(on)
            proto.start_execution();
          else
            proto.stop_execution();
        }
    };
  }

  setup.settings = sessionSettings();
  auto& s = setup.settings;
  const bool photometric = m_photometric->isChecked();
  {
    const QString dev = cam.section(':', 0, 0);
    const QString exposure = dev + QStringLiteral(":/controls/color/exposure");
    const QString autoExposure = dev + QStringLiteral(":/controls/color/auto_exposure");
    auto* e = parameterAt(*ctx, exposure);
    if(photometric && cam.endsWith(QStringLiteral(":/ir")))
      log(tr("Photometric calibration needs a colour camera, e.g. %1:/rgb.").arg(dev));
    if(photometric && e && !(s.captureExposure > 0.f))
    {
      const float current = ossia::convert<float>(e->value());
      if(current > 0.f)
      {
        s.captureExposure = current;
        for(float f : {0.125f, 0.25f, 0.5f, 1.f, 2.f, 4.f})
          s.exposures.push_back(current * f);
      }
    }
    if(e)
    {
      setup.setExposure = [doc, exposure](float v) {
        if(doc)
          if(auto* p = parameterAt(doc->context(), exposure))
            p->push_value(v);
      };
      setup.setAutoExposure = [doc, autoExposure](bool v) {
        if(doc)
          if(auto* p = parameterAt(doc->context(), autoExposure))
            p->push_value(v);
      };
    }
    else if(s.captureExposure > 0.f || photometric)
      log(tr("No colour exposure control on %1 (%2): the exposure is not set, and the "
             "camera is assumed linear. Turn its auto exposure and auto white balance "
             "off.")
              .arg(dev, exposure));
  }
  setup.blend = m_blend->isChecked() && m_display->currentIndex() == 0;
  setup.completePlanes = !depth && m_flat->isChecked();
  setup.canvas = ws->canvas;
  for(const auto& o : ws->outputs)
    setup.sourceRects.push_back(o.sourceRect);
  if(!depth && ws->canvas.projection != Gfx::CanvasProjection::Equirectangular)
    log(tr("The canvas projection of %1 is not Equirectangular: no warp maps are made "
           "from the viewing directions.")
            .arg(deviceName));

  m_estimateMs = estimateMs(setup.plan, s);
  const auto plan = setup.plan;
  m_run = std::make_unique<CalibrationRun>(std::move(setup));
  m_runDevice = deviceName;
  m_runStartMs = QDateTime::currentMSecsSinceEpoch();
  const bool ok = m_run->start(double(m_runStartMs));
  for(const auto& l : m_run->takeLog())
    log(l);
  if(!ok)
  {
    m_run.reset();
    updateButtons();
    return false;
  }
  log(tr("Calibrating %1 projector(s) of %2: about %3 s.")
          .arg(plan.projectors)
          .arg(deviceName)
          .arg(int(m_estimateMs / 1000.)));
  for(const auto& w : plan.warnings)
    if(!w.contains(QStringLiteral("canvas pixels")))
      log(QStringLiteral("- ") + w);
  m_progress->setValue(0);
  m_status->clear();
  m_timer.start();
  updateButtons();
  return true;
}

void CalibrationWizard::abort()
{
  if(!busy())
    return;
  m_run->abort(tr("Aborted"));
  poll();
}

void CalibrationWizard::poll()
{
  if(!m_run)
  {
    m_timer.stop();
    return;
  }
  m_run->tick(double(QDateTime::currentMSecsSinceEpoch()));
  for(const auto& l : m_run->takeLog())
    log(l);
  m_status->setText(m_run->status());
  m_progress->setValue(int(m_run->progress() * 1000));
  switch(m_run->state())
  {
    case CalibrationRun::State::Done:
      finish();
      break;
    case CalibrationRun::State::Failed:
      m_timer.stop();
      m_run.reset();
      updateButtons();
      break;
    default:
      break;
  }
}

void CalibrationWizard::finish()
{
  m_timer.stop();
  const auto names = m_run->setup().plan.names;
  m_run.reset();
  updateButtons();
  const auto r = readCalibrationResults(names, m_runStartMs);
  if(!r.text.isEmpty())
    log(r.text);
  const auto ws = deviceSettings(m_runDevice);
  if(ws && updateDevice(m_runDevice, assignCalibrations(*ws, names, m_runStartMs)))
    log(tr("Done: the outputs of %1 follow %2; a new run updates them live.")
            .arg(m_runDevice, names.join(QStringLiteral(", "))));
  else
    log(tr("Done, but %1 could not be updated: assign %2 in its settings.")
            .arg(m_runDevice, names.join(QStringLiteral(", "))));
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
