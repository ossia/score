#pragma once

#include <QObject>

class BaseScenario;
class DeviceList;
class OSSIAConstraintElement;
class OSSIAEventElement;
class OSSIAStateElement;
class OSSIATimeNodeElement;

namespace OSSIA
{
}

class OSSIABaseScenarioElement final : public QObject
{
    public:
        OSSIABaseScenarioElement(
                const BaseScenario& element,
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
