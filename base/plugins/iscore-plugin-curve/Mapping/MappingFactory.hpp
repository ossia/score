#pragma once
#include <ProcessInterface/ProcessFactory.hpp>

class MappingFactory : public ProcessFactory
{
    public:
        virtual QString name() const override
        {
            return "Mapping";
        }

        Process* makeModel(
                const TimeValue& duration,
                const Id<Process>& id,
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
