#pragma once
#include "OSSIAProcessElement.hpp"
#include <State/Address.hpp>
#include <QPointer>
namespace OSSIA
{
    class Automation;
    template<class> class Curve;
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
        ~OSSIAAutomationElement();

        std::shared_ptr<OSSIA::TimeProcess> process() const override;
        const ProcessModel* iscoreProcess() const override;

    public slots:
        void on_addressChanged(const iscore::Address&);
        void on_curveChanged();

    private:
        QPointer<OSSIAConstraintElement> m_parent_constraint;
        std::shared_ptr<OSSIA::Automation> m_ossia_autom;
        std::shared_ptr<OSSIA::Curve<float>> m_ossia_curve;
        const AutomationModel* m_iscore_autom{};

        const DeviceList& m_deviceList;
};
