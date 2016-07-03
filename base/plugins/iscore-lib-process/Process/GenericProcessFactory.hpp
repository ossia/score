#pragma once
#include <Process/ProcessFactory.hpp>
#include <iscore/serialization/VisitorCommon.hpp>

namespace Process
{
struct default_t { };

template<
        typename Model_T,
        typename LayerModel_T,
        typename LayerPresenter_T,
        typename LayerView_T>
class GenericProcessFactory final :
        public Process::ProcessFactory
{
    public:
        virtual ~GenericProcessFactory() = default;

    private:
        const UuidKey<Process::ProcessFactory>& concreteFactoryKey() const override
        { return Metadata<ConcreteFactoryKey_k, Model_T>::get(); }

        QString prettyName() const override
        { return Metadata<PrettyName_k, Model_T>::get(); }

        Model_T* makeModel(
                const TimeValue& duration,
                const Id<Process::ProcessModel>& id,
                QObject* parent) final override;

        Model_T* load(
                const VisitorVariant& vis,
                QObject* parent) final override;

        LayerModel_T* makeLayer_impl(
                Process::ProcessModel& proc,
                const Id<Process::LayerModel>& viewModelId,
                const QByteArray& constructionData,
                QObject* parent) final override;

        LayerModel_T* cloneLayer_impl(
                Process::ProcessModel& proc,
                const Id<Process::LayerModel>& newId,
                const Process::LayerModel& source,
                QObject* parent) final override;

        LayerModel_T* loadLayer_impl(
                Process::ProcessModel& proc,
                const VisitorVariant& vis,
                QObject* parent) final override;

        LayerView_T* makeLayerView(
                const Process::LayerModel& viewmodel,
                QGraphicsItem* parent) final override
        {
            return new LayerView_T{parent};
        }

        LayerPresenter_T* makeLayerPresenter(
                const Process::LayerModel& lm,
                Process::LayerView* v,
                const Process::ProcessPresenterContext& context,
                QObject* parent) final override
        {
            return new LayerPresenter_T {
                safe_cast<const LayerModel_T&>(lm),
                safe_cast<LayerView_T*>(v),
                context,
                parent};
        }
};


template<
        typename Model_T,
        typename LayerModel_T,
        typename LayerPresenter_T,
        typename LayerView_T>
Model_T* GenericProcessFactory<Model_T, LayerModel_T, LayerPresenter_T, LayerView_T>::makeModel(
        const TimeValue& duration,
        const Id<Process::ProcessModel>& id,
        QObject* parent)
{
    return new Model_T{duration, id, parent};
}

template<
        typename Model_T,
        typename LayerModel_T,
        typename LayerPresenter_T,
        typename LayerView_T>
Model_T* GenericProcessFactory<Model_T, LayerModel_T, LayerPresenter_T, LayerView_T>::load(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    { return new Model_T{deserializer, parent}; });
}

template<
        typename Model_T,
        typename LayerModel_T,
        typename LayerPresenter_T,
        typename LayerView_T>
LayerModel_T* GenericProcessFactory<Model_T, LayerModel_T, LayerPresenter_T, LayerView_T>::makeLayer_impl(
        Process::ProcessModel& proc,
        const Id<Process::LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    return new LayerModel_T{
                 static_cast<Model_T&>(proc),
                viewModelId,
                parent};
}

template<
        typename Model_T,
        typename LayerModel_T,
        typename LayerPresenter_T,
        typename LayerView_T>
LayerModel_T* GenericProcessFactory<Model_T, LayerModel_T, LayerPresenter_T, LayerView_T>::cloneLayer_impl(
        Process::ProcessModel& proc,
        const Id<Process::LayerModel>& newId,
        const Process::LayerModel& source,
        QObject* parent)
{
    return new LayerModel_T {
              static_cast<const LayerModel_T&>(source),
              static_cast<Model_T&>(proc),
              newId,
              parent};
}


template<
        typename Model_T,
        typename LayerModel_T,
        typename LayerPresenter_T,
        typename LayerView_T>
LayerModel_T* GenericProcessFactory<Model_T, LayerModel_T, LayerPresenter_T, LayerView_T>::loadLayer_impl(
        Process::ProcessModel& proc,
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    {
        auto layer = new LayerModel_T{
                        deserializer,
                        static_cast<Model_T&>(proc),
                        parent};

        return layer;
    });
}

template<typename Model_T>
class GenericProcessFactory<Model_T, default_t, default_t, default_t> final :
        public Process::ProcessFactory
{
    public:
        virtual ~GenericProcessFactory() = default;

    private:
        const UuidKey<Process::ProcessFactory>& concreteFactoryKey() const override
        { return Metadata<ConcreteFactoryKey_k, Model_T>::get(); }

        QString prettyName() const override
        { return Metadata<PrettyName_k, Model_T>::get(); }

        Model_T* makeModel(
                const TimeValue& duration,
                const Id<Process::ProcessModel>& id,
                QObject* parent) final override
        {
            return new Model_T{duration, id, parent};
        }

        Model_T* load(
                const VisitorVariant& vis,
                QObject* parent) final override
        {
            return deserialize_dyn(vis, [&] (auto&& deserializer)
            { return new Model_T{deserializer, parent}; });
        }
};

template<typename Model_T>
using GenericDefaultProcessFactory = GenericProcessFactory<Model_T, default_t, default_t, default_t>;
}
