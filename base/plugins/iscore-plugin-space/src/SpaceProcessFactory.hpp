#pragma once
#include <ProcessInterface/ProcessFactory.hpp>

class SpaceProcessFactory : public ProcessFactory
{
    public:
        QString name() const;

        ProcessModel *makeModel(
                const TimeValue &duration,
                const id_type<ProcessModel> &id,
                QObject *parent);

        QByteArray makeStaticLayerConstructionData() const;

        ProcessModel *loadModel(
                const VisitorVariant &,
                QObject *parent);

        ProcessPresenter *makePresenter(
                const LayerModel &,
                Layer *,
                QObject *parent);

        Layer* makeView(
                const LayerModel &view,
                QObject *parent);
};
