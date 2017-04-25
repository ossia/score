#include <iscore/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <iscore/tools/exceptions/MissingCommand.hpp>

#include "ApplicationComponents.hpp"
#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/plugins/application/GUIApplicationPlugin.hpp>
#include <iscore/plugins/panel/PanelDelegate.hpp>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

namespace iscore
{
ApplicationComponentsData::ApplicationComponentsData() = default;
ApplicationComponentsData::~ApplicationComponentsData()
{
  for(auto& sub_map : commands) 
    for(auto& pr : sub_map.second)
      delete pr.second;
    /*
     for(auto& elt : settings)
     {
         delete elt;
     }*/

  for (auto& elt : guiAppPlugins)
  {
    // TODO some have the presenter in parent,
    // check that it won't cause crashes.
    delete elt;
  }

  // FIXME do not delete static plug-ins ?
  for (auto& elt : addons)
  {
    if (elt.plugin)
    {
      dynamic_cast<QObject*>(elt.plugin)->deleteLater();
    }
  }
}

Command*
ApplicationComponents::instantiateUndoCommand(const CommandData& cmd) const
{
  auto it = m_data.commands.find(cmd.parentKey);
  if (it != m_data.commands.end())
  {
    auto it2 = it->second.find(cmd.commandKey);
    if (it2 != it->second.end())
    {
      return (*it2->second)(cmd.data);
    }
  }

#if defined(ISCORE_DEBUG)
  qDebug() << "ALERT: Command" << cmd.parentKey << "::" << cmd.commandKey
           << "could not be instantiated.";
  ISCORE_ABORT;
#else
  throw MissingCommandException(cmd.parentKey, cmd.commandKey);
#endif
  return nullptr;
}
}
