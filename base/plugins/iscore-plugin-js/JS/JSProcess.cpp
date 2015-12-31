#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <OSSIA/OSSIA2iscore.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <vector>

#include "Editor/Message.h"
#include "Editor/State.h"
#include "JSAPIWrapper.hpp"
#include "JSProcess.hpp"

namespace OSSIA {
class StateElement;
}  // namespace OSSIA


namespace JS
{
ProcessExecutor::ProcessExecutor(DeviceDocumentPlugin& devices):
    m_devices{devices.list()},
    m_start{OSSIA::State::create()},
    m_end{OSSIA::State::create()}
{
    m_engine.globalObject().setProperty("iscore", m_engine.newQObject(new JS::APIWrapper{devices}));
}

void ProcessExecutor::setTickFun(const QString& val)
{
    m_tickFun = m_engine.evaluate(val);
}

std::shared_ptr<OSSIA::StateElement> ProcessExecutor::state(
        const OSSIA::TimeValue& t,
        const OSSIA::TimeValue&)
{
    if(!m_tickFun.isCallable())
        return {};

    // 1. Convert the time in value.
    auto js_time = iscore::convert::JS::time(Ossia::convert::time(t));

    // 2. Get the value of the js fun
    auto messages = JS::convert::messages(m_tickFun.call({js_time}));

    m_engine.collectGarbage();

    auto st = OSSIA::State::create();
    for(const auto& mess : messages)
    {
        //qDebug() << mess.toString();
        auto ossia_mess = iscore::convert::message(mess, m_devices);
        if(ossia_mess)
            st->stateElements().push_back(ossia_mess);
    }

    // 3. Convert our value back
    return st;
}
}
