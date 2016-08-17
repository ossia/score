#pragma once
#include <Process/ProcessFactory.hpp>
#include <Process/WidgetLayer/WidgetLayerModel.hpp>
#include <Process/WidgetLayer/WidgetLayerPresenter.hpp>
#include <Process/WidgetLayer/WidgetLayerView.hpp>
#include <Process/WidgetLayer/WidgetLayerPanelProxy.hpp>

namespace WidgetLayer
{
template<
        typename Model_T,
        typename Widget_T>
class ProcessFactory final :
        public Process::ProcessFactory
{
    public:
        virtual ~ProcessFactory() = default;

    private:
        UuidKey<Process::ProcessFactory> concreteFactoryKey() const override
        { return Metadata<ConcreteFactoryKey_k, Model_T>::get(); }

        QString prettyName() const override
        { return Metadata<PrettyName_k, Model_T>::get(); }

        Model_T* make(
                const TimeValue& duration,
                const Id<Process::ProcessModel>& id,
                QObject* parent) final override;

        Model_T* load(
                const VisitorVariant& vis,
                QObject* parent) final override;

        Layer* makeLayer_impl(
                Process::ProcessModel& proc,
                const Id<Process::LayerModel>& viewModelId,
                const QByteArray& constructionData,
                QObject* parent) final override;

        Layer* cloneLayer_impl(
                Process::ProcessModel& proc,
                const Id<Process::LayerModel>& newId,
                const Process::LayerModel& source,
                QObject* parent) final override;

        Layer* loadLayer_impl(
                Process::ProcessModel& proc,
                const VisitorVariant& vis,
                QObject* parent) final override;

        View* makeLayerView(
                const Process::LayerModel& viewmodel,
                QGraphicsItem* parent) final override
        {
            return new View{parent};
        }

        Presenter<Model_T, Widget_T>* makeLayerPresenter(
                const Process::LayerModel& lm,
                Process::LayerView* v,
                const Process::ProcessPresenterContext& context,
                QObject* parent) final override
        {
            return new Presenter<Model_T, Widget_T> {
                safe_cast<const Layer&>(lm),
                safe_cast<View*>(v),
                context,
                parent};
        }

        Process::LayerModelPanelProxy* makePanel(
                const Process::LayerModel& viewmodel,
                QObject* parent) final override
        {
            return new LayerPanelProxy<Model_T, Widget_T>{viewmodel, parent};
        }
};


template<
        typename Model_T,
        typename Widget_T>
Model_T* ProcessFactory<Model_T, Widget_T>::make(
        const TimeValue& duration,
        const Id<Process::ProcessModel>& id,
        QObject* parent)
{
    return new Model_T{duration, id, parent};
}

template<
        typename Model_T,
        typename Widget_T>
Model_T* ProcessFactory<Model_T, Widget_T>::load(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    { return new Model_T{deserializer, parent}; });
}

template<
        typename Model_T,
        typename Widget_T>
Layer* ProcessFactory<Model_T, Widget_T>::makeLayer_impl(
        Process::ProcessModel& proc,
        const Id<Process::LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    return new Layer{
                 static_cast<Model_T&>(proc),
                viewModelId,
                parent};
}

template<
        typename Model_T,
        typename Widget_T>
Layer* ProcessFactory<Model_T, Widget_T>::cloneLayer_impl(
        Process::ProcessModel& proc,
        const Id<Process::LayerModel>& newId,
        const Process::LayerModel& source,
        QObject* parent)
{
    return new Layer {
              static_cast<const Layer&>(source),
              static_cast<Model_T&>(proc),
              newId,
              parent};
}


template<
        typename Model_T,
        typename Widget_T>
Layer* ProcessFactory<Model_T, Widget_T>::loadLayer_impl(
        Process::ProcessModel& proc,
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    {
        auto layer = new Layer{
                        deserializer,
                        static_cast<Model_T&>(proc),
                        parent};

        return layer;
    });
}
}
