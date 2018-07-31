// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ApplicationComponents.hpp"

#include <score/command/CommandGeneratorMap.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/plugins/PluginInstances.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <score/tools/exceptions/MissingCommand.hpp>

namespace score
{
ApplicationComponentsData::ApplicationComponentsData() = default;
ApplicationComponentsData::~ApplicationComponentsData()
{
  for (auto& sub_map : commands)
    for (auto& pr : sub_map.second)
      delete pr.second;
  /*
   for(auto& elt : settings)
   {
       delete elt;
   }*/

  for (auto& elt : guiAppPlugins)
  {
    delete elt;
  }

  for (auto& elt : appPlugins)
  {
    delete elt;
  }

  const auto& static_plugs = score::staticPlugins();
  for (auto& elt : addons)
  {
    if (elt.plugin && ossia::find(static_plugs, elt.plugin) == static_plugs.end())
    {
      if(auto obj = dynamic_cast<QObject*>(elt.plugin))
      {
        obj->deleteLater();
      }
      else
      {
        delete obj;
      }
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

#if defined(SCORE_DEBUG)
  qDebug() << "ALERT: Command" << cmd.parentKey << "::" << cmd.commandKey
           << "could not be instantiated.";
  SCORE_ABORT;
#else
  throw MissingCommandException(cmd.parentKey, cmd.commandKey);
#endif
  return nullptr;
}
}
