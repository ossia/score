#include <OSSIA/OSSIA2iscore.hpp>
#include <QDebug>

#include <Process/TimeValue.hpp>
#include "SimpleProcess.hpp"
#include <Editor/TimeConstraint.h>
namespace OSSIA {
class StateElement;
}  // namespace OSSIA

std::shared_ptr<OSSIA::StateElement> SimpleProcess::state()
{
    qDebug() << Ossia::convert::time(getParentTimeConstraint()->getPosition());
    auto state = OSSIA::State::create();
    return state;
}
