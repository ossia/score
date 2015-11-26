#pragma once

#include <Process/ProcessFactory.hpp>
#include <Audio/AudioProcessModel.hpp>
#include <DummyProcess/DummyLayerPresenter.hpp>
#include <DummyProcess/DummyLayerView.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <Audio/AudioProcessMetadata.hpp>

namespace Audio
{
class ProcessFactory final : public ::ProcessFactory
{
    public:
        QString prettyName() const override;

        const ProcessFactoryKey& key_impl() const override;


        ::Process* makeModel(
                const TimeValue& duration,
                const Id<::Process>& id,
                QObject* parent) override;

        QByteArray makeStaticLayerConstructionData() const override;

        ::Process* loadModel(const VisitorVariant& vis, QObject* parent) override;

        LayerPresenter* makeLayerPresenter(
                const LayerModel& model,
                LayerView* v,
                QObject* parent) override;

        LayerView* makeLayerView(
                const LayerModel&,
                QGraphicsItem* parent) override;
};
}
