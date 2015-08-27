#include "OSSIAStateElement.hpp"
#include <iscore/tools/Todo.hpp>

OSSIAStateElement::OSSIAStateElement(
        const StateModel &element,
        std::shared_ptr<OSSIA::State> root,
        QObject *parent):
    QObject{parent},
    m_iscore_state{element},
    m_ossia_rootState{root}
{
}

const StateModel &OSSIAStateElement::iscoreState() const
{
    return m_iscore_state;
}
