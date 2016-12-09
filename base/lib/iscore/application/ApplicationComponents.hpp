#pragma once
#include <iscore/command/CommandData.hpp>
#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/plugins/Addon.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore/tools/std/IndirectContainer.hpp>
#include <iscore_lib_base_export.h>

#include <QByteArray>
#include <algorithm>
#include <iterator>
#include <iscore/tools/std/HashMap.hpp>
#include <utility>
#include <vector>

namespace iscore
{
class DocumentDelegateFactory;
class DocumentPluginFactory;
class InterfaceListBase;
class Plugin_QtInterface;
class GUIApplicationContextPlugin;
class PanelDelegate;

struct ISCORE_LIB_BASE_EXPORT ApplicationComponentsData
{
  ApplicationComponentsData();
  ~ApplicationComponentsData();
  ApplicationComponentsData(const ApplicationComponentsData&) = delete;
  ApplicationComponentsData(ApplicationComponentsData&&) = delete;
  ApplicationComponentsData& operator=(const ApplicationComponentsData&)
      = delete;
  ApplicationComponentsData& operator=(ApplicationComponentsData&&) = delete;

  std::vector<iscore::Addon> addons;
  std::vector<GUIApplicationContextPlugin*> appPlugins;

  iscore::hash_map<iscore::InterfaceKey, std::unique_ptr<InterfaceListBase>>
          factories;
  iscore::hash_map<CommandGroupKey, CommandGeneratorMap> commands;
  std::vector<std::unique_ptr<PanelDelegate>> panels;
};

class ISCORE_LIB_BASE_EXPORT ApplicationComponents
{
public:
  ApplicationComponents(const iscore::ApplicationComponentsData& d) : m_data(d)
  {
  }

  // Getters for plugin-registered things
  const auto& applicationPlugins() const
  {
    return m_data.appPlugins;
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

    ISCORE_ABORT;
    throw;
  }

  auto panels() const
  {
    return wrap_indirect(m_data.panels);
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

    ISCORE_ABORT;
    throw;
  }

  iscore::Command*
  instantiateUndoCommand(const CommandData& cmd) const;

private:
  const iscore::ApplicationComponentsData& m_data;
};

ISCORE_LIB_BASE_EXPORT const ApplicationComponents& AppComponents();
}
