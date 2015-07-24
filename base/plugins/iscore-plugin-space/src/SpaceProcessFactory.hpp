#pragma once
#include <ProcessInterface/ProcessFactory.hpp>

class SpaceProcessFactory : public ProcessFactory
{
    public:
        QString name() const;

        Process* makeModel(
                const TimeValue &duration,
                const id_type<Process> &id,
                QObject *parent);

        Process* loadModel(
                const VisitorVariant &,
                QObject *parent);


        QByteArray makeStaticLayerConstructionData() const;

        LayerPresenter* makeLayerPresenter(
                const LayerModel &,
                LayerView *,
                QObject *parent);

        LayerView* makeLayerView(
                const LayerModel &view,
                QObject *parent);
};
