#include <Scenario/Document/State/StateModel.hpp>
#include <OSSIA/iscore2OSSIA.hpp>

#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include "StateElement.hpp"
#include <ossia/editor/scenario/time_event.hpp>
namespace RecreateOnPlay
{
StateElement::StateElement(
        const Scenario::StateModel &element,
        ossia::time_event& root,
        const RecreateOnPlay::Context& ctx,
        QObject *parent):
    QObject{parent},
    m_iscore_state{element},
    m_root{root},
    m_context{ctx}
{
    m_root.addState(
                iscore::convert::state(
                    m_iscore_state,
                    m_context));
}

const Scenario::StateModel& StateElement::iscoreState() const
{
    return m_iscore_state;
}

}
