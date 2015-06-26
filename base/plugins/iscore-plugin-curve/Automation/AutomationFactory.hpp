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

        virtual Layer* makeView(
                const LayerModel& viewmodel,
                QObject* parent) override;

        virtual ProcessPresenter* makePresenter(
                const LayerModel&,
                Layer*,
                QObject* parent) override;
};
