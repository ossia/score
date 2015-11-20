#include "StateElement.hpp"

#include <Scenario/Document/State/StateModel.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <iscore2OSSIA.hpp>
#include <iscore/tools/Todo.hpp>
namespace RecreateOnPlay
{
StateElement::StateElement(
        const StateModel &element,
        std::shared_ptr<OSSIA::State> root,
        const DeviceList& deviceList,
        QObject *parent):
    QObject{parent},
    m_iscore_state{element},
    m_ossia_state{root},
    m_deviceList{deviceList}
{
    iscore::convert::state(
                m_ossia_state,
                m_iscore_state.messages().rootNode(),
                m_deviceList);
}

const StateModel &StateElement::iscoreState() const
{
    return m_iscore_state;
}

}
