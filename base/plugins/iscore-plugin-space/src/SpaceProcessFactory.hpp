#pragma once
#include <Process/ProcessFactory.hpp>
namespace Space
{
class ProcessFactory : public Process::ProcessFactory
{
    public:
        const ProcessFactoryKey& key_impl() const override;
        QString prettyName() const override;

        Process::ProcessModel* makeModel(
                const TimeValue &duration,
                const Id<Process::ProcessModel> &id,
                QObject *parent) override;

        Process::ProcessModel* loadModel(
                const VisitorVariant &,
                QObject *parent) override;


        QByteArray makeStaticLayerConstructionData() const override;

        Process::LayerPresenter* makeLayerPresenter(
                const Process::LayerModel &,
                Process::LayerView *,
                QObject *parent) override;

        Process::LayerView* makeLayerView(
                const Process::LayerModel &view,
                QGraphicsItem *parent) override;
};
}
