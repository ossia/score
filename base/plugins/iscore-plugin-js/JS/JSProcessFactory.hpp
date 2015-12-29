#pragma once

#include <Process/ProcessFactory.hpp>
#include <JS/JSProcessModel.hpp>
#include <DummyProcess/DummyLayerPresenter.hpp>
#include <DummyProcess/DummyLayerView.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <JS/JSProcessMetadata.hpp>


class JSProcessFactory final : public Process::ProcessFactory
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


        Process::ProcessModel* makeModel(
                const TimeValue& duration,
                const Id<Process::ProcessModel>& id,
                QObject* parent) override
        {
            return new JS::ProcessModel{duration, id, parent};
        }

        QByteArray makeStaticLayerConstructionData() const override
        {
            return {};
        }

        Process::ProcessModel* loadModel(const VisitorVariant& vis, QObject* parent) override
        {
            return deserialize_dyn(vis, [&] (auto&& deserializer)
            { return new JS::ProcessModel{deserializer, parent};});
        }

        Process::LayerPresenter* makeLayerPresenter(
                const Process::LayerModel& model,
                Process::LayerView* v,
                QObject* parent) override
        {
            return new DummyLayerPresenter{model, dynamic_cast<DummyLayerView*>(v), parent};
        }

        Process::LayerView* makeLayerView(
                const Process::LayerModel&,
                QGraphicsItem* parent) override
        {
            return new DummyLayerView{parent};
        }
};
