#pragma once
#include <Scenario/Document/Constraint/ViewModels/ConstraintPresenter.hpp>

class TemporalConstraintViewModel;
class TemporalConstraintView;
class QGraphicsObject;


namespace iscore
{
    class SerializableCommand;
}
class LayerPresenter;

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

