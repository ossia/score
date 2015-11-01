#include "ProcessList.hpp"
#include <Process/ProcessFactory.hpp>

ProcessList::ProcessList(NamedObject* parent):
    NamedObject {"ProcessList", parent}
{

}

QStringList ProcessList::getProcessesName_impl() const
{
    QStringList lst;

    for(auto& elt : m_processes)
    {
        lst.append(elt->name());
    }

    return lst;
}

ProcessFactory* ProcessList::getProcess(const QString &name)
{
    auto it = std::find_if(m_processes.begin(),
                           m_processes.end(),
                           [&name](ProcessFactory * p)
    {
        return p->name() == name;
    });

    return it != m_processes.end() ? *it : nullptr;
}

void ProcessList::registerProcess(iscore::FactoryInterface* arg)
{
    auto p = static_cast<ProcessFactory*>(arg);
    auto it = std::find_if(m_processes.begin(),
                           m_processes.end(),
                           [&p](ProcessFactory * inner_p)
    {
        return inner_p->name() == p->name();
    });

    if(it == m_processes.end())
    {
        m_processes.push_back(p);
    }
    else
    {
        qDebug() << "Alert : a process with the name" << p->name() << "already exists.";
    }
}

#include <QApplication>
ProcessFactory* ProcessList::getFactory(QString processName)
{
    return qApp
           ->findChild<ProcessList*> ("ProcessList")
           ->getProcess(processName);
}


QStringList ProcessList::getProcessesName()
{
    return qApp
           ->findChild<ProcessList*> ("ProcessList")
           ->getProcessesName_impl();
}
