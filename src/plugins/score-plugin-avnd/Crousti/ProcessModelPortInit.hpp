#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Process.hpp>

#include <Crousti/Concepts.hpp>
#include <Media/Sound/Drop/SoundDrop.hpp>

#include <avnd/binding/ossia/port_base.hpp>
#include <avnd/concepts/all.hpp>
#if SCORE_PLUGIN_GFX
#include <Gfx/TexturePort.hpp>
#endif

namespace oscr
{
template <typename Info>
class ProcessModel;
}

namespace oscr
{

template <typename T>
static AVND_INLINE QString portName()
{
  //FIXME
#if !defined(__APPLE__)
  static constexpr auto name = avnd::get_name<T>();
  return fromStringView(name);
#else
  auto name = avnd::get_name<T>();
  return fromStringView(name);
#endif
}

template <typename T>
inline void setupNewPort(Process::Port* obj)
{
  if constexpr(avnd::has_description<T>)
    obj->setDescription(fromStringView(avnd::get_description<T>()));
}

template <std::size_t N, typename T>
inline void setupNewPort(const avnd::field_reflection<N, T>& spec, Process::Port* obj)
{
  setupNewPort<T>(obj);
}

template <typename T>
inline void setupNewPort(const T& spec, Process::Port* obj)
{
  setupNewPort<T>(obj);
}

template <typename Node>
struct InletInitFunc
{
  Process::ProcessModel& self;
  Process::Inlets& ins;
  int inlet = 0;

  template <oscr::ossia_value_port T>
  void operator()(const T& in, auto idx)
  {
    auto p = new Process::ValueInlet(portName<T>(), Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
  }
  template <oscr::ossia_audio_port T>
  void operator()(const T& in, auto idx)
  {
    auto p = new Process::AudioInlet(portName<T>(), Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
  }
  template <oscr::ossia_midi_port T>
  void operator()(const T& in, auto idx)
  {
    auto p = new Process::MidiInlet(portName<T>(), Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
  }

  template <avnd::audio_port T>
  void operator()(const T& in, auto idx)
  {
    auto p = new Process::AudioInlet(portName<T>(), Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
  }
  template <avnd::midi_port T>
  void operator()(const T& in, auto idx)
  {
    auto p = new Process::MidiInlet(portName<T>(), Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
  }

  static QString toFilters(const QSet<QString>& exts)
  {
    QString res;
    for(const auto& s : exts)
    {
      res += "*.";
      res += s;
      res += " ";
    }
    if(!res.isEmpty())
      res.resize(res.size() - 1);
    return res;
  }

  template <typename P>
  static QString getFilters(P& p)
  {
    if constexpr(requires { P::filters(); })
    {
      using filter = decltype(P::filters());
      if constexpr(requires { filter::sound; } || requires { filter::audio; })
      {
        return QString{"Sound files (%1)"}.arg(
            toFilters(Media::Sound::DropHandler{}.fileExtensions()));
      }
      else if constexpr(requires { filter::video; })
      {
        // FIXME refactor supported formats with Video process
        QSet<QString> files = {"mkv",  "mov", "mp4", "h264", "avi",  "hap", "mpg",
                               "mpeg", "imf", "mxf", "mts",  "m2ts", "mj2", "webm"};
        return QString{"Videos (%1)"}.arg(toFilters(files));
      }
      else if constexpr(requires { filter::image; })
      {
        // FIXME refactor supported formats with Image List Chooser
        return QString{"Images (*.png *.jpg *.jpeg *.gif *.bmp *.tiff)"};
      }
      else if constexpr(requires { filter::midi; })
      {
        return "MIDI (*.mid)";
      }
      else if constexpr(avnd::string_ish<filter>)
      {
        static constexpr auto text_filters = P::filters();
        return fromStringView(P::filters());
      }
      else
      {
        return "";
      }
    }
    else if constexpr(requires { P::extensions(); })
    {
      static constexpr auto text_filters = P::extensions();
      return fromStringView(text_filters);
    }
    else
    {
      return "";
    }
  }

  template <avnd::soundfile_port T>
  void operator()(const T& in, auto idx)
  {
    static constexpr auto name = avnd::get_name<T>();
    Process::FileChooserBase* p{};
    if constexpr(requires { T::waveform; })
    {
      p = new Process::AudioFileChooser{
          "", getFilters(in), QString::fromUtf8(name.data(), name.size()),
          Id<Process::Port>(inlet++), &self};
    }
    else
    {
      p = new Process::FileChooser{
          "", getFilters(in), QString::fromUtf8(name.data(), name.size()),
          Id<Process::Port>(inlet++), &self};
    }

    p->displayHandledExplicitly = true;
    ins.push_back(p);

    if constexpr(avnd::tag_file_watch<T>)
    {
      p->enableFileWatch();
    }
  }

  template <typename T>
    requires avnd::midifile_port<T> || avnd::raw_file_port<T>
  void operator()(const T& in, auto idx)
  {
    static constexpr auto name = avnd::get_name<T>();

    auto p = new Process::FileChooser{
        "", getFilters(in), QString::fromUtf8(name.data(), name.size()),
        Id<Process::Port>(inlet++), &self};
    p->displayHandledExplicitly = true;
    ins.push_back(p);

    if constexpr(avnd::tag_file_watch<T>)
    {
      p->enableFileWatch();
    }
  }

  template <avnd::dynamic_ports_port T, std::size_t N>
  void operator()(const T& in, avnd::field_index<N> i)
  {
    using port_type = typename decltype(in.ports)::value_type;
    port_type p;
    (*this)(p, i);
  }

  template <avnd::parameter T, std::size_t N>
    requires(!oscr::ossia_port<T>)
  void operator()(const T& in, avnd::field_index<N>)
  {
    if constexpr(avnd::has_widget<T> || avnd::curve_port<T>)
    {
      auto p = oscr::make_control_in<Node, T>(
          avnd::field_index<N>{}, Id<Process::Port>(inlet), &self);
      using port_ptr_type = std::decay_t<decltype(p)>;
      if constexpr(!std::is_same_v<port_ptr_type, std::nullptr_t>)
      {
        if constexpr(
            !std::is_same_v<port_ptr_type, Process::ControlInlet*>
            && !std::is_same_v<port_ptr_type, Process::ValueInlet*>)
          p->displayHandledExplicitly = true;
        ins.push_back(p);
      }
      else
      {
        auto vp
            = new Process::ValueInlet(portName<T>(), Id<Process::Port>(inlet), &self);
        setupNewPort(in, vp);
        ins.push_back(vp);
      }
    }
    else
    {
      auto vp = new Process::ValueInlet(portName<T>(), Id<Process::Port>(inlet), &self);
      setupNewPort(in, vp);
      ins.push_back(vp);
    }
    inlet++;
  }

  template <avnd::buffer_port T>
  void operator()(const T& in, auto idx)
  {
#if SCORE_PLUGIN_GFX
    auto p = new Gfx::TextureInlet(portName<T>(), Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
#endif
  }

  template <avnd::texture_port T>
  void operator()(const T& in, auto idx)
  {
#if SCORE_PLUGIN_GFX
    auto p = new Gfx::TextureInlet(portName<T>(), Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
#endif
  }

  template <avnd::geometry_port T>
  void operator()(const T& in, auto idx)
  {
#if SCORE_PLUGIN_GFX
    auto p = new Gfx::GeometryInlet(portName<T>(), Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
#endif
  }

  template <std::size_t Idx, avnd::message T>
  void operator()(const avnd::field_reflection<Idx, T>& in, auto dummy)
  {
    auto p = new Process::ValueInlet(portName<T>(), Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
  }

  template <std::size_t Idx, avnd::unreflectable_message<Node> T>
  void operator()(const avnd::field_reflection<Idx, T>& in, auto dummy)
  {
    auto p = new Process::ValueInlet(portName<T>(), Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
  }

  void operator()(const auto& ctrl, auto idx)
  {
    //(avnd::message<std::decay_t<decltype(ctrl)>>);
    qDebug() << fromStringView(avnd::get_name(ctrl)) << "unhandled";
  }
};

template <typename Node>
struct OutletInitFunc
{
  Process::ProcessModel& self;
  Process::Outlets& outs;
  int outlet = 0;

  template <oscr::ossia_value_port T>
  void operator()(const T& out, auto idx)
  {
    auto p = new Process::ValueOutlet(portName<T>(), Id<Process::Port>(outlet++), &self);
    setupNewPort(out, p);
    outs.push_back(p);
  }

  template <oscr::ossia_audio_port T>
  void operator()(const T& out, auto idx)
  {
    auto p = new Process::AudioOutlet(portName<T>(), Id<Process::Port>(outlet++), &self);
    setupNewPort(out, p);
    outs.push_back(p);
  }

  template <oscr::ossia_midi_port T>
  void operator()(const T& out, auto idx)
  {
    auto p = new Process::MidiOutlet(portName<T>(), Id<Process::Port>(outlet++), &self);
    setupNewPort(out, p);
    outs.push_back(p);
  }

  template <avnd::dynamic_ports_port T, std::size_t N>
  void operator()(const T& in, avnd::field_index<N> i)
  {
    using port_type = typename decltype(in.ports)::value_type;
    port_type p;
    (*this)(p, i);
  }

  template <avnd::audio_port T>
  void operator()(const T& out, auto idx)
  {
    auto p = new Process::AudioOutlet(portName<T>(), Id<Process::Port>(outlet++), &self);
    setupNewPort(out, p);
    if(outlet == 1)
      p->setPropagate(true);
    outs.push_back(p);
  }

  template <avnd::midi_port T>
  void operator()(const T& out, auto idx)
  {
    auto p = new Process::MidiOutlet(portName<T>(), Id<Process::Port>(outlet++), &self);
    setupNewPort(out, p);
    outs.push_back(p);
  }

  template <avnd::parameter T, std::size_t N>
    requires(!oscr::ossia_port<T>)
  void operator()(const T& out, avnd::field_index<N>)
  {
    if constexpr(avnd::has_widget<T>)
    {
      if(auto p = oscr::make_control_out<T>(
             avnd::field_index<N>{}, Id<Process::Port>(outlet), &self))
      {
        p->displayHandledExplicitly = true;
        outs.push_back(p);
      }
      else
      {
        // FIXME ControlOutlet?
        auto vp
            = new Process::ValueOutlet(portName<T>(), Id<Process::Port>(outlet), &self);
        setupNewPort(out, vp);
        outs.push_back(vp);
      }
    }
    else
    {
      auto vp
          = new Process::ValueOutlet(portName<T>(), Id<Process::Port>(outlet), &self);
      setupNewPort(out, vp);
      outs.push_back(vp);
    }
    outlet++;
  }

  template <avnd::buffer_port T>
  void operator()(const T& out, auto idx)
  {
#if SCORE_PLUGIN_GFX
    auto p = new Gfx::TextureOutlet(portName<T>(), Id<Process::Port>(outlet++), &self);
    setupNewPort(out, p);
    outs.push_back(p);
#endif
  }

  template <avnd::texture_port T>
  void operator()(const T& out, auto idx)
  {
#if SCORE_PLUGIN_GFX
    auto p = new Gfx::TextureOutlet(portName<T>(), Id<Process::Port>(outlet++), &self);
    setupNewPort(out, p);
    outs.push_back(p);
#endif
  }

  template <avnd::geometry_port T>
  void operator()(const T& out, auto idx)
  {
#if SCORE_PLUGIN_GFX
    auto p = new Gfx::GeometryOutlet(portName<T>(), Id<Process::Port>(outlet++), &self);
    setupNewPort(out, p);
    outs.push_back(p);
#endif
  }

  template <avnd::curve_port T>
  void operator()(const T& out, auto idx)
  {
    auto p = new Process::ValueOutlet(portName<T>(), Id<Process::Port>(outlet++), &self);
    setupNewPort(out, p);
    outs.push_back(p);
  }

  template <avnd::callback T, std::size_t N>
  void operator()(const T& out, avnd::field_index<N>)
  {
    if constexpr(avnd::control<T>)
    {
      if(auto p = oscr::make_control_out<T>(
             avnd::field_index<N>{}, Id<Process::Port>(outlet), &self))
      {
        p->hidden = true;
        outs.push_back(p);
      }
      else
      {
        // FIXME ControlOutlet?
        auto vp
            = new Process::ValueOutlet(portName<T>(), Id<Process::Port>(outlet), &self);
        setupNewPort(out, vp);
        outs.push_back(vp);
      }
    }
    else if constexpr(requires { T::control; })
    {
      // FIXME remove this duplication
      auto p
          = new Process::ControlOutlet(portName<T>(), Id<Process::Port>(outlet), &self);
      setupNewPort(out, p);
      outs.push_back(p);
    }
    else
    {
      auto p = new Process::ValueOutlet(portName<T>(), Id<Process::Port>(outlet), &self);
      setupNewPort(out, p);
      outs.push_back(p);
    }
    outlet++;
  }

  void operator()(const auto& ctrl, auto idx)
  {
    qDebug() << fromStringView(avnd::get_name(ctrl)) << "unhandled";
  }
};
}
