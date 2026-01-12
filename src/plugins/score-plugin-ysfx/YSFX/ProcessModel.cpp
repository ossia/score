// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "ProcessModel.hpp"

#include "YSFX/ApplicationPlugin.hpp"
#include "YSFX/ProcessMetadata.hpp"

#include <State/Expression.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/PresetHelpers.hpp>

#include <Media/Effect/Settings/Model.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/model/Identifier.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/DeleteAll.hpp>
#include <score/tools/File.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <core/document/Document.hpp>

#include <ossia/detail/disable_fpe.hpp>

#include <QApplication>
#include <QCursor>
#include <QDebug>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QQmlComponent>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>

#include <wobjectimpl.h>

#include <bitset>
#include <vector>
W_OBJECT_IMPL(YSFX::ProcessModel)
namespace YSFX
{
bool ProcessModel::hasExternalUI() const noexcept
{
  if(!this->fx)
    return false;

  return ysfx_has_section(this->fx.get(), ysfx_section_gfx);
}

ProcessModel::ProcessModel(
    const TimeVal& duration, const QString& data, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{
          duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
{
  setInitialScript(data);
  metadata().setInstanceName(*this);
}

ProcessModel::~ProcessModel()
{
  if(m_bank)
    ysfx_bank_free(m_bank);

  if(externalUI)
  {
    auto w = reinterpret_cast<Window*>(externalUI);
    delete w;
  }
}

bool ProcessModel::validate(const QString& txt) const noexcept
{
  return true;
}

void ProcessModel::setInitialScript(const QString& script)
{
  ossia::disable_fpe();
  auto path_to_jsfx = script.toStdString();
  auto& config
      = score::GUIAppContext().applicationPlugin<YSFX::ApplicationPlugin>().config;
  ysfx_guess_file_roots(config.get(), path_to_jsfx.c_str());

  fx.reset(ysfx_new(config.get()), ysfx_u_deleter{});
  if(!ysfx_load_file(fx.get(), path_to_jsfx.c_str(), 0))
  {
    qDebug() << "ysfx: could not load " << script;
    return;
  }

  m_jsfx_path = script;

  // Read the file content and store it for editing
  if(QFile f{script}; f.open(QIODevice::ReadOnly))
  {
    m_text = QString::fromUtf8(f.readAll());
  }

  uint32_t compile_opts = 0;
  if(!ysfx_compile(fx.get(), compile_opts))
    return;

  ysfx_init(fx.get());
  ysfx_process_double(fx.get(), 0, 0, 0, 0, 0);
  auto name = ysfx_get_name(fx.get());
  metadata().setName(name);

  recreatePorts();

  if(auto bank = ysfx_get_bank_path(this->fx.get()))
  {
    m_bank = ysfx_load_bank(bank);
  }

  programChanged();
}

void ProcessModel::recreatePorts()
{
  if(ysfx_get_num_inputs(fx.get()) > 0)
  {
    auto inl = new Process::AudioInlet{"Audio In", Id<Process::Port>{0}, this};
    this->m_inlets.push_back(inl);
  }
  this->m_inlets.push_back(
      new Process::MidiInlet{"MIDI In", Id<Process::Port>{1}, this});

  if(ysfx_get_num_outputs(fx.get()) > 0)
  {
    auto outl = new Process::AudioOutlet{"Audio Out", Id<Process::Port>{2}, this};
    outl->setPropagate(true);
    this->m_outlets.push_back(outl);
  }
  this->m_outlets.push_back(
      new Process::MidiOutlet{"MIDI Out", Id<Process::Port>{3}, this});

  this->m_sliderBeingChanged.reset();

  for(uint32_t i = 0; i < ysfx_max_sliders; ++i)
  {
    if(!ysfx_slider_exists(fx.get(), i))
      continue;

    auto id = Id<Process::Port>(4 + i);
    const bool is_visible = ysfx_slider_is_initially_visible(fx.get(), i);
    QString slider_name = ysfx_slider_get_name(fx.get(), i);
    if(ysfx_slider_is_enum(fx.get(), i) || ysfx_slider_is_path(fx.get(), i))
    {
      std::vector<std::pair<QString, ossia::value>> values;
      ossia::value init;

      uint32_t count = ysfx_slider_get_enum_size(fx.get(), i);
      std::vector<const char*> names(count);
      ysfx_slider_get_enum_names(fx.get(), i, names.data(), count);
      values.reserve(count);
      int k = 0;
      for(const char* val : names)
        values.push_back({val, k++});

      auto slider = new Process::ComboBox{values, 0, slider_name, id, this};

      connect(
          slider, &Process::ComboBox::valueChanged, this,
          [this, i](const ossia::value& v) {
        if(!this->fx)
          return;
        if(this->m_sliderBeingChanged.test(i))
          return;
        this->m_sliderBeingChanged.set(i, true);

        auto old_val = (int)ysfx_slider_get_value(this->fx.get(), i);
        auto new_val = ossia::convert<int>(v);
        if(old_val != new_val)
        {
#if __has_include(<ysfx-s.h>)
          ysfx_slider_set_value(this->fx.get(), i, new_val, true);
#else
          ysfx_slider_set_value(this->fx.get(), i, new_val);
#endif
        }
        this->m_sliderBeingChanged.set(i, false);
      });

      this->m_inlets.push_back(slider);
    }
    else
    {
      ysfx_slider_range_t range{};
      ysfx_slider_get_range(fx.get(), i, &range);

      auto slider = new Process::FloatSlider(
          range.min, range.max, range.def, slider_name, id, this);
      // TODO increment
      connect(
          slider, &Process::FloatSlider::valueChanged, this,
          [this, i](const ossia::value& v) {
        if(!this->fx)
          return;
        if(this->m_sliderBeingChanged.test(i))
          return;
        this->m_sliderBeingChanged.set(i, true);

        auto old_val = ysfx_slider_get_value(this->fx.get(), i);
        auto new_val = ossia::convert<float>(v);
        if(old_val != new_val)
        {
#if __has_include(<ysfx-s.h>)
          ysfx_slider_set_value(this->fx.get(), i, new_val, true);
#else
          ysfx_slider_set_value(this->fx.get(), i, new_val);
#endif
        }
        this->m_sliderBeingChanged.set(i, false);
      });

      this->m_inlets.push_back(slider);
    }
  }
}
QString ProcessModel::script() const noexcept
{
  return m_jsfx_path;
}

std::vector<Process::Preset> ProcessModel::builtinPresets() const noexcept
{
  std::vector<Process::Preset> presets;
  if(m_bank)
  {
    for(std::size_t i = 0; i < m_bank->preset_count; i++)
    {
      Process::Preset p;
      p.key.key = this->static_concreteKey();
      p.key.effect = m_jsfx_path;
      p.name = m_bank->presets[i].name;
      p.data = QStringLiteral(R"({ "ProgramIndex": %1 } )").arg(i).toLatin1();
      presets.push_back(p);
    }
  }
  return presets;
}

void ProcessModel::loadPreset(const Process::Preset& preset)
{
  if(!m_bank)
    return;

  const rapidjson::Document doc = readJson(preset.data);
  if(!doc.IsObject())
    return;
  auto obj = doc.GetObject();

  if(auto it = obj.FindMember("ProgramIndex"); it != obj.MemberEnd())
  {
    auto idx = JsonValue{it->value}.toInt();
    ysfx_load_state(this->fx.get(), this->m_bank->presets[idx].state);
  }
}

QString ProcessModel::effect() const noexcept
{
  return m_text;
}

Process::Preset ProcessModel::savePreset() const noexcept
{
  return Process::saveScriptProcessPreset(*this, this->m_text);
}

Process::ScriptChangeResult ProcessModel::setText(const QString& text)
{
  if(text == m_text)
    return {};

  m_text = text;
  auto res = reload();
  scriptChanged(m_text);
  return res;
}

Process::ScriptChangeResult ProcessModel::reload()
{
  Process::ScriptChangeResult res;
  ossia::disable_fpe();

  if(m_text.isEmpty())
    return res;

  // Create a temporary file for the edited code
  static QTemporaryDir tempDir;
  if(!tempDir.isValid())
  {
    qDebug() << "ysfx: could not create temp directory";
    return res;
  }

  // Generate a unique filename
  QString tempFilePath = tempDir.filePath(
      QStringLiteral("ysfx_%1.jsfx").arg(reinterpret_cast<quintptr>(this), 0, 16));

  // Write the text to the temp file
  {
    QFile tempFile{tempFilePath};
    if(!tempFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
      qDebug() << "ysfx: could not write temp file" << tempFilePath;
      return res;
    }
    tempFile.write(m_text.toUtf8());
  }

  // Create a new config with the proper roots
  ysfx_config_u config{ysfx_config_new()};
  ysfx_register_builtin_audio_formats(config.get());

  // Set import and data roots from the original file
  ysfx_guess_file_roots(config.get(), m_jsfx_path.toStdString().c_str());

  // Create a new fx instance
  auto new_fx = std::shared_ptr<ysfx_t>(ysfx_new(config.get()), ysfx_u_deleter{});

  auto tempPath = tempFilePath.toStdString();
  if(!ysfx_load_file(new_fx.get(), tempPath.c_str(), 0))
  {
    qDebug() << "ysfx: could not load temp file" << tempFilePath;
    return res;
  }

  uint32_t compile_opts = 0;
  if(!ysfx_compile(new_fx.get(), compile_opts))
  {
    qDebug() << "ysfx: compilation failed";
    return res;
  }

  res.valid = true;

  // Update the effect
  score::delete_later<Process::Inlets>& inlets_to_clear = res.inlets;
  score::delete_later<Process::Outlets>& outlets_to_clear = res.outlets;

  // Clear old ports
  score::clearAndDeleteLater(m_inlets, inlets_to_clear);
  score::clearAndDeleteLater(m_outlets, outlets_to_clear);

  // Replace the fx
  fx = std::move(new_fx);

  ysfx_init(fx.get());
  ysfx_process_double(fx.get(), 0, 0, 0, 0, 0);
  auto name = ysfx_get_name(fx.get());
  metadata().setName(name);

  recreatePorts();

  programChanged();

  return res;
}

Window::Window(ProcessModel& e, const score::DocumentContext& ctx, QWidget* parent)
    : PluginWindow{ctx.app.settings<Media::Settings::Model>().getVstAlwaysOnTop(), parent}
    , m_model{&e}
{
  fx = e.fx;
  e.externalUIVisible(true);
  const_cast<QWidget*&>(e.externalUI) = this;

  // Enable drag and drop
  setAcceptDrops(true);

  rebuild();
  update();

  connect(
      &ctx.coarseUpdateTimer, &QTimer::timeout, this, &Window::refreshTimer,
      Qt::UniqueConnection);
  connect(
      &e, &ProcessModel::programChanged, this, &Window::rebuild, Qt::UniqueConnection);
}

Window::~Window()
{
  if(m_model)
  {
    const_cast<QWidget*&>(m_model->externalUI) = nullptr;
    m_model->externalUIVisible(false);
  }
}

void Window::rebuild()
{
  if(!m_model)
  {
    this->fx.reset();
    return;
  }

  this->fx = m_model->fx;
  if(!this->fx)
    return;

  m_retina = ysfx_gfx_wants_retina(fx.get());

  uint32_t dim[2];
  ysfx_get_gfx_dim(fx.get(), dim);

  if(dim[0] == 0)
    dim[0] = 640;
  if(dim[1] == 0)
    dim[1] = 480;

  setGeometry(0, 0, dim[0], dim[1]);

  conf.user_data = this;

  if(m_retina)
  {
    conf.scale_factor = this->devicePixelRatioF();
    conf.pixel_width = dim[0] * conf.scale_factor;
    conf.pixel_height = dim[1] * conf.scale_factor;
    m_frame = QImage(
        QSize(dim[0] * conf.scale_factor, dim[1] * conf.scale_factor),
        QImage::Format_RGBX8888);
    m_frame.setDevicePixelRatio(conf.scale_factor);
    m_frame.fill(0);
  }
  else
  {
    conf.scale_factor = 1.0;
    conf.pixel_width = dim[0];
    conf.pixel_height = dim[1];
    m_frame = QImage(QSize(dim[0], dim[1]), QImage::Format_RGBX8888);
    m_frame.fill(0);
  }
  conf.pixels = m_frame.bits();

  // Show menu callback - parse menu spec and display a QMenu
  conf.show_menu = [](void* user_data, const char* menu_spec, int32_t xpos,
                      int32_t ypos) -> int32_t {
    auto* self = static_cast<Window*>(user_data);
    if(!self || !menu_spec || !menu_spec[0])
      return 0;

    // Needed because some plugins blast show_menu events on each mousemove
    if(self->m_mouseHeldMenuEvent++ >= 1)
      return 0;

    ysfx_menu_u parsed_menu{ysfx_parse_menu(menu_spec)};
    if(!parsed_menu || parsed_menu->insn_count == 0)
      return 0;

    // Build QMenu from parsed menu
    QMenu menu;
    std::vector<QMenu*> menu_stack;
    menu_stack.push_back(&menu);

    for(uint32_t i = 0; i < parsed_menu->insn_count; ++i)
    {
      const auto& insn = parsed_menu->insns[i];

      switch(insn.opcode)
      {
        case ysfx_menu_item: {
          QAction* action = menu_stack.back()->addAction(QString::fromUtf8(insn.name));
          action->setData(static_cast<int>(insn.id));
          action->setEnabled(!(insn.item_flags & ysfx_menu_item_disabled));
          action->setCheckable(insn.item_flags & ysfx_menu_item_checked);
          action->setChecked(insn.item_flags & ysfx_menu_item_checked);
          break;
        }
        case ysfx_menu_separator: {
          menu_stack.back()->addSeparator();
          break;
        }
        case ysfx_menu_sub: {
          QMenu* submenu = menu_stack.back()->addMenu(QString::fromUtf8(insn.name));
          menu_stack.push_back(submenu);
          break;
        }
        case ysfx_menu_endsub: {
          if(menu_stack.size() > 1)
            menu_stack.pop_back();
          break;
        }
      }
    }

    // Show menu at specified position
    QPoint pos = self->mapToGlobal(QPoint(xpos, ypos));
    QAction* selected = menu.exec(pos);

    QMetaObject::invokeMethod(self, [self, xpos, ypos] {
      // Needed to release the mouse
      if(self->fx.get())
      {
        ysfx_gfx_update_mouse(self->fx.get(), 0, xpos, ypos, 0, 0, 0);
      }
    }, Qt::QueuedConnection);
    if(selected)
      return selected->data().toInt();

    return 0;
  };

  conf.set_cursor = [](void* user_data, int32_t cursor) {
    auto* self = static_cast<Window*>(user_data);
    if(!self)
      return;

    // Windows cursor IDs: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setsystemcursor
    Qt::CursorShape shape = Qt::ArrowCursor;
    switch(cursor)
    {
      case -1:
      case 0:
      case 32512: // Windows
        shape = Qt::ArrowCursor;
        break;
      case 1:
      case 32513:
        shape = Qt::IBeamCursor;
        break;
      case 2:
      case 32515:
        shape = Qt::CrossCursor;
        break;
      case 3:
      case 32649:
        shape = Qt::PointingHandCursor;
        break;
      case 4:
      case 32645:
        shape = Qt::SizeVerCursor;
        break;
      case 5:
      case 32644:
        shape = Qt::SizeHorCursor;
        break;
      case 6:
      case 32642:
        shape = Qt::SizeFDiagCursor;
        break;
      case 7:
      case 32643:
        shape = Qt::SizeBDiagCursor;
        break;
      case 8:
      case 32646:
        shape = Qt::SizeAllCursor;
        break;
      case 9:
      case 32648:
        shape = Qt::ForbiddenCursor;
        break; // No
      case 10:
      case 32514:
        shape = Qt::WaitCursor;
        break; // Wait
      case 11:
      case 32650:
        shape = Qt::BusyCursor;
        break; // Busy
      default:
        shape = Qt::ArrowCursor;
        break;
    }
    self->setCursor(QCursor(shape));
  };

  conf.get_drop_file = [](void* user_data, int32_t index) -> const char* {
    auto* self = static_cast<Window*>(user_data);
    if(!self)
      return nullptr;

    // index == -1 means clear the list
    if(index == -1)
    {
      self->m_droppedFiles.clear();
      return nullptr;
    }

    // Return file at index if valid
    if(index >= 0 && static_cast<size_t>(index) < self->m_droppedFiles.size())
      return self->m_droppedFiles[index].c_str();

    return nullptr;
  };

  ysfx_gfx_setup(fx.get(), &conf);
}

void Window::resizeEvent(QResizeEvent* event)
{
  event->accept();
}

void Window::closeEvent(QCloseEvent* event)
{
  event->accept();
}

static uint32_t qt_to_ysfx_mods()
{
  const auto qt_mods = qApp->keyboardModifiers();
  uint32_t mods{};
  mods |= (qt_mods & Qt::ControlModifier) ? ysfx_mod_ctrl : 0;
  mods |= (qt_mods & Qt::ShiftModifier) ? ysfx_mod_shift : 0;
  mods |= (qt_mods & Qt::AltModifier) ? ysfx_mod_alt : 0;
  mods |= (qt_mods & Qt::MetaModifier) ? ysfx_mod_super : 0;
  return mods;
}

static int qt_to_ysfx_key(int k)
{
  int key = -1;
  switch(k)
  {
    case Qt::Key_Backspace:
      key = ysfx_key_backspace;
      break;
    case Qt::Key_Escape:
      key = ysfx_key_escape;
      break;
    case Qt::Key_Delete:
      key = ysfx_key_delete;
      break;

    case Qt::Key_F1:
      key = ysfx_key_f1;
      break;
    case Qt::Key_F2:
      key = ysfx_key_f2;
      break;
    case Qt::Key_F3:
      key = ysfx_key_f3;
      break;
    case Qt::Key_F4:
      key = ysfx_key_f4;
      break;
    case Qt::Key_F5:
      key = ysfx_key_f5;
      break;
    case Qt::Key_F6:
      key = ysfx_key_f6;
      break;
    case Qt::Key_F7:
      key = ysfx_key_f7;
      break;
    case Qt::Key_F8:
      key = ysfx_key_f8;
      break;
    case Qt::Key_F9:
      key = ysfx_key_f9;
      break;
    case Qt::Key_F10:
      key = ysfx_key_f10;
      break;
    case Qt::Key_F11:
      key = ysfx_key_f11;
      break;
    case Qt::Key_F12:
      key = ysfx_key_f12;
      break;

    case Qt::Key_Left:
      key = ysfx_key_left;
      break;
    case Qt::Key_Right:
      key = ysfx_key_right;
      break;
    case Qt::Key_Up:
      key = ysfx_key_up;
      break;
    case Qt::Key_Down:
      key = ysfx_key_down;
      break;

    case Qt::Key_PageUp:
      key = ysfx_key_page_up;
      break;
    case Qt::Key_PageDown:
      key = ysfx_key_page_down;
      break;
    case Qt::Key_Home:
      key = ysfx_key_home;
      break;
    case Qt::Key_End:
      key = ysfx_key_end;
      break;
    case Qt::Key_Insert:
      key = ysfx_key_insert;
      break;
    default:
      break;
  }
  return key;
}

void Window::mousePressEvent(QMouseEvent* event)
{
  this->m_mouseHeldMenuEvent = 0;
  this->m_mouseHeld = true;
  if(this->fx)
  {
    auto qt_mods = qApp->keyboardModifiers();
    uint32_t mods{};
    mods |= (qt_mods & Qt::ControlModifier) ? ysfx_mod_ctrl : 0;
    mods |= (qt_mods & Qt::ShiftModifier) ? ysfx_mod_shift : 0;
    mods |= (qt_mods & Qt::AltModifier) ? ysfx_mod_alt : 0;
    mods |= (qt_mods & Qt::MetaModifier) ? ysfx_mod_super : 0;

    uint32_t buttons{};
    buttons |= event->button() & Qt::LeftButton ? ysfx_button_left : 0;
    buttons |= event->button() & Qt::MiddleButton ? ysfx_button_middle : 0;
    buttons |= event->button() & Qt::RightButton ? ysfx_button_right : 0;
    buttons |= event->buttons() & Qt::LeftButton ? ysfx_button_left : 0;
    buttons |= event->buttons() & Qt::MiddleButton ? ysfx_button_middle : 0;
    buttons |= event->buttons() & Qt::RightButton ? ysfx_button_right : 0;

    int32_t xpos = event->pos().x();
    int32_t ypos = event->pos().y();
    if(m_retina)
    {
      xpos = xpos * this->devicePixelRatioF();
      ypos = ypos * this->devicePixelRatioF();
    }
    ysfx_real wheel{};
    ysfx_real hwheel{};

    ysfx_gfx_update_mouse(fx.get(), mods, xpos, ypos, buttons, wheel, hwheel);
  }

  event->accept();
}

void Window::mouseReleaseEvent(QMouseEvent* event)
{
  this->m_mouseHeld = false;
  if(this->fx)
  {
    auto qt_mods = qApp->keyboardModifiers();
    uint32_t mods{};
    mods |= (qt_mods & Qt::ControlModifier) ? ysfx_mod_ctrl : 0;
    mods |= (qt_mods & Qt::ShiftModifier) ? ysfx_mod_shift : 0;
    mods |= (qt_mods & Qt::AltModifier) ? ysfx_mod_alt : 0;
    mods |= (qt_mods & Qt::MetaModifier) ? ysfx_mod_super : 0;

    uint32_t buttons{};

    int32_t xpos = event->pos().x();
    int32_t ypos = event->pos().y();
    if(m_retina)
    {
      xpos = xpos * this->devicePixelRatioF();
      ypos = ypos * this->devicePixelRatioF();
    }
    ysfx_real wheel{};
    ysfx_real hwheel{};

    ysfx_gfx_update_mouse(fx.get(), mods, xpos, ypos, buttons, wheel, hwheel);
  }
  event->accept();
}

void Window::mouseMoveEvent(QMouseEvent* event)
{
  mousePressEvent(event);
}

void Window::wheelEvent(QWheelEvent* event)
{
  if(this->fx)
  {
    auto qt_mods = qApp->keyboardModifiers();
    uint32_t mods{};
    mods |= (qt_mods & Qt::ControlModifier) ? ysfx_mod_ctrl : 0;
    mods |= (qt_mods & Qt::ShiftModifier) ? ysfx_mod_shift : 0;
    mods |= (qt_mods & Qt::AltModifier) ? ysfx_mod_alt : 0;
    mods |= (qt_mods & Qt::MetaModifier) ? ysfx_mod_super : 0;

    uint32_t buttons{};
    buttons |= event->buttons() & Qt::LeftButton ? ysfx_button_left : 0;
    buttons |= event->buttons() & Qt::MiddleButton ? ysfx_button_middle : 0;
    buttons |= event->buttons() & Qt::RightButton ? ysfx_button_right : 0;

    int32_t xpos = event->position().x();
    int32_t ypos = event->position().y();
    if(m_retina)
    {
      xpos = xpos * this->devicePixelRatioF();
      ypos = ypos * this->devicePixelRatioF();
    }
    ysfx_real wheel{event->angleDelta().y() / 120.};
    ysfx_real hwheel{event->angleDelta().x() / 120.};

    ysfx_gfx_update_mouse(fx.get(), mods, xpos, ypos, buttons, wheel, hwheel);
  }
  event->accept();
}

void Window::keyPressEvent(QKeyEvent* event)
{
  if(this->fx)
  {
    const auto mods = qt_to_ysfx_mods();

    const auto key = qt_to_ysfx_key(event->key());
    if(key != -1)
      ysfx_gfx_add_key(fx.get(), mods, key, true);
  }
  event->accept();
}

void Window::keyReleaseEvent(QKeyEvent* event)
{
  if(this->fx)
  {
    const auto mods = qt_to_ysfx_mods();

    const auto key = qt_to_ysfx_key(event->key());
    if(key != -1)
      ysfx_gfx_add_key(fx.get(), mods, key, false);
  }

  event->accept();
}

void Window::updateState()
{
#if __has_include(<ysfx-s.h>)
  ysfx_gfx_set_window_state(
      fx.get(), this->hasFocus(), this->isVisible(), this->underMouse());
#endif
}

void Window::showEvent(QShowEvent* ev)
{
  updateState();
  return score::PluginWindow::showEvent(ev);
}
void Window::hideEvent(QHideEvent* ev)
{
  updateState();
  return score::PluginWindow::hideEvent(ev);
}
void Window::focusInEvent(QFocusEvent* ev)
{
  updateState();
  return score::PluginWindow::focusInEvent(ev);
}
void Window::focusOutEvent(QFocusEvent* ev)
{
  updateState();
  return score::PluginWindow::focusOutEvent(ev);
}
void Window::paintEvent(QPaintEvent* event)
{
  if(this->fx)
  {
    updateState();

    // logical w / h:
    auto width = this->width();
    auto height = this->height();

    QPainter p{this};
    const auto retina = ysfx_gfx_wants_retina(fx.get());
    if(retina)
    {
      const float host_scale_factor = this->devicePixelRatioF();
      p.setRenderHint(QPainter::RenderHint::SmoothPixmapTransform, true);
      conf.scale_factor = 1.;
      conf.pixel_width = std::floor(width * host_scale_factor);
      conf.pixel_height = std::floor(height * host_scale_factor);
      conf.pixel_stride = std::floor(width * host_scale_factor) * 4;

      if(m_frame.width() != conf.pixel_width || m_frame.height() != conf.pixel_height)
      {
        m_frame = QImage(
            QSize(conf.pixel_width, conf.pixel_height), QImage::Format_RGBX8888);
        m_frame.fill(0);
      }
      m_frame.setDevicePixelRatio(host_scale_factor);
    }
    else
    {
      p.setRenderHint(QPainter::RenderHint::SmoothPixmapTransform, false);
      conf.scale_factor = 1.0;
      conf.pixel_width = width;
      conf.pixel_height = height;
      conf.pixel_stride = width * 4;
      if(m_frame.width() != width || m_frame.height() != height)
      {
        m_frame = QImage(QSize(width, height), QImage::Format_RGBX8888);
        m_frame.fill(0);
      }
      m_frame.setDevicePixelRatio(1.0);
    }
    conf.pixels = m_frame.bits();
    ysfx_gfx_setup(fx.get(), &conf);

    ysfx_gfx_run(fx.get());

    p.drawImage(QPointF{}, this->m_frame.rgbSwapped());
    event->accept();

#if __has_include(<ysfx-s.h>)
    auto y = this->fx.get();
    if(m_mouseHeld)
    {
      if(std::bitset<64> res = ysfx_fetch_slider_automations(y, 0); res.any())
      {
        for(int i = 0; i < 64; i++)
        {
          if(res.test(i))
          {
            // See ProcessModel.hpp around the loop:
            // for (uint32_t i = 0; i < ysfx_max_sliders; ++i)
            int idx = 4 + i;
            if(auto inl = static_cast<Process::ControlInlet*>(
                   m_model->inlet(Id<Process::Port>{idx})))
            {
              if(m_model->m_sliderBeingChanged.test(i))
                return;
              m_model->m_sliderBeingChanged.set(i, true);
              inl->setValue(ysfx_slider_get_value(y, i));
              m_model->m_sliderBeingChanged.set(i, false);
            }
          }
        }
      }
    }
#endif
  }
}

void Window::refreshTimer()
{
  update();
}

void Window::dragEnterEvent(QDragEnterEvent* event)
{
  if(event->mimeData()->hasUrls())
  {
    event->acceptProposedAction();
  }
}

void Window::dragMoveEvent(QDragMoveEvent* event)
{
  if(event->mimeData()->hasUrls())
  {
    event->acceptProposedAction();
  }
}

void Window::dropEvent(QDropEvent* event)
{
  const QMimeData* mimeData = event->mimeData();
  if(mimeData->hasUrls())
  {
    // Clear previous dropped files
    m_droppedFiles.clear();

    // Store all dropped file paths
    for(const QUrl& url : mimeData->urls())
    {
      if(url.isLocalFile())
      {
        m_droppedFiles.push_back(url.toLocalFile().toStdString());
      }
    }

    event->acceptProposedAction();
  }
}
}
