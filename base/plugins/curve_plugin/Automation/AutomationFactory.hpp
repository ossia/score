#pragma once
#include <ProcessInterface/ProcessFactoryInterface.hpp>

class AutomationFactory : public ProcessFactoryInterface
{
    public:
        virtual QString name() const override
        {
            return "Automation";
        }

        virtual ProcessModel* makeModel(
                const TimeValue& duration,
                const id_type<ProcessModel>& id,
                QObject* parent) override;

        virtual ProcessModel* loadModel(
                const VisitorVariant&,
                QObject* parent) override;

        virtual ProcessViewInterface* makeView(
                const ProcessViewModel& viewmodel,
                QObject* parent) override;

        virtual ProcessPresenterInterface* makePresenter(
                const ProcessViewModel&,
                ProcessViewInterface*,
                QObject* parent) override;
};
