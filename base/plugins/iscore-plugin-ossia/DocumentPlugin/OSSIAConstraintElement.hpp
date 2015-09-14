#pragma once
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <memory>

class ConstraintModel;
class Process;
class OSSIAProcessElement;
namespace OSSIA
{
    class TimeConstraint;
}
class OSSIAConstraintElement : public QObject
{
    public:
        OSSIAConstraintElement(
                std::shared_ptr<OSSIA::TimeConstraint> ossia_cst,
                ConstraintModel& iscore_cst,
                QObject* parent);

        std::shared_ptr<OSSIA::TimeConstraint> constraint() const;


        void play();
        void stop();

        void executionStarted();
        void executionStopped();

    private slots:
        void on_processAdded(const Process& id);
        void on_processRemoved(const Process& id);

    private:
        ConstraintModel& m_iscore_constraint;
        std::shared_ptr<OSSIA::TimeConstraint> m_ossia_constraint;

        std::map<Id<Process>, OSSIAProcessElement*> m_processes;

};
