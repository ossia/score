#pragma once
#include <QStringList>
#include <iscore/tools/NamedObject.hpp>
#include <vector>

namespace iscore
{
    class FactoryInterface;
}

class ProcessFactory;
/**
 * @brief The ProcessList class
 *
 * Contains the list of the process plug-ins that can be loaded.
 */
class ProcessList : public NamedObject
{
        Q_OBJECT
    public:
        explicit ProcessList(NamedObject* parent);

        ProcessFactory* getProcess(const QString&);
        void registerProcess(iscore::FactoryInterface*);

        static QStringList getProcessesName();
        static ProcessFactory* getFactory(QString processName);

    private:
        QStringList getProcessesName_impl() const;
        std::vector<ProcessFactory*> m_processes;
};
