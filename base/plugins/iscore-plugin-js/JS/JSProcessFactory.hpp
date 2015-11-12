#pragma once

#include <Process/ProcessFactory.hpp>
#include <JS/JSProcessModel.hpp>
#include <DummyProcess/DummyLayerPresenter.hpp>
#include <DummyProcess/DummyLayerView.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <JS/JSProcessMetadata.hpp>


class JSProcessFactory final : public ProcessFactory
{
    public:
        QString prettyName() const override
        { // In factory list
            return JSProcessMetadata::factoryPrettyName();
        }

        const ProcessFactoryKey& key_impl() const override
        {
            return JSProcessMetadata::factoryKey();
        }


        Process* makeModel(
                const TimeValue& duration,
                const Id<Process>& id,
                QObject* parent) override
        {
            return new JSProcessModel{duration, id, parent};
        }

        QByteArray makeStaticLayerConstructionData() const override
        {
            return {};
        }

        Process* loadModel(const VisitorVariant& vis, QObject* parent) override
        {
            return deserialize_dyn(vis, [&] (auto&& deserializer)
            { return new JSProcessModel{deserializer, parent};});
        }

        LayerPresenter* makeLayerPresenter(
                const LayerModel& model,
                LayerView* v,
                QObject* parent) override
        {
            return new DummyLayerPresenter{model, dynamic_cast<DummyLayerView*>(v), parent};
        }

        LayerView* makeLayerView(
                const LayerModel&,
                QGraphicsItem* parent) override
        {
            return new DummyLayerView{parent};
        }
};
