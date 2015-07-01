#pragma once
#include "OSSIAProcessElement.hpp"
#include <State/Address.hpp>

namespace OSSIA
{
    template<typename> class Automation;
}

class AutomationModel;


class OSSIAAutomationElement : public OSSIAProcessElement
{
    public:
        OSSIAAutomationElement(
                const AutomationModel* element,
                QObject* parent);

        std::shared_ptr<OSSIA::TimeProcess> process() const;

    public slots:
        void on_addressChanged(const iscore::Address&);

    private:
        std::shared_ptr<OSSIA::Automation<double>> m_ossia_autom;
        const AutomationModel* m_iscore_autom{};
};
