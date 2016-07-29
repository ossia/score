#pragma once
#include <QObject>
#include <memory>
#include <iscore_plugin_ossia_export.h>

namespace Device
{
class DeviceList;
}
namespace ossia
{
class time_node;
class time_value;
}
namespace Scenario
{
class TimeNodeModel;
}

namespace RecreateOnPlay
{
class ISCORE_PLUGIN_OSSIA_EXPORT TimeNodeElement final : public QObject
{
    public:
        TimeNodeElement(
                std::shared_ptr<ossia::time_node> ossia_tn,
                const Scenario::TimeNodeModel& element,
                const Device::DeviceList& devlist,
                QObject* parent);

        std::shared_ptr<ossia::time_node> OSSIATimeNode() const;
        const Scenario::TimeNodeModel& iscoreTimeNode() const;

    private:
        std::shared_ptr<ossia::time_node> m_ossia_node;
        const Scenario::TimeNodeModel& m_iscore_node;

        const Device::DeviceList& m_deviceList;
};

}
