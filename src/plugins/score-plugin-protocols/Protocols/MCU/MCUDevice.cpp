// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MCUDevice.hpp"

#include <State/MessageListSerialization.hpp>

#include <Device/Protocol/DeviceSettings.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Protocols/MCU/MCUSpecificSettings.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/serialization/MimeVisitor.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/protocols/midi/midi.hpp>

#include <boost/algorithm/string/trim.hpp>

#include <QDebug>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMimeData>

#include <RemoteControl/RemoteControlProvider.hpp>
// clang-format off
#include <libremidi/libremidi.hpp>
#include <libremidi/backends.hpp>
#include <libremidi/configurations.hpp>
#include <libremidi/protocols/remote_control.hpp>

// clang-format on

#include <memory>

namespace Protocols
{
class mcu_protocol
    : public QObject
    , public ossia::net::protocol_base
{
public:
  const score::DocumentContext& doc;
  mcu_protocol(
      const score::DocumentContext& doc, std::shared_ptr<Process::RemoteControl> rc,
      ossia::net::network_context_ptr ctx, libremidi::API api,
      const libremidi::input_port& ip, const libremidi::output_port& op)
      : doc{doc}
      , m_rc{rc}
      , m_context{ctx}
  {
    using hint = Process::RemoteControl::ControllerHint;
    // Init the handles
    m_knob_handles
        = m_rc->registerControllerGroup((hint)(hint::Knob | hint::MapControls), 8);
    m_knob_button_handles = m_rc->registerControllerGroup(hint::Button_Knob, 8);
    m_fader_handles = m_rc->registerControllerGroup(hint::Fader, 8);
    m_f_handles = m_rc->registerControllerGroup(hint::Button_Function, 8);
    m_rec_handles = m_rc->registerControllerGroup(hint::Button_Rec, 8);
    m_mute_handles = m_rc->registerControllerGroup(hint::Button_Mute, 8);
    m_solo_handles = m_rc->registerControllerGroup(hint::Button_Solo, 8);
    m_select_handles = m_rc->registerControllerGroup(hint::Button_Select, 8);

    libremidi::output_configuration out_conf{};

    m_output = std::make_shared<libremidi::midi_out>(out_conf, api);

    libremidi::rcp_configuration conf{
        .midi_out =
            [this, ctx = m_context](libremidi::message&& msg) {
      boost::asio::post(
          ctx->context, [msg = std::move(msg), o = std::weak_ptr{m_output}] {
        if(auto out = o.lock())
          out->send_message(std::move(msg));
      });
    },
        .on_command =
            [self = QPointer{this}](
                libremidi::remote_control_protocol::mixer_command cmd, bool pressed) {
      QMetaObject::invokeMethod(qApp, [self, cmd, pressed] {
        if(self)
          self->on_command(cmd, pressed);
      });
    },
        .on_control =
            [self = QPointer{this}](
                libremidi::remote_control_protocol::mixer_control ctl, int v) {
      QMetaObject::invokeMethod(qApp, [self, ctl, v] {
        if(self)
          self->on_control(ctl, v);
      });
    },
        .on_fader = [self = QPointer{this}](
                        libremidi::remote_control_protocol::fader f, uint16_t v) {
      QMetaObject::invokeMethod(qApp, [self, f, v] {
        if(self)
          self->on_fader(f, v);
      });
    }};
    m_rcp = std::make_shared<libremidi::remote_control_processor>(conf);

    libremidi::input_configuration in_conf{};
    in_conf.on_message
        = [this](const libremidi::message& message) { m_rcp->on_midi(message); };
    m_input = std::make_shared<libremidi::midi_in>(in_conf, api);

    m_output->open_port(op);
    m_rcp->start();
    m_input->open_port(ip);

    QObject::connect(
        m_rc.get(), &Process::RemoteControl::controlNameChanged, this,
        &mcu_protocol::on_control_name_changed);
    QObject::connect(
        m_rc.get(), &Process::RemoteControl::controlValueChanged, this,
        &mcu_protocol::on_control_value_changed);
    QObject::connect(
        m_rc.get(), &Process::RemoteControl::transportChanged, this,
        &mcu_protocol::on_transport_changed);

    blank();
  }

  ~mcu_protocol()
  {
    auto& remote_controls
        = score::AppContext().interfaces<Process::RemoteControlProviderList>();

    if(!remote_controls.empty())
    {
      remote_controls[0].release(this->doc, this->m_rc);
    }
  }

  static constexpr const char blank_lcd_0[] = {" .*^ welcome to ossia score ^*. "};
  void blank()
  {
    QTimer::singleShot(1, this, [this] { m_rcp->update_lcd(std::string(112, ' ')); });

    QTimer::singleShot(
        40, this, [this, i = -int64_t(strlen(blank_lcd_0))] { blank(i); });
  }
  void blank(int i)
  {
    std::string_view txt = blank_lcd_0;
    if(i < 0)
      txt = txt.substr(-i);
    else if(i > (56 - (int64_t)strlen(blank_lcd_0)))
      txt = txt.substr(0, 56 - i);
    m_rcp->update_lcd(txt, std::clamp(i, 0, 56));
    if(i++ < 56)
      QTimer::singleShot(40, this, [this, i] { blank(i); });
  }

  std::string nameToLed(QString n)
  {
    if(n.size() > 7)
      n.resize(7);
    return n.toStdString();
  }

  std::string valueToLed(const ossia::value& v)
  {
    auto vv = ossia::convert<std::string>(v);
    if(vv.size() > 7)
      vv.resize(7);
    return vv;
  }

  void centerText(std::string& txt)
  {
    static const thread_local auto loc = std::locale("C");
    boost::algorithm::trim(txt);
    switch(txt.size())
    {
      case 0:
        break;
      case 1:
        txt = std::format("   {}   ", txt);
        break;
      case 2:
        txt = std::format("   {}  ", txt);
        break;
      case 3:
        txt = std::format("  {}  ", txt);
        break;
      case 4:
        txt = std::format("  {} ", txt);
        break;
      case 5:
        txt = std::format(" {} ", txt);
        break;
      case 6:
        txt = std::format("{}", txt);
      case 7:
        txt = std::format("{}", txt);
        break;
    }
  }

  void
  on_control_name_changed(Process::RemoteControl::ControllerHandle index, QString name)
  {
    if(int idx = ossia::index_in_container(this->m_knob_handles, index); idx != -1)
    {
      int pos = idx * 7;
      if(name.isEmpty())
      {
        boost::asio::post(
            m_context->context, [this, pos] { m_rcp->update_lcd("       ", pos); });
      }
      else
      {
        auto str = nameToLed(name);
        centerText(str);
        boost::asio::post(
            m_context->context, [this, str, pos] { m_rcp->update_lcd(str, pos); });
      }
    }
  }

  void on_control_value_changed(
      Process::RemoteControl::ControllerHandle index, const ossia::value& v)
  {
    if(int idx = ossia::index_in_container(this->m_knob_handles, index); idx != -1)
    {
      int pos = idx * 7 + 56;
      if(!v.valid())
      {
        boost::asio::post(
            m_context->context, [this, pos] { m_rcp->update_lcd("       ", pos); });
      }
      else
      {
        auto str = valueToLed(v);
        centerText(str);
        boost::asio::post(
            m_context->context, [this, str, pos] { m_rcp->update_lcd(str, pos); });
      }
    }
    else if(int idx = ossia::index_in_container(this->m_fader_handles, index); idx != -1)
    {
      if(idx >= 0 && idx < 9)
      {
        if(!m_fader_grab.test(idx))
        {
          // TODO
          boost::asio::post(m_context->context, [this] { });
        }
      }
    }
  }

  void on_transport_changed(ossia::time_value t, double b, double q, double s, double ss)
  {
    const double one_hour_in_ms = 3600 * 1000;
    const double one_minute_in_ms = 60000;
    const double one_second_in_ms = 1000;
    int64_t ms = t.impl / ossia::flicks_per_millisecond<double>;
    int64_t hours = ms / one_hour_in_ms;
    ms -= hours * one_hour_in_ms;
    int64_t minutes = ms / one_minute_in_ms;
    ms -= minutes * one_minute_in_ms;
    int64_t seconds = ms / one_second_in_ms;
    ms -= seconds * one_second_in_ms;

    this->m_rcp->update_timecode(hours, minutes, seconds, ms);
  }

  void on_command(libremidi::remote_control_protocol::mixer_command cmd, bool pressed)
  {
    auto act = pressed ? Process::RemoteControl::ControllerAction::Press
                       : Process::RemoteControl::ControllerAction::Release;
    using rcp = libremidi::remote_control_protocol::mixer_command;
    switch(cmd)
    {
      case rcp::vpot_click_0:
        m_rc->setControl(m_knob_button_handles[0], pressed);
        break;
      case rcp::vpot_click_1:
        m_rc->setControl(m_knob_button_handles[1], pressed);
        break;
      case rcp::vpot_click_2:
        m_rc->setControl(m_knob_button_handles[2], pressed);
        break;
      case rcp::vpot_click_3:
        m_rc->setControl(m_knob_button_handles[3], pressed);
        break;
      case rcp::vpot_click_4:
        m_rc->setControl(m_knob_button_handles[4], pressed);
        break;
      case rcp::vpot_click_5:
        m_rc->setControl(m_knob_button_handles[5], pressed);
        break;
      case rcp::vpot_click_6:
        m_rc->setControl(m_knob_button_handles[6], pressed);
        break;
      case rcp::vpot_click_7:
        m_rc->setControl(m_knob_button_handles[7], pressed);
        break;

      case rcp::rec_0:
        m_rc->setControl(m_rec_handles[0], pressed);
        break;
      case rcp::rec_1:
        m_rc->setControl(m_rec_handles[1], pressed);
        break;
      case rcp::rec_2:
        m_rc->setControl(m_rec_handles[2], pressed);
        break;
      case rcp::rec_3:
        m_rc->setControl(m_rec_handles[3], pressed);
        break;
      case rcp::rec_4:
        m_rc->setControl(m_rec_handles[4], pressed);
        break;
      case rcp::rec_5:
        m_rc->setControl(m_rec_handles[5], pressed);
        break;
      case rcp::rec_6:
        m_rc->setControl(m_rec_handles[6], pressed);
        break;
      case rcp::rec_7:
        m_rc->setControl(m_rec_handles[7], pressed);
        break;

      case rcp::solo_0:
        m_rc->setControl(m_solo_handles[0], pressed);
        break;
      case rcp::solo_1:
        m_rc->setControl(m_solo_handles[1], pressed);
        break;
      case rcp::solo_2:
        m_rc->setControl(m_solo_handles[2], pressed);
        break;
      case rcp::solo_3:
        m_rc->setControl(m_solo_handles[3], pressed);
        break;
      case rcp::solo_4:
        m_rc->setControl(m_solo_handles[4], pressed);
        break;
      case rcp::solo_5:
        m_rc->setControl(m_solo_handles[5], pressed);
        break;
      case rcp::solo_6:
        m_rc->setControl(m_solo_handles[6], pressed);
        break;
      case rcp::solo_7:
        m_rc->setControl(m_solo_handles[7], pressed);
        break;

      case rcp::mute_0:
        m_rc->setControl(m_mute_handles[0], pressed);
        break;
      case rcp::mute_1:
        m_rc->setControl(m_mute_handles[1], pressed);
        break;
      case rcp::mute_2:
        m_rc->setControl(m_mute_handles[2], pressed);
        break;
      case rcp::mute_3:
        m_rc->setControl(m_mute_handles[3], pressed);
        break;
      case rcp::mute_4:
        m_rc->setControl(m_mute_handles[4], pressed);
        break;
      case rcp::mute_5:
        m_rc->setControl(m_mute_handles[5], pressed);
        break;
      case rcp::mute_6:
        m_rc->setControl(m_mute_handles[6], pressed);
        break;
      case rcp::mute_7:
        m_rc->setControl(m_mute_handles[7], pressed);
        break;

      case rcp::sel_0:
        m_rc->setControl(m_select_handles[0], pressed);
        break;
      case rcp::sel_1:
        m_rc->setControl(m_select_handles[1], pressed);
        break;
      case rcp::sel_2:
        m_rc->setControl(m_select_handles[2], pressed);
        break;
      case rcp::sel_3:
        m_rc->setControl(m_select_handles[3], pressed);
        break;
      case rcp::sel_4:
        m_rc->setControl(m_select_handles[4], pressed);
        break;
      case rcp::sel_5:
        m_rc->setControl(m_select_handles[5], pressed);
        break;
      case rcp::sel_6:
        m_rc->setControl(m_select_handles[6], pressed);
        break;
      case rcp::sel_7:
        m_rc->setControl(m_select_handles[7], pressed);
        break;

      // TODO metering

      case rcp::assign_track:
        break;
      case rcp::assign_send:
        break;
      case rcp::assign_pan:
        break;
      case rcp::assign_plugin:
        break;
      case rcp::assign_eq:
        break;
      case rcp::assign_instrument:
        break;

      case rcp::bank_left:
        m_rc->prevBank(act);
        break;
      case rcp::bank_right:
        m_rc->nextBank(act);
        break;
      case rcp::channel_left:
        m_rc->prevChannel(act);
        break;
      case rcp::channel_right:
        m_rc->nextChannel(act);
        break;
      case rcp::flip:
        break;
      case rcp::global:
        break;

      case rcp::name_value_button:
        break;
      case rcp::smpte_beats_button:
        break;

      case rcp::f1:
        m_rc->setControl(m_f_handles[0], pressed);
        break;
      case rcp::f2:
        m_rc->setControl(m_f_handles[1], pressed);
        break;
      case rcp::f3:
        m_rc->setControl(m_f_handles[2], pressed);
        break;
      case rcp::f4:
        m_rc->setControl(m_f_handles[3], pressed);
        break;
      case rcp::f5:
        m_rc->setControl(m_f_handles[4], pressed);
        break;
      case rcp::f6:
        m_rc->setControl(m_f_handles[5], pressed);
        break;
      case rcp::f7:
        m_rc->setControl(m_f_handles[6], pressed);
        break;
      case rcp::f8:
        m_rc->setControl(m_f_handles[7], pressed);
        break;

      case rcp::midi_tracks:
        break;
      case rcp::inputs:
        break;
      case rcp::audio_tracks:
        break;
      case rcp::audio_instruments:
        break;
      case rcp::aux:
        break;
      case rcp::busses:
        break;
      case rcp::outputs:
        break;
      case rcp::user:
        break;

      case rcp::shift:
        m_shift = pressed;
        break;
      case rcp::option:
        m_option = pressed;
        break;
      case rcp::control:
        m_control = pressed;
        break;
      case rcp::alt:
        m_alt = pressed;
        break;

      case rcp::save:
        m_rc->save(act);
        break;
      case rcp::undo:
        m_rc->undo(act);
        break;
      case rcp::cancel:
        m_rc->cancel(act);
        break;
      case rcp::enter:
        m_rc->enter(act);
        break;

      case rcp::markers:
        break;
      case rcp::nudge:
        break;
      case rcp::cycle:
        break;
      case rcp::drop:
        break;
      case rcp::replace:
        break;
      case rcp::click:
        break;
      case rcp::solo:
        m_rc->solo(act);
        break;

      case rcp::rewind:
        break;
      case rcp::forward:
        break;
      case rcp::stop:
        m_rc->stop(act);
        break;
      case rcp::play:
        m_rc->play(act);
        break;
      case rcp::record:
        m_rc->record(act);
        break;

      case rcp::up:
        m_rc->up(act);
        break;
      case rcp::down:
        m_rc->down(act);
        break;
      case rcp::left:
        m_rc->left(act);
        break;
      case rcp::right:
        m_rc->right(act);
        break;
      case rcp::zoom: {
        double zoom_amount = m_shift ? -0.5 : 0.5;
        if(m_control)
          zoom_amount /= 10.;
        m_rc->zoom(zoom_amount, 0.);
        break;
      }
      case rcp::scrub:
        m_scrub = pressed;
        break;

      case rcp::user_switch_1:
        break;
      case rcp::user_switch_2:
        break;

      case rcp::fader_touched_0:
        m_fader_grab.set(0, pressed);
        break;
      case rcp::fader_touched_1:
        m_fader_grab.set(1, pressed);
        break;
      case rcp::fader_touched_2:
        m_fader_grab.set(2, pressed);
        break;
      case rcp::fader_touched_3:
        m_fader_grab.set(3, pressed);
        break;
      case rcp::fader_touched_4:
        m_fader_grab.set(4, pressed);
        break;
      case rcp::fader_touched_5:
        m_fader_grab.set(5, pressed);
        break;
      case rcp::fader_touched_6:
        m_fader_grab.set(6, pressed);
        break;
      case rcp::fader_touched_7:
        m_fader_grab.set(7, pressed);
        break;
      case rcp::fader_touched_master:
        m_fader_grab.set(8, pressed);
        break;

      case rcp::smpte_led:
        break;
      case rcp::beats_led:
        break;
      case rcp::rude_solo_led:
        break;

      case rcp::relay_click:
        break;
    }
  }

  void on_control(libremidi::remote_control_protocol::mixer_control cmd, int v)
  {
    auto knob_to_speed = [](int v) {
      if(v >= 0 && v <= 64)
        return 1.f;
      else
        return -1.f;
    };
    using rcp = libremidi::remote_control_protocol::mixer_control;
    switch(cmd)
    {
      case rcp::vpot_rotation_0:
        m_rc->offsetControl(m_knob_handles[0], knob_to_speed(v));
        break;
      case rcp::vpot_rotation_1:
        m_rc->offsetControl(m_knob_handles[1], knob_to_speed(v));
        break;
      case rcp::vpot_rotation_2:
        m_rc->offsetControl(m_knob_handles[2], knob_to_speed(v));
        break;
      case rcp::vpot_rotation_3:
        m_rc->offsetControl(m_knob_handles[3], knob_to_speed(v));
        break;
      case rcp::vpot_rotation_4:
        m_rc->offsetControl(m_knob_handles[4], knob_to_speed(v));
        break;
      case rcp::vpot_rotation_5:
        m_rc->offsetControl(m_knob_handles[5], knob_to_speed(v));
        break;
      case rcp::vpot_rotation_6:
        m_rc->offsetControl(m_knob_handles[6], knob_to_speed(v));
        break;
      case rcp::vpot_rotation_7:
        m_rc->offsetControl(m_knob_handles[7], knob_to_speed(v));
        break;
      case rcp::external_control:
        // FIXME ??
        break;
      case rcp::jog_wheel: {
        float speed = knob_to_speed(v);
        if(m_control)
          speed /= 10.;

        if(m_scrub)
        {
          m_rc->scrub(speed);
        }
        else
        {
          if(m_shift)
            m_rc->scroll(0., speed);
          else
            m_rc->scroll(speed, 0.);
        }
      }
        break;
    }
  }

  void on_fader(libremidi::remote_control_protocol::fader f, uint16_t v)
  {
    using rcp = libremidi::remote_control_protocol::fader;
    switch(f)
    {
      case rcp::fader_0:
        m_rc->setControl(m_fader_handles[0], v / 16384.f);
        break;
      case rcp::fader_1:
        m_rc->setControl(m_fader_handles[1], v / 16384.f);
        break;
      case rcp::fader_2:
        m_rc->setControl(m_fader_handles[2], v / 16384.f);
        break;
      case rcp::fader_3:
        m_rc->setControl(m_fader_handles[3], v / 16384.f);
        break;
      case rcp::fader_4:
        m_rc->setControl(m_fader_handles[4], v / 16384.f);
        break;
      case rcp::fader_5:
        m_rc->setControl(m_fader_handles[5], v / 16384.f);
        break;
      case rcp::fader_6:
        m_rc->setControl(m_fader_handles[6], v / 16384.f);
        break;
      case rcp::fader_7:
        m_rc->setControl(m_fader_handles[7], v / 16384.f);
        break;
      case rcp::fader_master:
        // TODO
        break;
    }
  }
  std::shared_ptr<Process::RemoteControl> m_rc;
  ossia::net::network_context_ptr m_context;
  std::shared_ptr<libremidi::midi_out> m_output;
  std::shared_ptr<libremidi::remote_control_processor> m_rcp;
  std::shared_ptr<libremidi::midi_in> m_input;

  bool m_shift{};
  bool m_alt{};
  bool m_option{};
  bool m_control{};
  bool m_scrub{};
  std::bitset<9> m_fader_grab{};

  std::vector<Process::RemoteControl::ControllerHandle> m_knob_handles;
  std::vector<Process::RemoteControl::ControllerHandle> m_knob_button_handles;
  std::vector<Process::RemoteControl::ControllerHandle> m_fader_handles;
  std::vector<Process::RemoteControl::ControllerHandle> m_f_handles;
  std::vector<Process::RemoteControl::ControllerHandle> m_rec_handles;
  std::vector<Process::RemoteControl::ControllerHandle> m_mute_handles;
  std::vector<Process::RemoteControl::ControllerHandle> m_solo_handles;
  std::vector<Process::RemoteControl::ControllerHandle> m_select_handles;

  bool pull(ossia::net::parameter_base&) override { return true; }
  bool push(const ossia::net::parameter_base&, const ossia::value& v) override
  {
    return true;
  }
  bool push_raw(const ossia::net::full_parameter_data&) override { return true; }
  bool observe(ossia::net::parameter_base&, bool) override { return true; }
  bool update(ossia::net::node_base& node_base) override { return true; }
};

MCUDevice::MCUDevice(
    const Device::DeviceSettings& settings, const ossia::net::network_context_ptr& ctx,
    const score::DocumentContext& doc)
    : OwningDeviceInterface{settings}
    , m_ctx{ctx}
    , m_doc{doc}
{
  using namespace ossia;

  const auto set = settings.deviceSpecificSettings.value<MCUSpecificSettings>();
  m_capas.canRefreshTree = true;
  m_capas.canSerialize = false;
  m_capas.hasCallbacks = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canLearn = true;
}

MCUDevice::~MCUDevice() { }

bool MCUDevice::reconnect()
{
  disconnect();

  try
  {
    MCUSpecificSettings set
        = settings().deviceSpecificSettings.value<MCUSpecificSettings>();

    auto& remote_controls
        = this->m_doc.app.interfaces<Process::RemoteControlProviderList>();

    if(!remote_controls.empty())
    {
      auto rc = remote_controls[0].make(this->m_doc);
      if(rc)
      {
        auto proto = std::make_unique<mcu_protocol>(
            this->m_doc, rc, this->m_ctx, set.api, set.input_handle, set.output_handle);

        auto dev = std::make_unique<ossia::net::generic_device>(
            std::move(proto), settings().name.toStdString());
        m_dev = std::move(dev);
      }
    }

    deviceChanged(nullptr, m_dev.get());
  }
  catch(std::exception& e)
  {
    qDebug() << e.what();
  }
  m_capas.canSerialize = false;

  return connected();
}

void MCUDevice::disconnect()
{
  if(connected())
  {
    removeListening_impl(m_dev->get_root_node(), State::Address{m_settings.name, {}});
  }

  m_callbacks.clear();
  auto old = m_dev.get();
  m_dev.reset();
  deviceChanged(old, nullptr);
}
}
