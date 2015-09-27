#pragma once

#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <memory>

#include "OSSIAConstraintElement.hpp"
#include "OSSIATimeNodeElement.hpp"
#include "OSSIAEventElement.hpp"
#include "OSSIAStateElement.hpp"

class EventModel;
class ConstraintModel;
class TimeNodeModel;
class BaseScenario;
namespace OSSIA
{
    class Scenario;
    class TimeProcess;
}

class OSSIABaseScenarioElement : public QObject
{
    public:
        OSSIABaseScenarioElement(
                const BaseScenario* element,
                QObject* parent);

        OSSIAConstraintElement* baseConstraint() const;

        OSSIATimeNodeElement* startTimeNode() const;
        OSSIATimeNodeElement* endTimeNode() const;

        OSSIAEventElement* startEvent() const;
        OSSIAEventElement* endEvent() const;

        OSSIAStateElement* startState() const;
        OSSIAStateElement* endState() const;

    private:
        OSSIAConstraintElement* m_ossia_constraint{};

        OSSIATimeNodeElement* m_ossia_startTimeNode{};
        OSSIATimeNodeElement* m_ossia_endTimeNode{};

        OSSIAEventElement* m_ossia_startEvent{};
        OSSIAEventElement* m_ossia_endEvent{};

        OSSIAStateElement* m_ossia_startState{};
        OSSIAStateElement* m_ossia_endState{};

        const DeviceList& m_deviceList;
};
