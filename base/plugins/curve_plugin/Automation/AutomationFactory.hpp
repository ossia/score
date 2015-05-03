#pragma once
#include <ProcessInterface/ProcessFactory.hpp>

class AutomationFactory : public ProcessFactory
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

        virtual ProcessView* makeView(
                const ProcessViewModel& viewmodel,
                QObject* parent) override;

        virtual ProcessPresenter* makePresenter(
                const ProcessViewModel&,
                ProcessView*,
                QObject* parent) override;
};
