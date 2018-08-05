#pragma once
#include <QByteArray>
#include <algorithm>
#include <iterator>
#include <score/command/CommandData.hpp>
#include <score/command/CommandGeneratorMap.hpp>
#include <score/plugins/Addon.hpp>
#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <score/plugins/customfactory/FactoryInterface.hpp>
#include <score/plugins/customfactory/StringFactoryKey.hpp>
#include <score/tools/Todo.hpp>
#include <score/tools/std/HashMap.hpp>
#include <score/tools/std/IndirectContainer.hpp>
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

struct SCORE_LIB_BASE_EXPORT ApplicationComponentsData
{
  ApplicationComponentsData();
  ~ApplicationComponentsData();
  ApplicationComponentsData(const ApplicationComponentsData&) = delete;
  ApplicationComponentsData(ApplicationComponentsData&&) = delete;
  ApplicationComponentsData& operator=(const ApplicationComponentsData&)
      = delete;
  ApplicationComponentsData& operator=(ApplicationComponentsData&&) = delete;

  std::vector<score::Addon> addons;
  std::vector<ApplicationPlugin*> appPlugins;
  std::vector<GUIApplicationPlugin*> guiAppPlugins;

  score::hash_map<score::InterfaceKey, std::unique_ptr<InterfaceListBase>>
      factories;
  score::hash_map<CommandGroupKey, CommandGeneratorMap> commands;
  std::vector<std::unique_ptr<PanelDelegate>> panels;
};

class SCORE_LIB_BASE_EXPORT ApplicationComponents
{
public:
  ApplicationComponents(const score::ApplicationComponentsData& d) : m_data(d)
  {
  }

  // Getters for plugin-registered things
  const auto& applicationPlugins() const
  {
    return m_data.appPlugins;
  }
  const auto& guiApplicationPlugins() const
  {
    return m_data.guiAppPlugins;
  }
  const auto& addons() const
  {
    return m_data.addons;
  }

  template <typename T>
  T& applicationPlugin() const
  {
    for (auto& elt : m_data.appPlugins)
    {
      if (auto c = dynamic_cast<T*>(elt))
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
    for (auto& elt : m_data.guiAppPlugins)
    {
      if (auto c = dynamic_cast<T*>(elt))
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
    for (auto& elt : m_data.appPlugins)
    {
      if (auto c = dynamic_cast<T*>(elt))
      {
        return c;
      }
    }

    return nullptr;
  }

  template <typename T>
  T* findGuiApplicationPlugin() const
  {
    for (auto& elt : m_data.guiAppPlugins)
    {
      if (auto c = dynamic_cast<T*>(elt))
      {
        return c;
      }
    }

    return nullptr;
  }

  auto panels() const
  {
    return wrap_indirect(m_data.panels);
  }

  template <typename T>
  T& panel() const
  {
    for (auto& elt : m_data.panels)
    {
      if (auto c = dynamic_cast<T*>(elt.get()))
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
    for (auto& elt : m_data.panels)
    {
      if (auto c = dynamic_cast<T*>(elt.get()))
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
        T::factory_list_tag,
        "This needs to be called with a factory list class");
    auto it = m_data.factories.find(T::static_interfaceKey());
    if (it != m_data.factories.end())
    {
      return safe_cast<T*>(it->second.get());
    }

    return nullptr;
  }

  template <typename T>
  const T& interfaces() const
  {
    static_assert(
        T::factory_list_tag,
        "This needs to be called with a factory list class");
    auto it = m_data.factories.find(T::static_interfaceKey());
    if (it != m_data.factories.end())
    {
      return *safe_cast<T*>(it->second.get());
    }

    SCORE_ABORT;
    throw;
  }

  score::Command* instantiateUndoCommand(const CommandData& cmd) const;

private:
  const score::ApplicationComponentsData& m_data;
};

SCORE_LIB_BASE_EXPORT const ApplicationComponents& AppComponents();
}
