#pragma once
#include <QStringList>
#include <interface/processes/Process.h>

// Note : here we don't use camel case for the "process_" part because it is more of a namespacing required 
// by the fact that if a plugin implements for instance a process and a panel, 
// Qt requires the creation of the class which inherits both the process interface and the panel interface
// which might cause name collisions.
namespace iscore
{
	class ProcessFactoryPluginInterface
	{
		public:
			virtual ~ProcessFactoryPluginInterface() = default;
			// List the Processes offered by the plugin.

            virtual QStringList process_list() const = 0;
			virtual std::unique_ptr<iscore::Process> process_make(QString name) = 0;
	};
}

#define ProcessFactoryPluginInterface_iid "org.ossia.i-score.plugins.ProcessFactoryPluginInterface"

Q_DECLARE_INTERFACE(iscore::ProcessFactoryPluginInterface, ProcessFactoryPluginInterface_iid)
