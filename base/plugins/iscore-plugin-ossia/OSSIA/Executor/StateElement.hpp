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
    class State;
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
                std::shared_ptr<OSSIA::State> root,
                const RecreateOnPlay::Context& deviceList,
                QObject* parent);

        const Scenario::StateModel& iscoreState() const;
        std::shared_ptr<OSSIA::State> OSSIAState() const
        { return m_ossia_state; }

    private:
        void on_stateUpdated();

        const Scenario::StateModel& m_iscore_state;
        std::shared_ptr<OSSIA::State> m_ossia_state;

        const RecreateOnPlay::Context& m_context;
};
}
