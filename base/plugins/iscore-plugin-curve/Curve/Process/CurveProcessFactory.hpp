#pragma once
#include <Process/ProcessFactory.hpp>
#include <iscore/serialization/VisitorCommon.hpp>

template<
        typename Model_T,
        typename LayerModel_T,
        typename LayerPresenter_T,
        typename LayerView_T,
        typename CurveColors_T>
class CurveProcessFactory_T : public ProcessFactory
{
    public:
        Model_T* makeModel(
                const TimeValue& duration,
                const Id<Process>& id,
                QObject* parent) override
        {
            return new Model_T{duration, id, parent};
        }

        Model_T* loadModel(
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
                const LayerModel& viewmodel,
                QGraphicsItem* parent) override
        {
            return new LayerView_T{parent};
        }

        LayerPresenter_T* makeLayerPresenter(
                const LayerModel& lm,
                LayerView* v,
                QObject* parent) override
        {
            return new LayerPresenter_T {
                m_colors.style(),
                safe_cast<const LayerModel_T&>(lm),
                safe_cast<LayerView_T*>(v),
                parent};
        }

    private:
        CurveColors_T m_colors;
};

#define DEFINE_CURVE_PROCESS_FACTORY(Name, DynName, Model, Layer, Presenter, View, Colors) \
class Name final : public CurveProcessFactory_T<Model, Layer, Presenter, View, Colors> \
{ \
    QString name() const override { return DynName; } \
};
