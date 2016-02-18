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
    qDebug() << Ossia::convert::time(parent->getPosition());
    auto state = OSSIA::State::create();
    return state;
}


std::shared_ptr<OSSIA::StateElement> SimpleProcess::offset(const OSSIA::TimeValue & t)
{
    qDebug() << Ossia::convert::time(parent->getOffset() / parent->getDurationNominal());
    auto state = OSSIA::State::create();
    return state;
}
