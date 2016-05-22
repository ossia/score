#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>
#include <iscore/tools/exceptions/MissingCommand.hpp>

#include "ApplicationComponents.hpp"
#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/plugins/panel/PanelDelegate.hpp>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

namespace iscore
{

ApplicationComponentsData::~ApplicationComponentsData()
{/*
    for(auto& elt : settings)
    {
        delete elt;
    }*/

    for(auto& elt : appPlugins)
    {
        // TODO some have the presenter in parent,
        // check that it won't cause crashes.
        delete elt;
    }

    // FIXME do not delete static plug-ins ?
    for(auto& elt : plugins)
    {
        if(elt)
        {
            dynamic_cast<QObject*>(elt)->deleteLater();
        }
    }
}

SerializableCommand*ApplicationComponents::instantiateUndoCommand(const CommandData& cmd) const
{
    auto it = m_data.commands.find(cmd.parentKey);
    if(it != m_data.commands.end())
    {
        auto it2 = it->second.find(cmd.commandKey);
        if(it2 != it->second.end())
        {
            return (*it2->second)(cmd.data);
        }
    }

#if defined(ISCORE_DEBUG)
    qDebug() << "ALERT: Command"
             << cmd.parentKey
             << "::"
             << cmd.commandKey
             << "could not be instantiated.";
    ISCORE_ABORT;
#else
    throw MissingCommandException(cmd.parentKey, cmd.commandKey);
#endif
    return nullptr;
}
}
