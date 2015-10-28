#pragma once
#include "OSSIAProcessElement.hpp"
#include <State/Address.hpp>
#include <QPointer>
#include <API/Headers/Editor/Value.h>
namespace OSSIA
{
    class Automation;
    class CurveAbstract;
}

class AutomationModel;
class DeviceList;
class OSSIAConstraintElement;


class OSSIAAutomationElement : public OSSIAProcessElement
{
    public:
        OSSIAAutomationElement(
                OSSIAConstraintElement& parentConstraint,
                AutomationModel& element,
                QObject* parent);

        std::shared_ptr<OSSIA::TimeProcess> OSSIAProcess() const override;
        Process& iscoreProcess() const override;

    public slots:
        void on_addressChanged(const iscore::Address&);
        std::shared_ptr<OSSIA::CurveAbstract> on_curveChanged();

    private:
        OSSIA::Value::Type m_addressType{OSSIA::Value::Type(-1)};

        template<typename T>
        std::shared_ptr<OSSIA::CurveAbstract> on_curveChanged_impl();

        std::shared_ptr<OSSIA::Automation> m_ossia_autom;
        std::shared_ptr<OSSIA::CurveAbstract> m_ossia_curve;

        AutomationModel& m_iscore_autom;

        const DeviceList& m_deviceList;
};
