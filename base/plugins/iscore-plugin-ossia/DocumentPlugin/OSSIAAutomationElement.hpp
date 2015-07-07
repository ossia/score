#pragma once
#include "OSSIAProcessElement.hpp"
#include <State/Address.hpp>

namespace OSSIA
{
    class Automation;
    template<class> class Curve;
}

class AutomationModel;
class DeviceList;


class OSSIAAutomationElement : public OSSIAProcessElement
{
    public:
        OSSIAAutomationElement(
                const AutomationModel* element,
                QObject* parent);

        std::shared_ptr<OSSIA::TimeProcess> process() const;

    public slots:
        void on_addressChanged(const iscore::Address&);
        void on_curveChanged();

    private:
        std::shared_ptr<OSSIA::Automation> m_ossia_autom;
        std::shared_ptr<OSSIA::Curve<float>> m_ossia_curve;
        const AutomationModel* m_iscore_autom{};

        const DeviceList& m_deviceList;
};
