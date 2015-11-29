#include <Scenario/Document/State/StateModel.hpp>
#include <vector>

#include "Editor/State.h"
#include "OSSIAStateElement.hpp"
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include "iscore2OSSIA.hpp"

OSSIAStateElement::OSSIAStateElement(
        const StateModel &element,
        std::shared_ptr<OSSIA::State> root,
        const DeviceList& deviceList,
        QObject *parent):
    QObject{parent},
    m_iscore_state{element},
    m_ossia_state{root},
    m_deviceList{deviceList}
{
}

const StateModel &OSSIAStateElement::iscoreState() const
{
    return m_iscore_state;
}

void OSSIAStateElement::recreate()
{
    iscore::convert::state(
                m_ossia_state,
                m_iscore_state.messages().rootNode(),
                m_deviceList);
}

void OSSIAStateElement::clear()
{
    m_ossia_state->stateElements().clear();
}

void OSSIAStateElement::on_stateUpdated()
{
    clear();
    recreate();
}
