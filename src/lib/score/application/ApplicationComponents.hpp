#pragma once
#include <score/command/CommandData.hpp>
#include <score/command/CommandGeneratorMap.hpp>
#include <score/plugins/Addon.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/tools/SafeCast.hpp>
#include <score/tools/std/HashMap.hpp>
#include <score/tools/std/IndirectContainer.hpp>

#include <ossia/detail/hash.hpp>

#include <score_lib_base_export.h>

#include <utility>
#include <vector>

namespace score
{
class DocumentDelegateFactory;
class DocumentPluginFactory;
class InterfaceListBase;
class Plugin_QtInterface;
class ApplicationPlugin;
class GUIApplicationPlugin;
class PanelDelegate;

using FindCommandKey = std::pair<CommandGroupKey, CommandKey>;
struct CommandKeyHash : ossia::hash<std::string>
{
  std::size_t operator()(const FindCommandKey& val) const noexcept
  {
    std::size_t seed = 0;
    ossia::hash_combine(seed, val.first.toString());
    ossia::hash_combine(seed, val.second.toString());
    return seed;
  }
};
struct CommandStore : ossia::hash_map<FindCommandKey, CommandFactory, CommandKeyHash>
{
public:
  using ossia::hash_map<FindCommandKey, CommandFactory, CommandKeyHash>::hash_map;
};

struct SCORE_LIB_BASE_EXPORT ApplicationComponentsData
{
  ApplicationComponentsData();
  ~ApplicationComponentsData();
  ApplicationComponentsData(const ApplicationComponentsData&) = delete;
  ApplicationComponentsData(ApplicationComponentsData&&) = delete;
  ApplicationComponentsData& operator=(const ApplicationComponentsData&) = delete;
  ApplicationComponentsData& operator=(ApplicationComponentsData&&) = delete;

  InterfaceListBase*
  findInterfaceList(const UuidKey<score::InterfaceBase>& k) const noexcept;

  std::vector<score::Addon> addons;
  std::vector<ApplicationPlugin*> appPlugins;
  std::vector<GUIApplicationPlugin*> guiAppPlugins;

  score::hash_map<score::InterfaceKey, std::unique_ptr<InterfaceListBase>> factories;

  CommandStore commands;
  std::vector<std::unique_ptr<PanelDelegate>> panels;
};

class SCORE_LIB_BASE_EXPORT ApplicationComponents
{
public:
  ApplicationComponents(const score::ApplicationComponentsData& d)
      : m_data(d)
  {
  }

  // Getters for plugin-registered things
  const auto& applicationPlugins() const { return m_data.appPlugins; }
  const auto& guiApplicationPlugins() const { return m_data.guiAppPlugins; }
  const auto& addons() const { return m_data.addons; }

  template <typename T>
  T& applicationPlugin() const
  {
    for(auto& elt : m_data.appPlugins)
    {
      if(auto c = dynamic_cast<T*>(elt))
      {
        return *c;
      }
    }

    SCORE_ABORT;
    throw;
  }

  template <typename T>
  T& guiApplicationPlugin() const
  {
    for(auto& elt : m_data.guiAppPlugins)
    {
      if(auto c = dynamic_cast<T*>(elt))
      {
        return *c;
      }
    }

    SCORE_ABORT;
    throw;
  }

  template <typename T>
  T* findApplicationPlugin() const
  {
    for(auto& elt : m_data.appPlugins)
    {
      if(auto c = dynamic_cast<T*>(elt))
      {
        return c;
      }
    }

    return nullptr;
  }

  template <typename T>
  T* findGuiApplicationPlugin() const
  {
    for(auto& elt : m_data.guiAppPlugins)
    {
      if(auto c = dynamic_cast<T*>(elt))
      {
        return c;
      }
    }

    return nullptr;
  }

  auto panels() const { return wrap_indirect(m_data.panels); }

  template <typename T>
  T& panel() const
  {
    for(auto& elt : m_data.panels)
    {
      if(auto c = dynamic_cast<T*>(elt.get()))
      {
        return *c;
      }
    }

    SCORE_ABORT;
    throw;
  }

  template <typename T>
  T* findPanel() const
  {
    for(auto& elt : m_data.panels)
    {
      if(auto c = dynamic_cast<T*>(elt.get()))
      {
        return c;
      }
    }

    return nullptr;
  }

  template <typename T>
  const T* findInterfaces() const
  {
    static_assert(
        T::factory_list_tag, "This needs to be called with a factory list class");

    return static_cast<T*>(m_data.findInterfaceList(T::static_interfaceKey()));
  }

  template <typename T>
  const T& interfaces() const
  {
    static_assert(
        T::factory_list_tag, "This needs to be called with a factory list class");

    if(auto ptr = m_data.findInterfaceList(T::static_interfaceKey()))
      return *safe_cast<T*>(ptr);

    SCORE_ABORT;
    throw;
  }

  score::Command* instantiateUndoCommand(const CommandData& cmd) const;

private:
  const score::ApplicationComponentsData& m_data;
};

SCORE_LIB_BASE_EXPORT const ApplicationComponents& AppComponents();
}
