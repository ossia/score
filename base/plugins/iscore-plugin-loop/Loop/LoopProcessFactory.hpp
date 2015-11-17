#pragma once

#include <Process/ProcessFactory.hpp>

#include <Loop/LoopProcessModel.hpp>
#include <Loop/LoopLayer.hpp>
#include <Loop/LoopPresenter.hpp>
#include <Loop/LoopView.hpp>
#include <Loop/LoopProcessMetadata.hpp>

#include <iscore/serialization/VisitorCommon.hpp>


class LoopProcessFactory final : public ProcessFactory
{
    public:
        QString prettyName() const override
        { // In factory list
            return LoopProcessMetadata::factoryPrettyName();
        }

        const ProcessFactoryKey& key_impl() const override
        {
            return LoopProcessMetadata::factoryKey();
        }


        Process* makeModel(
                const TimeValue& duration,
                const Id<Process>& id,
                QObject* parent) override
        {
            return new LoopProcessModel{duration, id, parent};
        }

        QByteArray makeStaticLayerConstructionData() const override
        {
            return {};
        }

        Process* loadModel(const VisitorVariant& vis, QObject* parent) override
        {
            return deserialize_dyn(vis, [&] (auto&& deserializer)
            { return new LoopProcessModel{deserializer, parent};});
        }

        LayerPresenter* makeLayerPresenter(
                const LayerModel& model,
                LayerView* v,
                QObject* parent) override
        {
            return new LoopPresenter{
                iscore::IDocument::documentContext(model),
                        dynamic_cast<const LoopLayer&>(model),
                        dynamic_cast<LoopView*>(v),
                        parent};
        }

        LayerView* makeLayerView(
                const LayerModel&,
                QGraphicsItem* parent) override
        {
            return new LoopView{parent};
        }
};
