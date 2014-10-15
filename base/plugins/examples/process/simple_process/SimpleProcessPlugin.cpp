#include <SimpleProcessPlugin.hpp>
#include <HelloWorldProcess.hpp>

#define PROCESS_NAME "HelloWorld Process"
QStringList SimpleProcessPlugin::process_list() const
{
	return {PROCESS_NAME};
}

std::unique_ptr<iscore::Process> SimpleProcessPlugin::process_make(QString name)
{
	if(name == QString(PROCESS_NAME))
	{
		return std::make_unique<HelloWorldProcess>();
	}

	return std::unique_ptr<iscore::Process>();
}
