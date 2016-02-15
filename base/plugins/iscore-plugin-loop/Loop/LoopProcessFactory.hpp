#pragma once

#include <Process/ProcessFactory.hpp>

#include <Loop/LoopProcessModel.hpp>
#include <Loop/LoopLayer.hpp>
#include <Loop/LoopPresenter.hpp>
#include <Loop/LoopView.hpp>
#include <Loop/LoopProcessMetadata.hpp>

#include <iscore/serialization/VisitorCommon.hpp>

namespace Loop
{
class ProcessFactory final : public Process::ProcessFactory
{
    public:
        QString prettyName() const override
        { // In factory list
            return Metadata<PrettyName_k, ProcessModel>::get();
        }

        const UuidKey<Process::ProcessFactory>& concreteFactoryKey() const override
        {
            return Metadata<ConcreteFactoryKey_k, ProcessModel>::get();
        }


        Process::ProcessModel* makeModel(
                const TimeValue& duration,
                const Id<Process::ProcessModel>& id,
                QObject* parent) override
        {
            return new Loop::ProcessModel{duration, id, parent};
        }

        QByteArray makeStaticLayerConstructionData() const override
        {
            return {};
        }

        Process::ProcessModel* load(const VisitorVariant& vis, QObject* parent) override
        {
            return deserialize_dyn(vis, [&] (auto&& deserializer)
            { return new Loop::ProcessModel{deserializer, parent};});
        }

        Process::LayerPresenter* makeLayerPresenter(
                const Process::LayerModel& model,
                Process::LayerView* v,
                QObject* parent) override
        {
            return new LayerPresenter{
                iscore::IDocument::documentContext(model),
                        dynamic_cast<const Layer&>(model),
                        dynamic_cast<LayerView*>(v),
                        parent};
        }

        Process::LayerView* makeLayerView(
                const Process::LayerModel&,
                QGraphicsItem* parent) override
        {
            return new Loop::LayerView{parent};
        }
};
}
