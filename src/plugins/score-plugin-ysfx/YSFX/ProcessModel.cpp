// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "ProcessModel.hpp"

#include "YSFX/ProcessMetadata.hpp"
#include "YSFX/ApplicationPlugin.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/PresetHelpers.hpp>
#include <State/Expression.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/model/Identifier.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/DeleteAll.hpp>
#include <score/tools/File.hpp>

#include <core/document/Document.hpp>

#include <QApplication>
#include <QDebug>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QPainter>
#include <QQmlComponent>

#include <wobjectimpl.h>

#include <vector>
W_OBJECT_IMPL(YSFX::ProcessModel)
namespace YSFX
{
bool ProcessModel::hasExternalUI() const noexcept {
  if(!this->fx)
    return false;

  return ysfx_has_section(this->fx.get(), ysfx_section_gfx);
}

ProcessModel::ProcessModel(
      const TimeVal& duration,
      const QString& data,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{
        duration,
        id,
        Metadata<ObjectKey_k, ProcessModel>::get(),
        parent}
{
  setScript(data);
  metadata().setInstanceName(*this);
}

ProcessModel::~ProcessModel()
{
  if (externalUI)
  {
    auto w = reinterpret_cast<Window*>(externalUI);
    delete w;
  }
}

void ProcessModel::setScript(const QString& script)
{
  auto arr = script.toStdString();
  auto& config = score::GUIAppContext().applicationPlugin<YSFX::ApplicationPlugin>().config;
  ysfx_guess_file_roots(config.get(), arr.c_str());

  fx.reset(ysfx_new(config.get()), ysfx_u_deleter{});
  if (!ysfx_load_file(fx.get(), arr.c_str(), 0))
  {
    qDebug() << "ysfx: could not load " << script;
    return ;
  }

  m_script = script;

  uint32_t compile_opts = 0;
  if (!ysfx_compile(fx.get(), compile_opts))
      return;

  ysfx_init(fx.get());
  ysfx_process_double(fx.get(), 0, 0, 0, 0, 0);
  auto name = ysfx_get_name(fx.get());
  metadata().setName(name);

  if(ysfx_get_num_inputs(fx.get()) > 0)
  {
    auto inl = new Process::AudioInlet{Id<Process::Port>{0}, this};
    this->m_inlets.push_back(inl);
  }
  this->m_inlets.push_back(new Process::MidiInlet{Id<Process::Port>{1}, this});

  if(ysfx_get_num_outputs(fx.get()) > 0)
  {
    auto outl = new Process::AudioOutlet{Id<Process::Port>{2}, this};
    outl->setPropagate(true);
    this->m_outlets.push_back(outl);
  }
  this->m_outlets.push_back(new Process::MidiOutlet{Id<Process::Port>{3}, this});

  {
    for (uint32_t i = 0; i < ysfx_max_sliders; ++i)
    {
      if (!ysfx_slider_exists(fx.get(), i))
          continue;

      auto id = Id<Process::Port>(4 + i);
      const bool is_visible = ysfx_slider_is_initially_visible(fx.get(), i);
      QString name = ysfx_slider_get_name(fx.get(), i);
      if (ysfx_slider_is_enum(fx.get(), i))
      {
        std::vector<std::pair<QString, ossia::value>> values;
        ossia::value init;

        uint32_t count = ysfx_slider_get_enum_size(fx.get(), i);
        std::vector<const char *> names(count);
        ysfx_slider_get_enum_names(fx.get(), i, names.data(), count);
        int k = 0;
        for(const char* val : names)
          values.push_back({val, k++});

        auto slider = new Process::ComboBox{values, 0, name, id, this};

        this->m_inlets.push_back(slider);
      }
      else if(ysfx_slider_is_path(fx.get(), i))
      {
        auto slider = new Process::LineEdit{{}, name, id, this};
        this->m_inlets.push_back(slider);
      }
      else
      {
        ysfx_slider_range_t range{};
        ysfx_slider_get_range(fx.get(), i, &range);

        auto slider = new Process::FloatSlider(range.min, range.max, range.def, name, id, this);
        // TODO increment

        this->m_inlets.push_back(slider);
      }
    }
  }
}

QString ProcessModel::script() const noexcept
{
  return m_script;
}

Window::Window(const ProcessModel& e, const score::DocumentContext& ctx, QWidget* parent)
  : m_model{e}
{
  setAttribute(Qt::WA_DeleteOnClose, true);

  fx = e.fx;
  e.externalUIVisible(true);
  const_cast<QWidget*&>(m_model.externalUI) = this;

  uint32_t dim[2];
  ysfx_get_gfx_dim(fx.get(), dim);

  if(dim[0] == 0 || dim[1] == 0)
  {
    dim[0] = 640;
    dim[1] = 480;
    setFixedSize(640, 480);
  }
  else
  {
    setFixedSize(dim[0], dim[1]);
  }

  m_frame = QImage(QSize(dim[0], dim[1]), QImage::Format_RGBX8888);
  m_frame.fill(0);

  conf.user_data = this;
  conf.pixel_width = dim[0];
  conf.pixel_height = dim[1];
  conf.pixel_stride = 0;// conf.pixel_width * 4;
  conf.pixels = m_frame.bits();
  conf.scale_factor = 1.0;
  conf.show_menu = [] (void *user_data, const char *menu_spec, int32_t xpos, int32_t ypos) -> int32_t
  {
    //qDebug() << "Show menu" << menu_spec << xpos << ypos;
    return 0;
  };
  conf.set_cursor = [ ] (void *user_data, int32_t cursor)
  {
    //qDebug() << "set cursor:" << cursor;
  };

  conf.get_drop_file = [ ] (void *user_data, int32_t index) -> const char*
  {
    //qDebug() << "Get drop file" << index;
    return "";
  };

  ysfx_gfx_setup(fx.get(), &conf);

  update();

  startTimer(16, Qt::CoarseTimer);
}

Window::~Window()
{
  const_cast<QWidget*&>(m_model.externalUI) = nullptr;
  m_model.externalUIVisible(false);
}

void Window::resizeEvent(QResizeEvent* event)
{
  event->accept();
}

void Window::closeEvent(QCloseEvent* event)
{
  event->accept();
}


static
uint32_t qt_to_ysfx_mods()
{
  const auto qt_mods = qApp->keyboardModifiers();
  uint32_t mods{};
  mods |= (qt_mods & Qt::ControlModifier) ? ysfx_mod_ctrl : 0;
  mods |= (qt_mods & Qt::ShiftModifier) ? ysfx_mod_shift : 0;
  mods |= (qt_mods & Qt::AltModifier) ? ysfx_mod_alt : 0;
  mods |= (qt_mods & Qt::MetaModifier) ? ysfx_mod_super : 0;
  return mods;
}

static
int qt_to_ysfx_key(int k)
{
  int key = -1;
  switch(k)
  {
    case Qt::Key_Backspace: key = ysfx_key_backspace; break;
    case Qt::Key_Escape: key = ysfx_key_escape; break;
    case Qt::Key_Delete: key = ysfx_key_delete; break;

    case Qt::Key_F1: key = ysfx_key_f1; break;
    case Qt::Key_F2: key = ysfx_key_f2; break;
    case Qt::Key_F3: key = ysfx_key_f3; break;
    case Qt::Key_F4: key = ysfx_key_f4; break;
    case Qt::Key_F5: key = ysfx_key_f5; break;
    case Qt::Key_F6: key = ysfx_key_f6; break;
    case Qt::Key_F7: key = ysfx_key_f7; break;
    case Qt::Key_F8: key = ysfx_key_f8; break;
    case Qt::Key_F9: key = ysfx_key_f9; break;
    case Qt::Key_F10: key = ysfx_key_f10; break;
    case Qt::Key_F11: key = ysfx_key_f11; break;
    case Qt::Key_F12: key = ysfx_key_f12; break;

    case Qt::Key_Left: key = ysfx_key_left; break;
    case Qt::Key_Right: key = ysfx_key_right; break;
    case Qt::Key_Up: key = ysfx_key_up; break;
    case Qt::Key_Down: key = ysfx_key_down; break;

    case Qt::Key_PageUp: key = ysfx_key_page_up; break;
    case Qt::Key_PageDown: key = ysfx_key_page_down; break;
    case Qt::Key_Home: key = ysfx_key_home; break;
    case Qt::Key_End: key = ysfx_key_end; break;
    case Qt::Key_Insert: key = ysfx_key_insert; break;
    default:
      break;
  }
  return key;
}


void Window::mousePressEvent(QMouseEvent* event)
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
  ysfx_real wheel{};
  ysfx_real hwheel{};

  ysfx_gfx_update_mouse(fx.get(), mods, xpos, ypos, buttons, wheel, hwheel);

  event->accept();
}

void Window::mouseReleaseEvent(QMouseEvent* event)
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
  ysfx_real wheel{};
  ysfx_real hwheel{};

  ysfx_gfx_update_mouse(fx.get(), mods, xpos, ypos, buttons, wheel, hwheel);

  event->accept();
}

void Window::mouseMoveEvent(QMouseEvent* event)
{
  mousePressEvent(event);
}

void Window::wheelEvent(QWheelEvent* event)
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
  ysfx_real wheel{event->angleDelta().y() / 120.};
  ysfx_real hwheel{event->angleDelta().x() / 120.};

  ysfx_gfx_update_mouse(fx.get(), mods, xpos, ypos, buttons, wheel, hwheel);

  event->accept();
  event->accept();
}

void Window::keyPressEvent(QKeyEvent* event)
{
  const auto mods = qt_to_ysfx_mods();

  const auto key = qt_to_ysfx_key(event->key());
  if(key != -1)
    ysfx_gfx_add_key(fx.get(), mods, key, true);

  event->accept();
}

void Window::keyReleaseEvent(QKeyEvent* event)
{
  const auto mods = qt_to_ysfx_mods();

  const auto key = qt_to_ysfx_key(event->key());
  if(key != -1)
    ysfx_gfx_add_key(fx.get(), mods, key, false);

  event->accept();
}

void Window::paintEvent(QPaintEvent* event)
{
  ysfx_gfx_run(fx.get());

  QPainter p{this};
  p.drawImage(QPoint{}, this->m_frame.rgbSwapped());
  event->accept();
}

void YSFX::Window::timerEvent(QTimerEvent* event)
{
  update();
}
}



