#pragma once
#include <QStringList>
#include <tools/NamedObject.hpp>
#include <vector>

namespace iscore{
	class FactoryInterface;
}
//namespace iscore
//{
// TODO : put in Scenario.
class ProcessFactoryInterface;
/**
 * @brief The ProcessList class
 *
 * Contains the list of the process plug-ins that can be loaded.
 */
class ProcessList : public NamedObject
{
		Q_OBJECT
	public:
		ProcessList(NamedObject* parent);

		QStringList getProcessesName() const;
		ProcessFactoryInterface* getProcess(QString);
		void addProcess(iscore::FactoryInterface*);

		static ProcessFactoryInterface* getFactory(QString processName);

	private:
		std::vector<ProcessFactoryInterface*> m_processes;
};
//}
