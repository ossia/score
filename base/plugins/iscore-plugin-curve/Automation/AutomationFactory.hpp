#pragma once
#include <ProcessInterface/ProcessFactory.hpp>

class AutomationFactory : public ProcessFactory
{
    public:
        virtual QString name() const override
        {
            return "Automation";
        }

        Process* makeModel(
                const TimeValue& duration,
                const id_type<Process>& id,
                QObject* parent) override;

        Process* loadModel(
                const VisitorVariant&,
                QObject* parent) override;

        QByteArray makeStaticLayerConstructionData() const override;

        LayerView* makeLayerView(
                const LayerModel& viewmodel,
                QGraphicsItem* parent) override;

        LayerPresenter* makeLayerPresenter(
                const LayerModel&,
                LayerView*,
                QObject* parent) override;
};
