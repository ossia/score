#pragma once

#include <Process/ProcessFactory.hpp>
#include "SimpleProcessModel.hpp"
#include <Process/Dummy/DummyLayerPresenter.hpp>
#include <Process/Dummy/DummyLayerView.hpp>
#include <iscore/serialization/VisitorCommon.hpp>


class SimpleProcessFactory : public Process::ProcessFactory
{
        ISCORE_CONCRETE_FACTORY("0107dfb7-dcab-45c3-b7b8-e824c0fe49a1")
    public:
        QString prettyName() const override
        { return QObject::tr("SimpleProcess"); }

        Process::ProcessModel* make(
                const TimeValue& duration,
                const Id<Process::ProcessModel>& id,
                QObject* parent) override
        {
            return new SimpleProcessModel{duration, id, parent};
        }

        Process::ProcessModel* load(const VisitorVariant& vis, QObject* parent) override
        {
            return deserialize_dyn(vis, [&] (auto&& deserializer)
            { return new SimpleProcessModel{deserializer, parent};});
        }

        Process::LayerPresenter* makeLayerPresenter(
                const Process::LayerModel& model,
                Process::LayerView* v,
                const Process::ProcessPresenterContext& context,
                QObject* parent) override
        {
            return new Dummy::DummyLayerPresenter{
                model,
                safe_cast<Dummy::DummyLayerView*>(v),
                context,
                parent};
        }

        Process::LayerView* makeLayerView(
                const Process::LayerModel&,
                QGraphicsItem* parent) override
        {
            return new Dummy::DummyLayerView{parent};
        }
};
