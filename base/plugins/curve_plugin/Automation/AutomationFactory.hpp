#pragma once
#include <ProcessInterface/ProcessFactoryInterface.hpp>

class AutomationFactory : public ProcessFactoryInterface
{
    public:
        virtual QString name() const override
        {
            return "Automation";
        }

        virtual QStringList availableViews() override
        {
            return {"Curve"};
        }

        virtual ProcessSharedModelInterface* makeModel(id_type<ProcessSharedModelInterface> id,
                QObject* parent) override;

        virtual ProcessSharedModelInterface* makeModel(SerializationIdentifier identifier,
                void* data,
                QObject* parent) override;

        virtual ProcessViewInterface* makeView(QString view,
                                               QObject* parent) override;
        virtual ProcessPresenterInterface* makePresenter(ProcessViewModelInterface*,
                ProcessViewInterface*,
                QObject* parent) override;
};
