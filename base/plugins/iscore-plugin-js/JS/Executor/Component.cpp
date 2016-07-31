#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <OSSIA/OSSIA2iscore.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>
#include <vector>

#include <ossia/editor/state/message.hpp>
#include <ossia/editor/state/state.hpp>
#include "JSAPIWrapper.hpp"
#include "Component.hpp"
#include <ossia/editor/scenario/time_constraint.hpp>
#include <JS/JSProcessModel.hpp>

namespace JS
{
namespace Executor
{
ProcessExecutor::ProcessExecutor(
        const Explorer::DeviceDocumentPlugin& devices):
    m_devices{devices.list()}
{
    auto obj = m_engine.newQObject(new JS::APIWrapper{m_engine, devices});
    m_engine.globalObject().setProperty("iscore", obj);
}

void ProcessExecutor::setTickFun(const QString& val)
{
    m_tickFun = m_engine.evaluate(val);
    if(m_tickFun.isError())
        qDebug()
                << "Uncaught exception at line"
                << m_tickFun.property("lineNumber").toInt()
                << ":" << m_tickFun.toString();

}

ossia::state_element ProcessExecutor::state()
{
    return state(parent->getPosition());
}

ossia::state_element ProcessExecutor::state(double t)
{
    ossia::state st;
    if(!m_tickFun.isCallable())
        return st;

    // 2. Get the value of the js fun
    auto messages = JS::convert::messages(m_tickFun.call({QJSValue{t}}));

    m_engine.collectGarbage();

    for(const auto& mess : messages)
    {
        st.add(iscore::convert::message(mess, m_devices));
    }

    // 3. Convert our value back
    return st;
}

ossia::state_element ProcessExecutor::offset(ossia::time_value off)
{
    return state(off / parent->getDurationNominal());
}

Component::Component(
        ::RecreateOnPlay::ConstraintElement& parentConstraint,
        JS::ProcessModel& element,
        const ::RecreateOnPlay::Context& ctx,
        const Id<iscore::Component>& id,
        QObject* parent):
    ::RecreateOnPlay::ProcessComponent_T<JS::ProcessModel>{parentConstraint, element, ctx, id, "JSComponent", parent}
{
    auto proc = std::make_shared<ProcessExecutor>(ctx.devices);
    proc->setTickFun(element.script());
    m_ossia_process = proc;
}
}
}
