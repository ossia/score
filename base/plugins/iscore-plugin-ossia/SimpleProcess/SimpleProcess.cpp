#include "SimpleProcess.hpp"
#include <OSSIA2iscore.hpp>

std::shared_ptr<OSSIA::StateElement> SimpleProcess::state(
        const OSSIA::TimeValue& t,
        const OSSIA::TimeValue&)
{
    qDebug() << OSSIA::convert::time(t);
    auto state = OSSIA::State::create();
    return state;
}
