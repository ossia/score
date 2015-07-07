#pragma once
#include <ProcessInterface/ProcessFactory.hpp>

class AutomationFactory : public ProcessFactory
{
    public:
        virtual QString name() const override
        {
            return "Automation";
        }

        ProcessModel* makeModel(
                const TimeValue& duration,
                const id_type<ProcessModel>& id,
                QObject* parent) override;

        ProcessModel* loadModel(
                const VisitorVariant&,
                QObject* parent) override;

        QByteArray makeStaticLayerConstructionData() const override;

        Layer* makeView(
                const LayerModel& viewmodel,
                QObject* parent) override;

        ProcessPresenter* makePresenter(
                const LayerModel&,
                Layer*,
                QObject* parent) override;
};
