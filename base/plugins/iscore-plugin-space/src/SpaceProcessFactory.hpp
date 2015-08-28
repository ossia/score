#pragma once
#include <ProcessInterface/ProcessFactory.hpp>

class SpaceProcessFactory : public ProcessFactory
{
    public:
        QString name() const;

        Process* makeModel(
                const TimeValue &duration,
                const Id<Process> &id,
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
                QGraphicsItem *parent);
};
