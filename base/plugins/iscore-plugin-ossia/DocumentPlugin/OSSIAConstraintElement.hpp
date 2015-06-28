#pragma once
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <memory>

class ConstraintModel;
class ProcessModel;
class OSSIAProcessElement;
namespace OSSIA
{
    class TimeConstraint;
}
class OSSIAConstraintElement : public iscore::ElementPluginModel
{
    public:
        static constexpr iscore::ElementPluginModelType staticPluginId() { return 1; }
        OSSIAConstraintElement(
                std::shared_ptr<OSSIA::TimeConstraint> ossia_cst,
                const ConstraintModel& iscore_cst,
                QObject* parent);

        std::shared_ptr<OSSIA::TimeConstraint> constraint() const;

        iscore::ElementPluginModelType elementPluginId() const;

        iscore::ElementPluginModel*clone(
                const QObject* element,
                QObject* parent) const override;

        void serialize(const VisitorVariant&) const;

    private slots:
        void on_processAdded(const QString& name,
                             const id_type<ProcessModel>& id);
        void on_processRemoved(const id_type<ProcessModel>& id);

    private:
        const ConstraintModel& m_iscore_constraint;
        std::shared_ptr<OSSIA::TimeConstraint> m_ossia_constraint;

        std::map<id_type<ProcessModel>, OSSIAProcessElement*> m_processes;

};
