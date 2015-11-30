#pragma once
#include <API/Headers/Editor/Value.h>
#include <memory>

#include "OSSIAProcessElement.hpp"

class Process;
class QObject;
namespace OSSIA {
class TimeProcess;
}  // namespace OSSIA

namespace OSSIA
{
    class Automation;
    class CurveAbstract;
}

class AutomationModel;
class DeviceList;
class OSSIAConstraintElement;


class OSSIAAutomationElement final : public OSSIAProcessElement
{
    public:
        OSSIAAutomationElement(
                OSSIAConstraintElement& parentConstraint,
                AutomationModel& element,
                QObject* parent);

        std::shared_ptr<OSSIA::TimeProcess> OSSIAProcess() const override;
        Process& iscoreProcess() const override;

        void recreate() override;
        void clear() override;

    private:
        OSSIA::Value::Type m_addressType{OSSIA::Value::Type(-1)};

        std::shared_ptr<OSSIA::CurveAbstract> on_curveChanged();

        template<typename T>
        std::shared_ptr<OSSIA::CurveAbstract> on_curveChanged_impl();

        std::shared_ptr<OSSIA::Automation> m_ossia_autom;
        std::shared_ptr<OSSIA::CurveAbstract> m_ossia_curve;

        AutomationModel& m_iscore_autom;

        const DeviceList& m_deviceList;
};
