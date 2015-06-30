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
class BaseScenario;
namespace OSSIA
{
    class Scenario;
    class TimeProcess;
}

class OSSIABaseScenarioElement :public iscore::ElementPluginModel
{
    public:
        OSSIABaseScenarioElement(
                const BaseScenario* element,
                QObject* parent);

        static constexpr iscore::ElementPluginModelType staticPluginId() { return 5; }
        iscore::ElementPluginModelType elementPluginId() const;

        iscore::ElementPluginModel* clone(
                const QObject* element,
                QObject* parent) const override;

        void serialize(const VisitorVariant&) const;


    private:
        OSSIAConstraintElement* m_ossia_constraint{};
        OSSIATimeNodeElement* m_ossia_startTimeNode{};
        OSSIATimeNodeElement* m_ossia_endTimeNode{};
        OSSIAEventElement* m_ossia_startEvent{};
        OSSIAEventElement* m_ossia_endEvent{};

};
