#include "OSSIAStateElement.hpp"
#include <iscore/tools/Todo.hpp>
OSSIAStateElement::OSSIAStateElement(
        std::shared_ptr<OSSIA::State> event,
        const StateModel *element,
        QObject *parent)
{
    ISCORE_TODO;
}

std::shared_ptr<OSSIA::State> OSSIAStateElement::state() const
{
    return m_state;
}
