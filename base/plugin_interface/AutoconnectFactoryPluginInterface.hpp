#pragma once
#include <QStringList>
#include <interface/autoconnect/Autoconnect.hpp>


namespace iscore
{
	class AutoconnectFactoryPluginInterface
	{
		public:
			virtual ~AutoconnectFactoryPluginInterface() = default;
			// List the Processes offered by the plugin.

			virtual QStringList process_list() const = 0;
			virtual std::unique_ptr<iscore::Process> process_make(QString name) = 0;
	};
}

#define ProcessFactoryPluginInterface_iid "org.ossia.i-score.plugins.ProcessFactoryPluginInterface"

Q_DECLARE_INTERFACE(iscore::ProcessFactoryPluginInterface, ProcessFactoryPluginInterface_iid)
