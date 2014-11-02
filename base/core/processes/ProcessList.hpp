#pragma once
#include <QStringList>
#include <vector>

namespace iscore
{
	class Process;
	/**
	 * @brief The ProcessList class
	 * 
	 * Contains the list of the process plug-ins that can be loaded.
	 */
	class ProcessList
	{
		public:
			QStringList getProcessesName() const;
			Process* getProcess(QString);
			void addProcess(Process*);

		private:
			std::vector<Process*> m_processes;
	};
}
