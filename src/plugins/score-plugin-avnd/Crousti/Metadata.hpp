#pragma once
#include <Process/ProcessMetadata.hpp>

#include <Crousti/Concepts.hpp>

#include <QString>

#include <avnd/concepts/all.hpp>
#include <avnd/wrappers/metadatas.hpp>

#include <string_view>

namespace oscr
{
template <typename Info>
class ProcessModel;
}

inline QString fromStringView(std::string_view v)
{
  return QString::fromUtf8(v.data(), v.size());
}

////////// METADATA ////////////
namespace oscr
{
template <typename Info>
class ProcessModel;
}
template <typename Info>
  requires avnd::has_name<Info>
struct Metadata<PrettyName_k, oscr::ProcessModel<Info>>
{
  static constexpr const char* get() noexcept { return avnd::get_name<Info>().data(); }
};
template <typename Info>
  requires avnd::has_category<Info>
struct Metadata<Category_k, oscr::ProcessModel<Info>>
{
  static constexpr const char* get() noexcept
  {
    return avnd::get_category<Info>().data();
  }
};
template <typename Info>
  requires(!avnd::has_category<Info>)
struct Metadata<Category_k, oscr::ProcessModel<Info>>
{
  static constexpr const char* get() noexcept { return ""; }
};

template <typename Info>
struct Metadata<Tags_k, oscr::ProcessModel<Info>>
{
  static QStringList get() noexcept
  {
    QStringList lst;
    for(std::string_view tag : avnd::get_tags<Info>())
      lst.push_back(QString::fromUtf8(tag.data(), tag.size()));
    return lst;
  }
};

template <typename T>
concept has_kind = requires { T::kind(); };

template <typename T>
auto get_kind()
{
  if constexpr(has_kind<T>)
    return T::kind();
  else
    return Process::ProcessCategory::Other;
}

struct ProcessPortVisitor
{
  std::vector<Process::PortType> port;
  void audio() { port.push_back(Process::PortType::Audio); }
  void midi() { port.push_back(Process::PortType::Midi); }
  void value() { port.push_back(Process::PortType::Message); }
  void texture() { port.push_back(Process::PortType::Texture); }
  void geometry() { port.push_back(Process::PortType::Geometry); }

  template <std::size_t N, oscr::ossia_value_port Port>
  void operator()(const avnd::field_reflection<N, Port>)
  {
    this->value();
  }
  template <std::size_t N, oscr::ossia_audio_port Port>
  void operator()(const avnd::field_reflection<N, Port>)
  {
    this->audio();
  }

  template <std::size_t N, oscr::ossia_midi_port Port>
  void operator()(const avnd::field_reflection<N, Port>)
  {
    this->midi();
  }

  template <std::size_t N, avnd::dynamic_ports_port Port>
  void operator()(const avnd::field_reflection<N, Port>)
  {
    using port_type = typename decltype(std::declval<Port>().ports)::value_type;
    (*this)(avnd::field_reflection<std::size_t{0}, port_type>{});
  }

  template <std::size_t N, avnd::audio_port Port>
  void operator()(const avnd::field_reflection<N, Port>)
  {
    this->audio();
  }

  template <std::size_t N, avnd::midi_port Port>
  void operator()(const avnd::field_reflection<N, Port>)
  {
    this->midi();
  }

  template <std::size_t N, avnd::parameter Port>
    requires(!oscr::ossia_port<Port>)
  void operator()(const avnd::field_reflection<N, Port>)
  {
    this->value();
  }

  template <std::size_t N, avnd::file_port Port>
  void operator()(const avnd::field_reflection<N, Port>)
  {
    this->value();
  }

  template <std::size_t N, avnd::soundfile_port Port>
  void operator()(const avnd::field_reflection<N, Port>)
  {
    this->value();
  }

  template <std::size_t N, avnd::midifile_port Port>
  void operator()(const avnd::field_reflection<N, Port>)
  {
    this->value();
  }

#if SCORE_PLUGIN_GFX
  template <std::size_t N, avnd::texture_port Port>
  void operator()(const avnd::field_reflection<N, Port>)
  {
    this->texture();
  }
  template <std::size_t N, avnd::geometry_port Port>
  void operator()(const avnd::field_reflection<N, Port>)
  {
    this->geometry();
  }
#endif

  template <std::size_t N, avnd::curve_port Port>
  void operator()(const avnd::field_reflection<N, Port>)
  {
    this->value();
  }

  template <std::size_t N, avnd::callback Port>
  void operator()(const avnd::field_reflection<N, Port>)
  {
    this->value();
  }

  void operator()(auto&&) = delete;
};

template <typename Info>
struct Metadata<Process::Descriptor_k, oscr::ProcessModel<Info>>
{
  static std::vector<Process::PortType> inletDescription()
  {
    ProcessPortVisitor vis;
    avnd::input_introspection<Info>::for_all(vis);
    return vis.port;
  }
  static std::vector<Process::PortType> outletDescription()
  {
    ProcessPortVisitor vis;
    avnd::output_introspection<Info>::for_all(vis);
    return vis.port;
  }
  static Process::ProcessCategory kind() noexcept
  {
    Process::ProcessCategory cat;
    if constexpr(has_kind<Info>)
      cat = Info::kind();
    else
      cat = Process::ProcessCategory::Other;

    if constexpr(avnd::tag_deprecated<Info>)
      cat = Process::ProcessCategory(cat | Process::ProcessCategory::Deprecated);
    return cat;
  }
  static Process::Descriptor get()
  {
// literate programming goes brr
#if defined(_MSC_VER)
#define if_exists(Expr, Else) \
  []() noexcept {             \
    if(false)                 \
    {                         \
    }                         \
    Else;                     \
  }()
#define if_attribute(Attr) QString{}
#else
#define if_exists(Expr, Else)        \
  []() noexcept {                    \
    if constexpr(requires { Expr; }) \
      return Expr;                   \
    Else;                            \
  }()

#define if_attribute(Attr)                             \
  []() noexcept -> QString {                           \
    if constexpr(avnd::has_##Attr<Info>)               \
      return fromStringView(avnd::get_##Attr<Info>()); \
    else                                               \
      return QString{};                                \
  }()
#endif
    static Process::Descriptor desc{
        Metadata<PrettyName_k, oscr::ProcessModel<Info>>::get(),
        kind(),
        if_attribute(category),
        if_attribute(description),
        if_attribute(author),
        Metadata<Tags_k, oscr::ProcessModel<Info>>::get(),
        inletDescription(),
        outletDescription(),
        if_attribute(manual_url)};
    return desc;
  }
};
template <typename Info>
struct Metadata<Process::ProcessFlags_k, oscr::ProcessModel<Info>>
{
  static Process::ProcessFlags get() noexcept
  {
    if constexpr(requires { Info::flags(); })
    {
      return Info::flags();
    }
    else
    {
      Process::ProcessFlags flags{};
      flags |= Process::ProcessFlags::ControlSurface;

      if constexpr(avnd::tag_temporal<Info>)
        flags |= Process::ProcessFlags::SupportsTemporal;
      else
        flags |= Process::ProcessFlags::SupportsLasting;

      if constexpr(avnd::tag_single_exec<Info>)
        flags |= Process::ProcessFlags::SupportsState;

      if constexpr(avnd::tag_fully_custom_item<Info>)
        flags |= Process::ProcessFlags::FullyCustomItem;

      if constexpr(avnd::dynamic_ports_input_introspection<Info>::size > 0)
        flags |= Process::ProcessFlags::DynamicPorts;

      if constexpr(avnd::dynamic_ports_output_introspection<Info>::size > 0)
        flags |= Process::ProcessFlags::DynamicPorts;

      return flags;
    }
  }
};
template <typename Info>
struct Metadata<ObjectKey_k, oscr::ProcessModel<Info>>
{
  static constexpr auto get() noexcept { return avnd::get_c_name<Info>().data(); }
};
template <typename Info>
struct Metadata<ConcreteKey_k, oscr::ProcessModel<Info>>
{
  static Q_DECL_RELAXED_CONSTEXPR UuidKey<Process::ProcessModel> get()
  {
    return oscr::uuid_from_string<Info>();
  }
};
