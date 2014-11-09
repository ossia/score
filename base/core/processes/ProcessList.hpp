#pragma once
#include <QStringList>
#include <vector>

namespace iscore
{
	class ProcessFactoryInterface;
	/**
	 * @brief The ProcessList class
	 *
	 * Contains the list of the process plug-ins that can be loaded.
	 */
	class ProcessList
	{
		public:
			QStringList getProcessesName() const;
			ProcessFactoryInterface* getProcess(QString);
			void addProcess(ProcessFactoryInterface*);

		private:
			std::vector<ProcessFactoryInterface*> m_processes;
	};
}
