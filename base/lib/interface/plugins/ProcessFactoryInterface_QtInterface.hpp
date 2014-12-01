#pragma once
#include <QStringList>
#include <interface/process/ProcessFactoryInterface.hpp>

// Note : here we don't use camel case for the "process_" part because it is more of a namespacing required
// by the fact that if a plugin implements for instance a process and a panel,
// Qt requires the creation of the class which inherits both the process interface and the panel interface
// which might cause name collisions.
namespace iscore
{
	class ProcessFactoryInterface_QtInterface
	{
		public:
			virtual ~ProcessFactoryInterface_QtInterface() = default;
			// List the Processes offered by the plugin.

			virtual QStringList process_list() const = 0;
			virtual iscore::ProcessFactoryInterface* process_make(QString name) = 0;
	};
}

#define ProcessFactoryInterface_QtInterface_iid "org.ossia.i-score.plugins.ProcessFactoryInterface_QtInterface"

Q_DECLARE_INTERFACE(iscore::ProcessFactoryInterface_QtInterface, ProcessFactoryInterface_QtInterface_iid)
