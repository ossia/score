#pragma once
#include "Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"

class ConstraintModel;
/**
 * @brief The FullViewConstraintViewModel class
 *
 * The ViewModel of a Constraint shown in full view.
 * It should show a TimeBar.
 *
 * In addition if it's the base constraint, it should be extensible.
 */
class FullViewConstraintViewModel : public AbstractConstraintViewModel
{
        Q_OBJECT

    public:

        /**
         * @brief FullViewConstraintViewModel
         * @param id identifier
         * @param model Pointer to the corresponding model object
         * @param parent Parent object (most certainly ScenarioViewModel)
         */
        FullViewConstraintViewModel(id_type<AbstractConstraintViewModel> id,
                                    ConstraintModel* model,
                                    QObject* parent);

        virtual FullViewConstraintViewModel* clone(id_type<AbstractConstraintViewModel> id,
                ConstraintModel* cm,
                QObject* parent) override;

        template<typename DeserializerVisitor>
        FullViewConstraintViewModel(DeserializerVisitor&& vis,
                                    ConstraintModel* model,
                                    QObject* parent) :
            AbstractConstraintViewModel {vis, model, parent}
        {
            // Nothing to add, no vis.visit(*this);
        }
};
