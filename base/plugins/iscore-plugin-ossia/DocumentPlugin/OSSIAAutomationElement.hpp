#pragma once
#include "OSSIAProcessElement.hpp"
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

        iscore::ElementPluginModel *clone(const QObject *element, QObject *parent) const;
        static constexpr iscore::ElementPluginModelType staticPluginId() { return 6; }
        iscore::ElementPluginModelType elementPluginId() const;
        void serialize(const VisitorVariant &) const;

        std::shared_ptr<OSSIA::TimeProcess> process() const;

    private:
        std::shared_ptr<OSSIA::Automation<double>> m_ossia_autom;
        const AutomationModel* m_iscore_autom{};
};
