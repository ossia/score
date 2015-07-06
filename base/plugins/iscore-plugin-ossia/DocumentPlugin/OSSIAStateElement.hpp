#pragma once
#include <QObject>
#include <memory>
#include <API/Headers/Editor/TimeEvent.h>
class StateModel;
namespace OSSIA
{
    class State;
}
class OSSIAStateElement : public QObject
{
    public:
        OSSIAStateElement(
                const StateModel* element,
                QObject* parent);

        QList<std::shared_ptr<OSSIA::State>> states() const;
        void addState(std::shared_ptr<OSSIA::State>);
        void removeState(std::shared_ptr<OSSIA::State>);

        void handleEventTriggering(OSSIA::TimeEvent::Status);

    private:
        QList<std::shared_ptr<OSSIA::State>> m_states;
};
