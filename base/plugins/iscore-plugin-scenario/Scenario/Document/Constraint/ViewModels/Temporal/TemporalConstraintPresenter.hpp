#pragma once
#include <Scenario/Document/Constraint/ViewModels/ConstraintPresenter.hpp>

#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp>
#include <iscore_plugin_scenario_export.h>
class QGraphicsObject;
class QObject;


namespace Scenario
{
class ISCORE_PLUGIN_SCENARIO_EXPORT TemporalConstraintPresenter final : public ConstraintPresenter
{
        Q_OBJECT

    public:
        using viewmodel_type = TemporalConstraintViewModel;
        using view_type = TemporalConstraintView;
        const auto& id() const { return ConstraintPresenter::id(); } // To please boost::const_mem_fun

        TemporalConstraintPresenter(const TemporalConstraintViewModel& viewModel,
                                    QGraphicsObject* parentobject,
                                    QObject* parent);
        virtual ~TemporalConstraintPresenter();

    signals:
        void constraintHoverEnter();
        void constraintHoverLeave();
};
}
