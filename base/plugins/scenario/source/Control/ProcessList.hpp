#pragma once
#include <QStringList>
#include <tools/NamedObject.hpp>
#include <vector>

namespace iscore{
	class FactoryInterface;
}

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

		ProcessFactoryInterface* getProcess(QString);
		void addProcess(iscore::FactoryInterface*);

		static QStringList getProcessesName();
		static ProcessFactoryInterface* getFactory(QString processName);

	private:
		QStringList getProcessesName_impl() const;
		std::vector<ProcessFactoryInterface*> m_processes;
};