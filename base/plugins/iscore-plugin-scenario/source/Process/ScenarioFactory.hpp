#pragma once
#include "ProcessInterface/ProcessFactory.hpp"

class ScenarioFactory : public ProcessFactory
{
    public:
        QString name() const override;

        ProcessModel* makeModel(
                const TimeValue& duration,
                const id_type<ProcessModel>& id,
                QObject* parent) override;

        ProcessModel* loadModel(
                const VisitorVariant&,
                QObject* parent) override;

        QByteArray makeStaticLayerConstructionData() const override;

        ProcessPresenter* makePresenter(
                const LayerModel&,
                Layer*,
                QObject* parent) override;

        Layer* makeView(
                const LayerModel& viewmodel,
                QObject* parent) override;

};
