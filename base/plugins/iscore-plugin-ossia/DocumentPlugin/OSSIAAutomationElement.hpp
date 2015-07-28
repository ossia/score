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
                OSSIAConstraintElement* parentConstraint,
                const AutomationModel* element,
                QObject* parent);

        std::shared_ptr<OSSIA::TimeProcess> process() const override;
        const Process* iscoreProcess() const override;

    public slots:
        void on_addressChanged(const iscore::Address&);
        void on_curveChanged();

    private:
        OSSIA::Value::Type m_addressType{OSSIA::Value::Type(-1)};

        template<typename T>
        void on_curveChanged_impl();

        QPointer<OSSIAConstraintElement> m_parent_constraint;
        std::shared_ptr<OSSIA::Automation> m_ossia_autom;
        std::shared_ptr<OSSIA::CurveAbstract> m_ossia_curve;
        const AutomationModel* m_iscore_autom{};

        const DeviceList& m_deviceList;
};
