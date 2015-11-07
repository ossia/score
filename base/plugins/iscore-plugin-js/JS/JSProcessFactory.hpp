#pragma once

#include <Process/ProcessFactory.hpp>
#include <JS/JSProcessModel.hpp>
#include <DummyProcess/DummyLayerPresenter.hpp>
#include <DummyProcess/DummyLayerView.hpp>
#include <iscore/serialization/VisitorCommon.hpp>


class JSProcessFactory : public ProcessFactory
{
    public:
        QString name() const override
        { // In factory list
            return JSProcessModel::staticProcessName();
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
                LayerView*,
                QObject* parent) override
        {
            return new DummyLayerPresenter{model, parent};
        }

        LayerView* makeLayerView(
                const LayerModel&,
                QGraphicsItem* parent) override
        {
            return new DummyLayerView{parent};
        }
};
