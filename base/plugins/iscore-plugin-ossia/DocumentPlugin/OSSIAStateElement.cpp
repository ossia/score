#include "OSSIAStateElement.hpp"
#include <iscore/tools/Todo.hpp>
#include "iscore2OSSIA.hpp"
#include "Document/State/StateModel.hpp"
#include "Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp"

OSSIAStateElement::OSSIAStateElement(
        const StateModel &element,
        std::shared_ptr<OSSIA::State> root,
        const DeviceList& deviceList,
        QObject *parent):
    QObject{parent},
    m_iscore_state{element},
    m_ossia_rootState{root},
    m_deviceList{deviceList}
{
    con(m_iscore_state, &StateModel::sig_statesUpdated, this,
        &OSSIAStateElement::on_stateUpdated);

    on_stateUpdated();
}

const StateModel &OSSIAStateElement::iscoreState() const
{
    return m_iscore_state;
}

void OSSIAStateElement::on_stateUpdated()
{
    m_ossia_rootState->stateElements().clear();
    iscore::convert::state(
                m_ossia_rootState,
                m_iscore_state.messages().rootNode(),
                m_deviceList);
}
