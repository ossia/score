#pragma once
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <memory>

#include "OSSIAConstraintElement.hpp"
#include "OSSIATimeNodeElement.hpp"
#include "OSSIAEventElement.hpp"

class EventModel;
class ConstraintModel;
class TimeNodeModel;
class ScenarioModel;
namespace OSSIA
{
    class Scenario;
    class TimeProcess;
}
class OSSIAProcessElement :public iscore::ElementPluginModel
{
    public:
        using iscore::ElementPluginModel::ElementPluginModel;
        virtual std::shared_ptr<OSSIA::TimeProcess> process() const = 0;

};

class OSSIAScenarioElement : public OSSIAProcessElement
{
    public:
        OSSIAScenarioElement(
                const ScenarioModel* element,
                QObject* parent);

        std::shared_ptr<OSSIA::TimeProcess> process() const override;
        std::shared_ptr<OSSIA::Scenario> scenario() const;

        static constexpr iscore::ElementPluginModelType staticPluginId() { return 4; }
        iscore::ElementPluginModelType elementPluginId() const;

        iscore::ElementPluginModel*clone(
                const QObject* element,
                QObject* parent) const override;

        void serialize(const VisitorVariant&) const;

    private slots:
        void on_constraintCreated(const id_type<ConstraintModel>& id);

        void on_eventCreated(const id_type<EventModel>& id);
        void on_timeNodeCreated(const id_type<TimeNodeModel>& id);

        void on_constraintMoved(const id_type<ConstraintModel>& id);
        void on_eventMoved(const id_type<EventModel>& id);

        void on_constraintRemoved(const id_type<ConstraintModel>& id);
        void on_eventRemoved(const id_type<EventModel>& id);
        void on_timeNodeRemoved(const id_type<TimeNodeModel>& id);

    private:
        std::map<id_type<ConstraintModel>, OSSIAConstraintElement*> m_ossia_constraints;
        std::map<id_type<TimeNodeModel>, OSSIATimeNodeElement*> m_ossia_timenodes;
        std::map<id_type<EventModel>, OSSIAEventElement*> m_ossia_timeevents;
        std::shared_ptr<OSSIA::Scenario> m_ossia_scenario;
        const ScenarioModel* m_iscore_scenario{};
};
