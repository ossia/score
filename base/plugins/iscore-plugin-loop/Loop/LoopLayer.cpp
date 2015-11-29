
#include "LoopLayer.hpp"
#include <Loop/LoopPanelProxy.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>


constexpr const char LoopLayer::className[];
LoopLayer::LoopLayer(
        Loop::ProcessModel& model,
        const Id<LayerModel>& id,
        QObject* parent) :
    LayerModel {id, LoopLayer::staticMetaObject.className(), model, parent}
{
    m_constraint = model.constraint().makeConstraintViewModel<TemporalConstraintViewModel>(
                Id<ConstraintViewModel>{0},
                this);
}

LoopLayer::LoopLayer(
        const LoopLayer& source,
        Loop::ProcessModel& model,
        const Id<LayerModel>& id,
        QObject* parent) :
    LayerModel {id, LoopLayer::staticMetaObject.className(), model, parent}
{
    m_constraint = source.m_constraint->clone(
                source.constraint().id(),
                model.constraint(),
                this);
}

LayerModelPanelProxy* LoopLayer::make_panelProxy(
        QObject* parent) const
{
    return new LoopPanelProxy{*this, parent};
}


const Loop::ProcessModel& LoopLayer::model() const
{
    return static_cast<const Loop::ProcessModel&>(processModel());
}
