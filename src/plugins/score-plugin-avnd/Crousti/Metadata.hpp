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

template <typename Info>
struct Metadata<Process::Descriptor_k, oscr::ProcessModel<Info>>
{
  static std::vector<Process::PortType> inletDescription()
  {
    std::vector<Process::PortType> port;
    /*
    for (std::size_t i = 0; i < info::audio_in_count; i++)
      port.push_back(Process::PortType::Audio);
    for (std::size_t i = 0; i < info::midi_in_count; i++)
      port.push_back(Process::PortType::Midi);
    for (std::size_t i = 0; i < info::value_in_count; i++)
      port.push_back(Process::PortType::Message);
    for (std::size_t i = 0; i < info::control_in_count; i++)
      port.push_back(Process::PortType::Message);
    */
    return port;
  }
  static std::vector<Process::PortType> outletDescription()
  {
    std::vector<Process::PortType> port;
    /*
    for (std::size_t i = 0; i < info::audio_out_count; i++)
      port.push_back(Process::PortType::Audio);
    for (std::size_t i = 0; i < info::midi_out_count; i++)
      port.push_back(Process::PortType::Midi);
    for (std::size_t i = 0; i < info::value_out_count; i++)
      port.push_back(Process::PortType::Message);
    for (std::size_t i = 0; i < info::control_out_count; i++)
      port.push_back(Process::PortType::Message);
    */
    return port;
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
