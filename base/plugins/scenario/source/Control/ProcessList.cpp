#include "ProcessList.hpp"
#include "ProcessInterface/ProcessFactoryInterface.hpp"
#include <algorithm>

ProcessList::ProcessList(NamedObject* parent):
	NamedObject{"ProcessList", parent}
{

}

QStringList ProcessList::getProcessesName_impl() const
{
	QStringList lst;
	for(auto& elt : m_processes)
		lst.append(elt->name());

	return lst;
}

ProcessFactoryInterface* ProcessList::getProcess(QString name)
{
	auto it = std::find_if(m_processes.begin(),
						   m_processes.end(),
						   [&name] (ProcessFactoryInterface* p)
	{ return p->name() == name;});

	return it != m_processes.end()? *it : nullptr;
}

void ProcessList::addProcess(iscore::FactoryInterface* arg)
{
	auto p = static_cast<ProcessFactoryInterface*>(arg);
	auto it = std::find_if(m_processes.begin(),
						   m_processes.end(),
						   [&p] (ProcessFactoryInterface* inner_p)
	{ return inner_p->name() == p->name();});

	if(it == m_processes.end())
		m_processes.push_back(p);
	else
		qDebug("Alert : a process with the same name already exists.");
}

#include <QApplication>
ProcessFactoryInterface* ProcessList::getFactory(QString processName)
{
	return qApp
			->findChild<ProcessList*>("ProcessList")
			->getProcess(processName);
}


QStringList ProcessList::getProcessesName()
{
	return qApp
			->findChild<ProcessList*>("ProcessList")
			->getProcessesName_impl();
}
