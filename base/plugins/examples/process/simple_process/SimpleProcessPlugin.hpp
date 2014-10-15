#pragma once

#include <plugin_interface/ProcessFactoryPluginInterface.hpp>
#include <QObject>

class SimpleProcessPlugin : public QObject, public iscore::ProcessFactoryPluginInterface
{
		Q_OBJECT
		Q_PLUGIN_METADATA(IID ProcessFactoryPluginInterface_iid)
		Q_INTERFACES(iscore::ProcessFactoryPluginInterface)

	public:
		SimpleProcessPlugin():
			QObject{},
			iscore::ProcessFactoryPluginInterface{}
		{
			setObjectName("SimpleProcessPlugin");
		}

		virtual QStringList process_list() const override;
		virtual std::unique_ptr<iscore::Process> process_make(QString name) override;
};
