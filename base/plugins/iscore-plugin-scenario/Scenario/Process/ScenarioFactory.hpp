#pragma once
#include <Process/ProcessFactory.hpp>

class ScenarioFactory final : public ProcessFactory
{
    public:
        const ProcessFactoryKey& key_impl() const override;
        QString prettyName() const override;

        Process* makeModel(
                const TimeValue& duration,
                const Id<Process>& id,
                QObject* parent) override;

        Process* loadModel(
                const VisitorVariant&,
                QObject* parent) override;

        QByteArray makeStaticLayerConstructionData() const override;

        LayerPresenter* makeLayerPresenter(
                const LayerModel&,
                LayerView*,
                QObject* parent) override;

        LayerView* makeLayerView(
                const LayerModel& viewmodel,
                QGraphicsItem* parent) override;

};
