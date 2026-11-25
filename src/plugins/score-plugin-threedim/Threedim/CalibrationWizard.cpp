#include "CalibrationWizard.hpp"

#include <Explorer/Commands/Update/UpdateDeviceSettings.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <Gfx/CameraDevice.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/Window.hpp>
#include <Gfx/TextureCapture.hpp>
#include <Gfx/Window/CalibrationSet.hpp>

#include <score/actions/ActionManager.hpp>
#include <score/actions/MenuManager.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <wobjectimpl.h>

#include <QAction>
#include <QCryptographicHash>
#include <QMessageBox>
#include <QScrollArea>
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

W_OBJECT_IMPL(Threedim::CalibrationWizard)

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
  auto* scroll = new QScrollArea;
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  m_options = new QWidget;
  auto* form = new QFormLayout{m_options};
  scroll->setWidget(m_options);
  lay->addWidget(scroll, 1);

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
  m_name->setText(QStringLiteral("calibration/room"));
  m_name->setToolTip(
      tr("Output 1 is saved as name.json, output 2 as name_p1.json, ...; relative to the "
         "document's folder"));
  auto* browse = new QPushButton{tr("...")};
  browse->setMaximumWidth(30);
  auto* nameRow = new QHBoxLayout;
  nameRow->addWidget(m_name, 1);
  nameRow->addWidget(browse);
  form->addRow(tr("Calibration name"), nameRow);
  connect(browse, &QPushButton::clicked, this, [this] {
    const QDir folder{documentFolder()};
    const auto f = QFileDialog::getSaveFileName(
        this, tr("Calibration name"), folder.absoluteFilePath(m_name->text()),
        tr("Calibrations (*.json)"));
    if(f.isEmpty())
      return;
    const QString base = Gfx::calibrationBase(f);
    const QString rel = folder.relativeFilePath(base);
    m_name->setText(rel.startsWith(QStringLiteral("..")) ? base : rel);
  });

  m_outputs = new QLineEdit;
  m_outputs->setPlaceholderText(tr("all"));
  m_outputs->setToolTip(
      tr("The outputs to measure in this run, e.g. 1-4 or 5, 6 when the camera sees "
         "some of them only: the others keep their calibration, and the blend is "
         "computed over every output's"));
  form->addRow(tr("Outputs"), m_outputs);
  connect(m_outputs, &QLineEdit::textChanged, this, [this] { updatePlan(); });
  m_alignTo = new QLineEdit;
  m_alignTo->setPlaceholderText(tr("none"));
  m_alignTo->setToolTip(
      tr("Viewer camera turned since a previous run (e.g. tilted down for the floor): "
         "outputs measured in both runs, whose current calibration this run is aligned "
         "to. They must be among the outputs of this run"));
  form->addRow(tr("Align to outputs"), m_alignTo);

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
  m_lens = new QComboBox;
  m_lens->addItem(tr("Pinhole"));
  m_lens->addItem(tr("Distorted (k1 k2 p1 p2 k3)"));
  m_lens->addItem(tr("Fisheye (k1 k2 k3 k4)"));
  m_lens->setToolTip(
      tr("How the camera's lens bends the view: Pinhole for a webcam without "
         "distortion; Distorted with OpenCV's radial and tangential coefficients; "
         "Fisheye (equidistant, OpenCV's fisheye model) for wide lenses and domes, "
         "with the FOV across the image width"));
  form->addRow(tr("Lens"), m_lens);
  m_lensK = new QLineEdit;
  m_lensK->setPlaceholderText(QStringLiteral("0 0 0 0 0"));
  m_lensK->setToolTip(tr("The lens coefficients, separated by spaces, in OpenCV's order"));
  form->addRow(tr("Lens coefficients"), m_lensK);
  m_estimateLens = new QCheckBox;
  m_estimateLens->setToolTip(
      tr("With Flat surfaces: find the radial distortion (k1) for which the walls and "
         "floor come out flat, and use it; the coefficients above are then updated. "
         "Works with Recompute, without capturing again"));
  form->addRow(tr("Estimate lens distortion"), m_estimateLens);
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
  m_geometry = new QComboBox;
  m_geometry->addItem(tr("Directions only"));
  m_geometry->addItem(tr("Room box"));
  m_geometry->addItem(tr("Room model"));
  m_geometry->setToolTip(
      tr("Viewer camera: where each projector pixel lands in the room. Directions only "
         "ties the warp to the camera's position; Room box (measured distances) and "
         "Room model (.obj, .ply) give each output a points map, so the viewer "
         "position of the Window device can move afterwards"));
  form->addRow(tr("Surface geometry"), m_geometry);
  {
    m_boxRow = new QWidget;
    auto* l = new QHBoxLayout{m_boxRow};
    l->setContentsMargins(0, 0, 0, 0);
    const QString names[6]{tr("front"), tr("back"), tr("left"), tr("right"), tr("floor"),
                           tr("ceiling")};
    for(int i = 0; i < 6; i++)
    {
      auto* sb = new QDoubleSpinBox;
      sb->setRange(0., 1000.);
      sb->setDecimals(3);
      sb->setSingleStep(0.05);
      sb->setSuffix(tr(" m"));
      sb->setToolTip(tr("Distance from the camera to the %1 (0 = open)").arg(names[i]));
      sb->setAccessibleName(tr("Room %1").arg(names[i]));
      m_box[i] = sb;
      l->addWidget(sb);
    }
    form->addRow(tr("Room (front back left right floor ceiling)"), m_boxRow);
  }
  {
    m_modelRow = new QWidget;
    auto* l = new QHBoxLayout{m_modelRow};
    l->setContentsMargins(0, 0, 0, 0);
    m_model = new QLineEdit;
    m_model->setToolTip(tr("Room mesh in metres, +Y up: Wavefront .obj or .ply"));
    auto* b = new QPushButton{tr("...")};
    b->setMaximumWidth(30);
    l->addWidget(m_model, 1);
    l->addWidget(b);
    connect(b, &QPushButton::clicked, this, [this] {
      const auto f = QFileDialog::getOpenFileName(
          this, tr("Room model"), m_model->text(), tr("Meshes (*.obj *.ply)"));
      if(!f.isEmpty())
        m_model->setText(f);
    });
    form->addRow(tr("Room model"), m_modelRow);
  }
  {
    m_poseRow = new QWidget;
    auto* l = new QHBoxLayout{m_poseRow};
    l->setContentsMargins(0, 0, 0, 0);
    const QString pos[3]{QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("z")};
    for(int i = 0; i < 3; i++)
    {
      auto* sb = new QDoubleSpinBox;
      sb->setRange(-1000., 1000.);
      sb->setDecimals(3);
      sb->setSingleStep(0.05);
      sb->setSuffix(tr(" m"));
      sb->setToolTip(tr("Camera %1 in the room's frame").arg(pos[i]));
      sb->setAccessibleName(tr("Camera position %1").arg(pos[i]));
      m_camPos[i] = sb;
      l->addWidget(sb);
    }
    const QString rot[3]{tr("pitch"), tr("yaw"), tr("roll")};
    for(int i = 0; i < 3; i++)
    {
      auto* sb = new QDoubleSpinBox;
      sb->setRange(-180., 180.);
      sb->setDecimals(2);
      sb->setSuffix(QStringLiteral("°"));
      sb->setToolTip(tr("Camera %1 in the room's frame (0 0 0: level, facing -Z)").arg(rot[i]));
      sb->setAccessibleName(tr("Camera %1").arg(rot[i]));
      m_camRot[i] = sb;
      l->addWidget(sb);
    }
    form->addRow(tr("Camera pose (position, rotation)"), m_poseRow);
    m_refinePose = new QCheckBox;
    m_refinePose->setToolTip(
        tr("Starting from the pose above, find the camera's pose in the room for which "
           "each output's surface points are those of one projector; the pose fields "
           "then take it. Works with Recompute"));
    form->addRow(tr("Refine the camera pose"), m_refinePose);
  }
  connect(m_geometry, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
    updateGeometryRows();
  });

  m_black = new QCheckBox;
  m_black->setToolTip(
      tr("Raise every projector's black to the brightest one, so that the overlaps do "
         "not show darker: the whole picture loses contrast. Off, only the blend "
         "weights are applied"));
  form->addRow(tr("Even out the black"), m_black);
  m_photometric = new QCheckBox;
  m_photometric->setToolTip(
      tr("Also measure the projector response and the surface colour, with a colour "
         "camera (e.g. Cam:/rgb aligned to depth) whose exposure can be set"));
  form->addRow(tr("Photometric"), m_photometric);

  auto* buttons = new QHBoxLayout;
  m_refreshBtn = new QPushButton{tr("Refresh")};
  m_startBtn = new QPushButton{tr("Start")};
  m_abortBtn = new QPushButton{tr("Abort")};
  m_recomputeBtn = new QPushButton{tr("Recompute")};
  m_recomputeBtn->setToolTip(
      tr("Redo the maps from the last measurement of these outputs (plane completion, "
         "room, warp, points and blend) with the current options, without projecting "
         "the patterns again"));
  buttons->addWidget(m_refreshBtn);
  buttons->addWidget(m_startBtn);
  buttons->addWidget(m_abortBtn);
  buttons->addWidget(m_recomputeBtn);
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
  connect(m_recomputeBtn, &QPushButton::clicked, this, [this] { start(true); });
  connect(m_abortBtn, &QPushButton::clicked, this, &CalibrationWizard::abort);
  connect(m_device, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
    updatePlan();
  });
  connect(m_name, &QLineEdit::editingFinished, this, [this] { updatePlan(); });
  connect(m_mode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int i) {
    m_positions->setEnabled(i == 0);
    m_flat->setEnabled(i == 1);
    updateGeometryRows();
    updatePlan();
  });
  connect(m_display, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int i) {
    m_blend->setEnabled(i == 0);
    m_black->setEnabled(i == 0);
  });
  for(auto* sb : {m_settle, m_stripe, m_frames})
    connect(sb, qOverload<int>(&QSpinBox::valueChanged), this, [this] { updatePlan(); });
  connect(m_photometric, &QCheckBox::toggled, this, [this] { updatePlan(); });

  m_mode->setCurrentIndex(1);
  loadOptions();
  m_positions->setEnabled(m_mode->currentIndex() == 0);
  m_flat->setEnabled(m_mode->currentIndex() == 1);
  updateGeometryRows();
  m_blend->setEnabled(m_display->currentIndex() == 0);
  m_black->setEnabled(m_display->currentIndex() == 0);
  m_timer.setInterval(15);
  m_timer.setTimerType(Qt::PreciseTimer);
  connect(&m_timer, &QTimer::timeout, this, &CalibrationWizard::poll);
  {
    auto& list = doc.context().plugin<Explorer::DeviceDocumentPlugin>().list();
    auto refresh = [this] {
      if(!busy())
        refreshDevices();
    };
    connect(&list, &Device::DeviceList::deviceAdded, this, refresh, Qt::QueuedConnection);
    connect(&list, &Device::DeviceList::deviceRemoved, this, refresh, Qt::QueuedConnection);
  }
  refreshDevices();
  updateButtons();
}

CalibrationWizard::~CalibrationWizard()
{
  if(busy())
    m_run->abort(tr("The calibration window was closed"));
  m_run.reset();
  releaseCameras();
}

void CalibrationWizard::updateGeometryRows()
{
  const bool viewer = m_mode->currentIndex() == 1;
  const int g = m_geometry->currentIndex();
  m_geometry->setEnabled(viewer);
  m_boxRow->setEnabled(viewer && g == 1);
  m_modelRow->setEnabled(viewer && g == 2);
  m_poseRow->setEnabled(viewer && g != 0);
  m_refinePose->setEnabled(viewer && g != 0);
}

void CalibrationWizard::keepStreaming(const QStringList& devices, bool on)
{
  auto* ctx = context();
  if(!ctx)
    return;
  auto* exec = ctx->findPlugin<Execution::DocumentPlugin>();
  const bool playing = exec && exec->isPlaying();
  auto& plug = ctx->plugin<Explorer::DeviceDocumentPlugin>();
  for(const auto& name : devices)
  {
    auto* d = plug.list().findDevice(name);
    auto* od = d ? d->getDevice() : nullptr;
    if(!od)
    {
      m_streams.erase(name);
      continue;
    }
    if(auto* video = dynamic_cast<Gfx::video_texture_input_protocol*>(&od->get_protocol()))
    {
      auto& hold = m_streams[name];
      if(!on)
        hold.reset();
      else if(!hold || hold->expired())
        hold = video->keepStreaming();
      continue;
    }
    if(on && !playing && !m_started.contains(name))
    {
      od->get_protocol().start_execution();
      m_started.push_back(name);
    }
    else if(!on && m_started.contains(name))
    {
      if(!playing)
        od->get_protocol().stop_execution();
      m_started.removeAll(name);
    }
  }
}

void CalibrationWizard::releaseCameras()
{
  QStringList devices = m_started;
  for(const auto& [name, hold] : m_streams)
    devices.push_back(name);
  keepStreaming(devices, false);
  m_streams.clear();
  m_started.clear();
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
  releaseCameras();
  QDialog::done(r);
}

void CalibrationWizard::detach()
{
  m_timer.stop();
  if(busy())
    m_run->abort(tr("The document was closed"));
  m_run.reset();
  releaseCameras();
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
  m_recomputeBtn->setEnabled(!active && m_doc && m_device->count() > 0);
  m_refreshBtn->setEnabled(!active && m_doc);
  m_options->setEnabled(!active);
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
  const auto sel = parseOutputSelection(m_outputs->text(), int(physical.outputs.size()));
  return planCalibration(
      physical, QDir{documentFolder()}.absoluteFilePath(m_name->text()), sel,
      m_stripe->value(), 4);
}

StructuredLightSession::Settings CalibrationWizard::sessionSettings() const
{
  StructuredLightSession::Settings s;
  const bool depth = m_mode->currentIndex() == 0;
  s.mode = depth ? StructuredLightSession::DepthCamera : StructuredLightSession::ViewerCamera;
  s.settleMs = m_settle->value();
  s.fov = float(m_fov->value());
  s.lens.model = CameraLens::Model(m_lens->currentIndex());
  {
    const auto parts = m_lensK->text().split(QRegularExpression(QStringLiteral("[\\s,;]+")), Qt::SkipEmptyParts);
    for(int i = 0; i < 5 && i < parts.size(); i++)
      s.lens.k[i] = parts[i].toDouble();
  }
  s.level = float(m_level->value());
  s.minStripe = m_stripe->value();
  s.frames = m_frames->value();
  const bool photometric = m_photometric->isChecked();
  s.responseLevels = photometric ? 8 : 0;
  s.compensation = photometric;
  s.captureExposure = float(m_exposure->value());
  return s;
}

QString CalibrationWizard::documentFolder() const
{
  if(m_doc)
    if(const QString f = m_doc->metadata().fileName(); !f.isEmpty() && QFileInfo{f}.isAbsolute())
      return QFileInfo{f}.absolutePath();
  return QDir{QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)}.filePath(
      QStringLiteral("score calibration"));
}

QString CalibrationWizard::optionsGroup() const
{
  const QString f = m_doc ? m_doc->metadata().fileName() : QString{};
  const QByteArray key
      = QCryptographicHash::hash(QFileInfo{f}.absoluteFilePath().toUtf8(), QCryptographicHash::Sha1)
            .toHex()
            .left(16);
  return QStringLiteral("Threedim/ProjectorCalibration/") + QString::fromLatin1(key);
}

void CalibrationWizard::loadOptions()
{
  QSettings q;
  q.beginGroup(optionsGroup());
  if(q.contains("Mode"))
    m_mode->setCurrentIndex(std::clamp(q.value("Mode").toInt(), 0, 1));
  if(q.contains("Name"))
    m_name->setText(q.value("Name").toString());
  m_outputs->setText(q.value("Outputs").toString());
  m_alignTo->setText(q.value("AlignTo").toString());
  m_settle->setValue(q.value("Settle", m_settle->value()).toInt());
  m_fov->setValue(q.value("Fov", m_fov->value()).toDouble());
  m_lens->setCurrentIndex(std::clamp(q.value("Lens", 0).toInt(), 0, 2));
  m_lensK->setText(q.value("LensK").toString());
  m_estimateLens->setChecked(q.value("EstimateLens", false).toBool());
  m_level->setValue(q.value("Level", m_level->value()).toDouble());
  m_stripe->setValue(q.value("MinStripe", m_stripe->value()).toInt());
  m_frames->setValue(q.value("Frames", m_frames->value()).toInt());
  m_exposure->setValue(q.value("Exposure", m_exposure->value()).toDouble());
  m_blend->setChecked(q.value("Blend", m_blend->isChecked()).toBool());
  m_display->setCurrentIndex(std::clamp(q.value("Display", 0).toInt(), 0, 1));
  m_flat->setChecked(q.value("FlatSurfaces", m_flat->isChecked()).toBool());
  m_black->setChecked(q.value("EvenBlack", false).toBool());
  m_geometry->setCurrentIndex(std::clamp(q.value("Geometry", 0).toInt(), 0, 2));
  for(int i = 0; i < 6; i++)
    m_box[i]->setValue(q.value(QStringLiteral("Box%1").arg(i), 0.).toDouble());
  m_model->setText(q.value("Model").toString());
  m_refinePose->setChecked(q.value("RefinePose", false).toBool());
  for(int i = 0; i < 3; i++)
  {
    m_camPos[i]->setValue(q.value(QStringLiteral("CameraPos%1").arg(i), 0.).toDouble());
    m_camRot[i]->setValue(q.value(QStringLiteral("CameraRot%1").arg(i), 0.).toDouble());
  }
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
  q.beginGroup(optionsGroup());
  q.setValue("Mode", m_mode->currentIndex());
  q.setValue("Name", m_name->text());
  q.setValue("Outputs", m_outputs->text());
  q.setValue("AlignTo", m_alignTo->text());
  q.setValue("Settle", m_settle->value());
  q.setValue("Fov", m_fov->value());
  q.setValue("Lens", m_lens->currentIndex());
  q.setValue("LensK", m_lensK->text());
  q.setValue("EstimateLens", m_estimateLens->isChecked());
  q.setValue("Level", m_level->value());
  q.setValue("MinStripe", m_stripe->value());
  q.setValue("Frames", m_frames->value());
  q.setValue("Exposure", m_exposure->value());
  q.setValue("Blend", m_blend->isChecked());
  q.setValue("Display", m_display->currentIndex());
  q.setValue("FlatSurfaces", m_flat->isChecked());
  q.setValue("EvenBlack", m_black->isChecked());
  q.setValue("Geometry", m_geometry->currentIndex());
  for(int i = 0; i < 6; i++)
    q.setValue(QStringLiteral("Box%1").arg(i), m_box[i]->value());
  q.setValue("Model", m_model->text());
  q.setValue("RefinePose", m_refinePose->isChecked());
  for(int i = 0; i < 3; i++)
  {
    q.setValue(QStringLiteral("CameraPos%1").arg(i), m_camPos[i]->value());
    q.setValue(QStringLiteral("CameraRot%1").arg(i), m_camRot[i]->value());
  }
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
                               st.responseLevels, st.compensation,
                               responseExposures(st.captureExposure), 0,
                               st.mixLevels)
                               .size());
    QString s = tr("%1 %2 of %3x%4, %5 patterns each, about %6 s")
                    .arg(p.projectors)
                    .arg(m_display->currentIndex() == 1 ? tr("screen(s)") : tr("projector(s)"))
                    .arg(p.projectorSize.width())
                    .arg(p.projectorSize.height())
                    .arg(p.patternsPerProjector + extras)
                    .arg(int(estimateMs(p, st, responseExposures(st.captureExposure)) / 1000.));
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

std::vector<float> CalibrationWizard::responseExposures(float captureExposure) const
{
  std::vector<float> out;
  if(m_photometric->isChecked() && captureExposure > 0.f)
    for(float f : {0.125f, 0.25f, 0.5f, 1.f, 2.f, 4.f})
      out.push_back(captureExposure * f);
  return out;
}

double CalibrationWizard::estimateMs(
    const CalibrationPlan& plan, const StructuredLightSession::Settings& s,
    const std::vector<float>& exposures) const
{
  CaptureTiming t;
  t.settleMs = s.settleMs;
  t.framesToAverage = s.frames;
  t.exposureSettleMs = s.exposureSettleMs;
  for(int k = 0; k < plan.projectors; k++)
  {
    const auto extras = StructuredLightCore::makeExtras(
        s.responseLevels, s.compensation, k == 0 ? exposures : std::vector<float>{}, k,
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
  PostTiming post;
  post.blend = m_display->currentIndex() == 0 && m_blend->isChecked();
  post.blackCompensation = m_black->isChecked();
  post.completePlanes = s.mode == StructuredLightSession::ViewerCamera && m_flat->isChecked();
  return estimateCaptureMs(plan, t) + estimatePostMs(plan, post) + 5000.;
}

bool CalibrationWizard::start(bool recompute)
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
  if(setup.plan.projectors == 0 || !setup.plan.error.isEmpty())
  {
    for(const auto& w : setup.plan.warnings)
      log(w);
    if(!setup.plan.error.isEmpty())
      log(setup.plan.error);
    return false;
  }
  if(!recompute)
  {
    QStringList existing;
    for(const auto& n : setup.plan.names)
      if(QFile::exists(n + QStringLiteral(".json")))
        existing.push_back(QFileInfo{n}.fileName());
    if(!existing.isEmpty()
       && QMessageBox::question(
              this, tr("Projector calibration"),
              tr("%1 already exist(s) in %2: replace?")
                  .arg(existing.join(QStringLiteral(", ")))
                  .arg(QFileInfo{setup.plan.names[0]}.absolutePath()))
              != QMessageBox::Yes)
      return false;
  }
  setup.gfx = &ctx->plugin<Gfx::DocumentPlugin>().context;
  QPointer<score::Document> doc = m_doc;
  setup.display = CalibrationRun::multiWindow([doc, deviceName]() -> score::gfx::MultiWindowNode* {
    return doc ? multiWindowNode(doc->context(), deviceName) : nullptr;
  });

  const QString cam = m_camera->currentText();
  setup.camera = [doc, cam]() -> std::optional<Gfx::port_index> {
    if(!doc)
      return std::nullopt;
    if(auto* p = parameterAt(doc->context(), cam))
      return Gfx::textureProducer(*p);
    return std::nullopt;
  };
  if(!recompute && !setup.camera())
  {
    log(tr("%1 is not the texture of a camera device.").arg(cam));
    return false;
  }
  const bool depth = m_mode->currentIndex() == 0;
  if(depth)
  {
    const QString pos = m_positions->currentText();
    setup.positions = [doc, pos]() -> std::optional<Gfx::port_index> {
      if(!doc || pos.isEmpty())
        return std::nullopt;
      if(auto* p = parameterAt(doc->context(), pos))
        return Gfx::textureProducer(*p);
      return std::nullopt;
    };
  }

  m_cameraDevices = QStringList{cam.section(':', 0, 0)};
  if(depth && !m_positions->currentText().isEmpty())
    if(const QString d = m_positions->currentText().section(':', 0, 0);
       !m_cameraDevices.contains(d))
      m_cameraDevices.push_back(d);
  {
    QPointer<CalibrationWizard> self = this;
    setup.keepStreaming = [self, devices = m_cameraDevices](bool on) {
      if(self)
        self->keepStreaming(devices, on);
    };
    if(auto* d = ctx->plugin<Explorer::DeviceDocumentPlugin>().list().findDevice(m_cameraDevices[0]))
      setup.expectedCameraSize = Gfx::cameraDeviceSize(d->settings().deviceSpecificSettings);
  }

  setup.settings = sessionSettings();
  auto& s = setup.settings;
  const bool photometric = m_photometric->isChecked();
  {
    const QString dev = cam.section(':', 0, 0);
    const QString exposure = dev + QStringLiteral(":/controls/color/exposure");
    const QString autoExposure = dev + QStringLiteral(":/controls/color/auto_exposure");
    auto* e = parameterAt(*ctx, exposure);
    auto* ae = parameterAt(*ctx, autoExposure);
    if(photometric && cam.endsWith(QStringLiteral(":/ir")))
      log(tr("Photometric calibration needs a colour camera, e.g. %1:/rgb.").arg(dev));
    if(photometric && e && !(s.captureExposure > 0.f))
      if(const float current = ossia::convert<float>(e->value()); current > 0.f)
        s.captureExposure = current;
    setup.run.exposures = responseExposures(s.captureExposure);
    if(e)
    {
      if(s.captureExposure > 0.f)
        setup.originalExposure = ossia::convert<float>(e->value());
      setup.setExposure = [doc, exposure](float v) {
        if(doc)
          if(auto* p = parameterAt(doc->context(), exposure))
            p->push_value(v);
      };
    }
    if(ae)
    {
      setup.originalAutoExposure = ossia::convert<bool>(ae->value());
      setup.setAutoExposure = [doc, autoExposure](bool v) {
        if(doc)
          if(auto* p = parameterAt(doc->context(), autoExposure))
            p->push_value(v);
      };
    }
    if(!e && (s.captureExposure > 0.f || photometric))
      log(tr("No colour exposure control on %1 (%2): the exposure is not set, and the "
             "camera is assumed linear. Turn its auto exposure and auto white balance "
             "off.")
              .arg(dev, exposure));
  }
  auto& post = setup.post;
  post.screens = m_display->currentIndex() == 1;
  post.blend = m_blend->isChecked() && !post.screens;
  post.completePlanes = !depth && m_flat->isChecked();
  post.estimateLens = post.completePlanes && m_estimateLens->isChecked();
  post.blackCompensation = post.blend && m_black->isChecked();
  if(!depth)
  {
    post.geometry = CalibrationPostProcess::Geometry(m_geometry->currentIndex());
    for(int i = 0; i < 6; i++)
      post.box[i] = float(m_box[i]->value());
    post.model = m_model->text().isEmpty()
                     ? QString{}
                     : QDir{documentFolder()}.absoluteFilePath(m_model->text());
    for(int i = 0; i < 3; i++)
    {
      post.cameraPosition[i] = float(m_camPos[i]->value());
      post.cameraRotation[i] = float(m_camRot[i]->value());
    }
    post.refinePose = m_refinePose->isChecked();
    if(!m_alignTo->text().trimmed().isEmpty())
      for(int out : parseOutputSelection(m_alignTo->text(), int(ws->outputs.size())))
      {
        const auto it = std::find(setup.plan.outputs.begin(), setup.plan.outputs.end(), out);
        if(it == setup.plan.outputs.end())
        {
          log(tr("- output %1 is not measured in this run: it cannot be aligned to").arg(out + 1));
          continue;
        }
        const QString prev = ws->outputs[out].maps.calibration;
        if(prev.isEmpty())
        {
          log(tr("- output %1 has no calibration to align to").arg(out + 1));
          continue;
        }
        post.anchors.push_back({int(it - setup.plan.outputs.begin()), prev});
        log(tr("- aligning to the earlier calibration of output %1 (%2)").arg(out + 1).arg(prev));
      }
    if(post.geometry == CalibrationPostProcess::Geometry::Box
       && std::all_of(post.box.begin(), post.box.end(), [](float d) { return d <= 0.f; }))
    {
      log(tr("Room box: give the distances from the camera to the walls and the floor."));
      return false;
    }
    if(post.geometry == CalibrationPostProcess::Geometry::Model && post.model.isEmpty())
    {
      log(tr("Room model: pick an .obj or .ply file."));
      return false;
    }
  }
  post.projection = ws->canvas.projection;
  for(int k = 0; k < (int)setup.plan.outputs.size(); k++)
    post.sourceRects.push_back(ws->outputs[setup.plan.outputs[k]].sourceRect);
  if(post.blend)
  {
    for(int i = 0; i < (int)ws->outputs.size(); i++)
    {
      const auto it = std::find(setup.plan.outputs.begin(), setup.plan.outputs.end(), i);
      if(it != setup.plan.outputs.end())
        post.blendCalibrations.push_back(
            setup.plan.names[int(it - setup.plan.outputs.begin())] + QStringLiteral(".json"));
      else if(const auto& c = ws->outputs[i].maps.calibration; !c.isEmpty())
        post.blendCalibrations.push_back(c);
      else
        log(tr("- output %1 has no calibration: the blend leaves it out").arg(i + 1));
    }
  }
  if(!depth && ws->canvas.projection != Gfx::CanvasProjection::Equirectangular)
    log(tr("The canvas projection of %1 is not Equirectangular: no warp maps are made "
           "from the viewing directions.")
            .arg(deviceName));

  m_estimateMs = estimateMs(setup.plan, s, setup.run.exposures);
  const auto plan = setup.plan;
  m_run = std::make_unique<CalibrationRun>(std::move(setup));
  m_runDevice = deviceName;
  m_runStartMs = QDateTime::currentMSecsSinceEpoch();
  const bool ok = recompute ? m_run->recompute(double(m_runStartMs))
                            : m_run->start(double(m_runStartMs));
  m_recomputing = recompute;
  for(const auto& l : m_run->takeLog())
    log(l);
  if(!ok)
  {
    m_run.reset();
    updateButtons();
    return false;
  }
  const QString what = m_display->currentIndex() == 1 ? tr("screen(s)") : tr("projector(s)");
  if(recompute)
    log(tr("Recomputing %1 %2 of %3 from their measurements.")
            .arg(plan.projectors)
            .arg(what)
            .arg(deviceName));
  else
    log(tr("Calibrating %1 %2 of %3: about %4 s.")
            .arg(plan.projectors)
            .arg(what)
            .arg(deviceName)
            .arg(int(m_estimateMs / 1000.)));
  for(const auto& w : plan.warnings)
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
  const auto& setup = m_run->setup();
  const auto names = setup.plan.names;
  const auto outputs = setup.plan.outputs;
  const bool geometry = setup.post.geometry != CalibrationPostProcess::Geometry::Directions
                        && setup.post.viewer;
  auto cameraPositionUsed = setup.post.cameraPosition;
  const auto outcome = m_run->outcome();
  if(outcome && outcome->pose)
  {
    const auto& pose = *outcome->pose;
    for(int i = 0; i < 3; i++)
    {
      m_camPos[i]->setValue(pose[i]);
      m_camRot[i]->setValue(pose[i + 3]);
      cameraPositionUsed[i] = pose[i];
    }
    log(tr("The refined camera pose is now in the pose fields."));
  }
  if(outcome && outcome->lens)
  {
    m_lens->setCurrentIndex(int(outcome->lens->model));
    QStringList k;
    for(double v : outcome->lens->k)
      k.push_back(QString::number(v, 'g', 6));
    m_lensK->setText(k.join(' '));
    m_estimateLens->setChecked(false);
    log(tr("The estimated lens is now in the Lens fields."));
  }
  saveOptions();
  m_run.reset();
  updateButtons();
  const qint64 since = m_recomputing ? 0 : m_runStartMs;
  const auto r = readCalibrationResults(names, since, outputs);
  if(!r.text.isEmpty())
    log(r.text);
  const auto ws = deviceSettings(m_runDevice);
  QStringList notes;
  auto assigned = ws ? assignCalibrations(*ws, names, since, outputs, &notes)
                     : Gfx::WindowSettings{};
  for(const auto& n : notes)
    log(QStringLiteral("- ") + n);
  if(ws && geometry && assigned.canvas.viewer == std::array<float, 3>{0.f, 0.f, 0.f})
  {
    assigned.canvas.viewer = cameraPositionUsed;
    log(tr("Viewer position set to the camera's (%1, %2, %3): move it in the device's "
           "settings or on its /canvas/viewer.")
            .arg(cameraPositionUsed[0])
            .arg(cameraPositionUsed[1])
            .arg(cameraPositionUsed[2]));
  }
  if(ws && geometry && assigned.canvas.projection != Gfx::CanvasProjection::Equirectangular)
    log(tr("The points maps are only used with an Equirectangular canvas projection."));
  if(ws && updateDevice(m_runDevice, assigned))
    log(tr("Done: the outputs of %1 follow %2; a new run updates them live.")
            .arg(m_runDevice, names.join(QStringLiteral(", "))));
  else
    log(tr("Done, but %1 could not be updated: assign %2 in its settings.")
            .arg(m_runDevice, names.join(QStringLiteral(", "))));
}
}

SCORE_DECLARE_ACTION(
    ProjectorCalibration, "&Projector calibration...", Common, QKeySequence::UnknownKey)

namespace Threedim
{
void CalibrationApplicationPlugin::open()
{
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
}

score::GUIElements CalibrationApplicationPlugin::makeGUIElements()
{
  score::GUIElements e;
  auto& menus = context.menus.get();
  auto it = menus.find(score::Menus::View());
  if(it == menus.end())
    return e;
  m_action = new QAction{it->second.menu()};
  m_action->setText(QCoreApplication::translate(
      "Threedim::CalibrationWizard", "Projector calibration..."));
  QObject::connect(m_action, &QAction::triggered, m_action, [this] { open(); });
  it->second.menu()->addAction(m_action);
  e.actions.add<Actions::ProjectorCalibration>(m_action);
  context.actions.condition<score::EnableActionIfDocument>().add<Actions::ProjectorCalibration>();
  return e;
}

void CalibrationApplicationPlugin::on_documentChanged(
    score::Document* olddoc, score::Document* newdoc)
{
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
