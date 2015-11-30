#pragma once
#include <API/Headers/Editor/Value.h>
#include <OSSIA/DocumentPlugin/ProcessElement.hpp>
#include <memory>

class Process;
class QObject;
namespace OSSIA {
class TimeProcess;
}  // namespace OSSIA
namespace RecreateOnPlay {
class ConstraintElement;
}  // namespace RecreateOnPlay

namespace OSSIA
{
    class Automation;
    class CurveAbstract;
}

class AutomationModel;
class DeviceList;



namespace RecreateOnPlay
{

class AutomationElement final : public ProcessElement
{
    public:
        AutomationElement(
                ConstraintElement& parentConstraint,
                AutomationModel& element,
                QObject* parent);

        std::shared_ptr<OSSIA::TimeProcess> OSSIAProcess() const override;
        Process& iscoreProcess() const override;

    private:
        void recreate();
        OSSIA::Value::Type m_addressType{OSSIA::Value::Type(-1)};

        std::shared_ptr<OSSIA::CurveAbstract> on_curveChanged();

        template<typename T>
        std::shared_ptr<OSSIA::CurveAbstract> on_curveChanged_impl();

        std::shared_ptr<OSSIA::Automation> m_ossia_autom;
        std::shared_ptr<OSSIA::CurveAbstract> m_ossia_curve;

        AutomationModel& m_iscore_autom;

        const DeviceList& m_deviceList;
};
}
