#pragma once

#include <ProcessInterface/ProcessViewModel.hpp>

class AutomationModel;
class AutomationViewModel : public ProcessViewModel
{
    public:
        AutomationViewModel(AutomationModel& model,
                            const id_type<ProcessViewModel>& id,
                            QObject* parent);

        // Copy
        AutomationViewModel(const AutomationViewModel& source,
                            AutomationModel& model,
                            const id_type<ProcessViewModel>& id,
                            QObject* parent);

        // Load
        template<typename Impl>
        AutomationViewModel(Deserializer<Impl>& vis,
                            AutomationModel& model,
                            QObject* parent) :
            ProcessViewModel {vis, model, parent}
        {
            vis.writeTo(*this);
        }

        virtual ProcessViewModelPanelProxy* make_panelProxy(QObject* parent) const override;
        virtual void serialize(const VisitorVariant&) const override;

        const AutomationModel& model() const;
};
