#pragma once
#include "Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"


class ConstraintModel;

/**
 * @brief The TemporalConstraintViewModel class
 *
 * The ViewModel of a Constraint shown inside a temporal view of a scenario
 */
class TemporalConstraintViewModel : public ConstraintViewModel
{
        Q_OBJECT

    public:

        /**
         * @brief TemporalConstraintViewModel
         * @param id identifier
         * @param model Pointer to the corresponding model object
         * @param parent Parent object (most certainly ScenarioViewModel)
         */
        TemporalConstraintViewModel(const id_type<ConstraintViewModel>& id,
                                    const ConstraintModel& model,
                                    QObject* parent);

        virtual TemporalConstraintViewModel* clone(
                const id_type<ConstraintViewModel>& id,
                const ConstraintModel& cm,
                QObject* parent) override;

        template<typename DeserializerVisitor>
        TemporalConstraintViewModel(DeserializerVisitor&& vis,
                                    const ConstraintModel& model,
                                    QObject* parent) :
            ConstraintViewModel {vis, model, parent}
        {
            // Nothing to add, no vis.visit(*this);
        }

    signals:
        void eventSelected(const QString&);
};
