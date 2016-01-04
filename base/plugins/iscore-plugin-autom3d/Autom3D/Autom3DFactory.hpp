#pragma once

#include <Process/ProcessFactory.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <Autom3D/Autom3DModel.hpp>
#include <Autom3D/Autom3DProcessMetadata.hpp>
#include <Autom3D/Autom3DLayerModel.hpp>
#include <DummyLayerPresenter.hpp>
#include <DummyLayerView.hpp>
namespace Autom3D
{
class ProcessFactory: public Process::ProcessFactory
{
    public:
        QString prettyName() const override
        { // In factory list
            return ProcessMetadata::factoryPrettyName();
        }

        const ProcessFactoryKey& key_impl() const override
        {
            return ProcessMetadata::factoryKey();
        }


        Process::ProcessModel* makeModel(
                const TimeValue& duration,
                const Id<Process::ProcessModel>& id,
                QObject* parent) override
        {
            return new Autom3D::ProcessModel{duration, id, parent};
        }

        QByteArray makeStaticLayerConstructionData() const override
        {
            return {};
        }

        Process::ProcessModel* loadModel(const VisitorVariant& vis, QObject* parent) override
        {
            return deserialize_dyn(vis, [&] (auto&& deserializer)
            { return new Autom3D::ProcessModel{deserializer, parent};});
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

}


