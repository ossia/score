#pragma once
#include <QObject>
#include <memory>
#include <State/State.hpp>
#include <QHash>
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

        const StateModel* iscoreState() const;
        const QHash<iscore::State, std::shared_ptr<OSSIA::State> > &states() const;

        void addState(const iscore::State& is, std::shared_ptr<OSSIA::State>);
        void removeState(const iscore::State &);

    private:
        QHash<iscore::State, std::shared_ptr<OSSIA::State>> m_states;
        const StateModel* m_iscore_state{};
};
