#pragma once

#include <Process/ProcessFactory.hpp>
#include "SimpleProcessModel.hpp"
#include <DummyProcess/DummyLayerPresenter.hpp>
#include <DummyProcess/DummyLayerView.hpp>
#include <iscore/serialization/VisitorCommon.hpp>


class SimpleProcessFactory : public ProcessFactory
{
    public:
        QString name() const override
        { // In factory list
            return SimpleProcessModel::staticProcessName();
        }

        Process* makeModel(
                const TimeValue& duration,
                const Id<Process>& id,
                QObject* parent) override
        {
            return new SimpleProcessModel{duration, id, parent};
        }

        QByteArray makeStaticLayerConstructionData() const override
        {
            return {};
        }

        Process* loadModel(const VisitorVariant& vis, QObject* parent) override
        {
            return deserialize_dyn(vis, [&] (auto&& deserializer)
            { return new SimpleProcessModel{deserializer, parent};});
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
