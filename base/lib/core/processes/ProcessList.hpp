#pragma once
#include <QStringList>
#include <tools/NamedObject.hpp>
#include <vector>

namespace iscore
{
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
			void addProcess(ProcessFactoryInterface*);
			
			static iscore::ProcessFactoryInterface* getFactory(QString processName);

		private:
			std::vector<ProcessFactoryInterface*> m_processes;
	};
}
