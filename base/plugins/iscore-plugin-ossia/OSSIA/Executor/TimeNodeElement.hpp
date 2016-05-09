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
class TimeNode;
class TimeValue;
}
namespace Scenario
{
class TimeNodeModel;
}

namespace RecreateOnPlay
{
class ISCORE_PLUGIN_OSSIA_EXPORT TimeNodeElement final : public QObject
{
        Q_OBJECT
    public:
        TimeNodeElement(
                std::shared_ptr<OSSIA::TimeNode> ossia_tn,
                const Scenario::TimeNodeModel& element,
                const Device::DeviceList& devlist,
                QObject* parent);

        std::shared_ptr<OSSIA::TimeNode> OSSIATimeNode() const;
        const Scenario::TimeNodeModel& iscoreTimeNode() const;

    signals:
        void triggered();

    private:
        std::shared_ptr<OSSIA::TimeNode> m_ossia_node;
        const Scenario::TimeNodeModel& m_iscore_node;

        const Device::DeviceList& m_deviceList;
};

}
