#pragma once

#include <plugin_interface/ProcessFactoryPluginInterface.h>
#include <QObject>

class SimpleProcessPlugin : public QObject, public iscore::ProcessFactoryPluginInterface
{
	Q_OBJECT
    Q_PLUGIN_METADATA(IID ProcessFactoryPluginInterface_iid)
	Q_INTERFACES(iscore::ProcessFactoryPluginInterface)
		
public:
    virtual QStringList process_list() const override;
    virtual std::unique_ptr<iscore::Process> process_make(QString name) override;
};
