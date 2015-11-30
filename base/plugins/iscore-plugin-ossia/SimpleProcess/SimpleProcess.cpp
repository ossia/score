#include <OSSIA/OSSIA2iscore.hpp>
#include <QDebug>

#include <Process/TimeValue.hpp>
#include "SimpleProcess.hpp"

namespace OSSIA {
class StateElement;
}  // namespace OSSIA

std::shared_ptr<OSSIA::StateElement> SimpleProcess::state(
        const OSSIA::TimeValue& t,
        const OSSIA::TimeValue&)
{
    qDebug() << OSSIA::convert::time(t);
    auto state = OSSIA::State::create();
    return state;
}
