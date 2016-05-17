#pragma once
#include <Process/ProcessFactory.hpp>
#include <Process/Process.hpp>
#include <Process/LayerModel.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <iscore_plugin_curve_export.h>

namespace Curve
{
class EditionSettings;
template<
        typename Model_T,
        typename LayerModel_T,
        typename LayerPresenter_T,
        typename LayerView_T,
        typename CurveColors_T>
class ISCORE_PLUGIN_CURVE_EXPORT CurveProcessFactory_T :
        public Process::ProcessFactory
{
    public:
        virtual ~CurveProcessFactory_T() = default;

        Model_T* makeModel(
                const TimeValue& duration,
                const Id<Process::ProcessModel>& id,
                QObject* parent) override
        {
            return new Model_T{duration, id, parent};
        }

        Model_T* load(
                const VisitorVariant& vis,
                QObject* parent) override
        {
            return deserialize_dyn(vis, [&] (auto&& deserializer)
            { return new Model_T{deserializer, parent}; });
        }

        QByteArray makeStaticLayerConstructionData() const override
        {
            return {};
        }

        LayerView_T* makeLayerView(
                const Process::LayerModel& viewmodel,
                QGraphicsItem* parent) override
        {
            return new LayerView_T{parent};
        }

        LayerPresenter_T* makeLayerPresenter(
                const Process::LayerModel& lm,
                Process::LayerView* v,
                const Process::ProcessPresenterContext& context,
                QObject* parent) override
        {
            return new LayerPresenter_T {
                m_colors.style(),
                safe_cast<const LayerModel_T&>(lm),
                safe_cast<LayerView_T*>(v),
                context,
                parent};
        }

    private:
        CurveColors_T m_colors;
};

}

namespace Process
{
class ProcessFactory;
}

// See AutomationProcessMetadata.
#define DEFINE_CURVE_PROCESS_FACTORY(Name, Model, Layer, Presenter, View, Colors) \
class Name final : public Curve::CurveProcessFactory_T<Model, Layer, Presenter, View, Colors> \
{ \
    using Curve::CurveProcessFactory_T<Model, Layer, Presenter, View, Colors>::CurveProcessFactory_T; \
    const UuidKey<Process::ProcessFactory>& concreteFactoryKey() const override \
    { return Metadata<ConcreteFactoryKey_k, ProcessModel>::get(); } \
    \
    QString prettyName() const override \
    { return Metadata<PrettyName_k, ProcessModel>::get(); } \
};
