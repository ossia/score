#include <core/processes/ProcessList.hpp>
#include <interface/processes/Process.hpp>
#include <algorithm>

using namespace iscore;

QStringList ProcessList::getProcessesName() const
{
	QStringList lst;
	for(auto& elt : m_processes)
		lst.append(elt->name());

	return lst;
}

Process* ProcessList::getProcess(QString name)
{
	auto it = std::find_if(m_processes.begin(),
						   m_processes.end(),
						   [&name] (Process* p)
	{ return p->name() == name;});

	return it != m_processes.end()? *it : nullptr;
}

void ProcessList::addProcess(Process* p)
{
	auto it = std::find_if(m_processes.begin(),
						   m_processes.end(),
						   [&p] (Process* inner_p)
	{ return inner_p->name() == p->name();});

	if(it == m_processes.end())
		m_processes.push_back(p);
	else
		qDebug("Alert : a process with the same name already exists.");
}
