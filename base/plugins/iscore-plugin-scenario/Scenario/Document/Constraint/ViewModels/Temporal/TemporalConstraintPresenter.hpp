#pragma once
#include <Scenario/Document/Constraint/ViewModels/ConstraintPresenter.hpp>

#include "Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"
#include "Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"

class QGraphicsObject;
class QObject;


namespace iscore
{
}

class TemporalConstraintPresenter final : public ConstraintPresenter
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

