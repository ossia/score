#pragma once
#include <QObject>
#include <memory>
#include <iscore_plugin_ossia_export.h>

namespace Device
{
class DeviceList;
}

namespace OSSIA
{
    class TimeEvent;
}
namespace Scenario
{
class StateModel;
}
namespace RecreateOnPlay
{
struct Context;
class ISCORE_PLUGIN_OSSIA_EXPORT StateElement final : public QObject
{
    public:
        StateElement(
                const Scenario::StateModel& element,
                OSSIA::TimeEvent& root,
                const RecreateOnPlay::Context& deviceList,
                QObject* parent);

        const Scenario::StateModel& iscoreState() const;

    private:
        const Scenario::StateModel& m_iscore_state;
        OSSIA::TimeEvent& m_root;

        const RecreateOnPlay::Context& m_context;
};
}
