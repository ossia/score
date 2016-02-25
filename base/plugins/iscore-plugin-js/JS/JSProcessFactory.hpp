#pragma once

#include <Process/ProcessFactory.hpp>
#include <JS/JSProcessModel.hpp>
#include <JS/StateProcess.hpp>
#include <DummyProcess/DummyLayerPresenter.hpp>
#include <DummyProcess/DummyLayerView.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <JS/JSProcessMetadata.hpp>
#include <Process/StateProcessFactory.hpp>
namespace JS
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
            return new JS::ProcessModel{duration, id, parent};
        }

        QByteArray makeStaticLayerConstructionData() const override
        {
            return {};
        }

        Process::ProcessModel* load(const VisitorVariant& vis, QObject* parent) override
        {
            return deserialize_dyn(vis, [&] (auto&& deserializer)
            { return new JS::ProcessModel{deserializer, parent};});
        }

        Process::LayerPresenter* makeLayerPresenter(
                const Process::LayerModel& model,
                Process::LayerView* v,
                QObject* parent) override
        {
            return new Dummy::DummyLayerPresenter{model, dynamic_cast<Dummy::DummyLayerView*>(v), parent};
        }

        Process::LayerView* makeLayerView(
                const Process::LayerModel&,
                QGraphicsItem* parent) override
        {
            return new Dummy::DummyLayerView{parent};
        }
};


class StateProcessFactory : public Process::StateProcessFactory
{
    public:
        QString prettyName() const override
        { return Metadata<PrettyName_k, StateProcess>::get(); }

        const UuidKey<Process::StateProcessFactory>& concreteFactoryKey() const override
        { return Metadata<ConcreteFactoryKey_k, StateProcess>::get(); }

        StateProcess* make(const Id<Process::StateProcess>& id, QObject* parent) override
        { return new StateProcess{id, parent}; }

        Process::StateProcess* load(
                const VisitorVariant& vis,
                QObject* parent) override
        {
            return deserialize_dyn(vis, [&] (auto&& deserializer)
            { return new StateProcess{deserializer, parent};});
        }
};

}
