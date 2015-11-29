#pragma once
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <QString>

class ConstraintModel;
class QObject;
template <typename tag, typename impl> class id_base_t;

/**
 * @brief The TemporalConstraintViewModel class
 *
 * The ViewModel of a Constraint shown inside a temporal view of a scenario
 */
class TemporalConstraintViewModel final : public ConstraintViewModel
{
        Q_OBJECT

    public:

        /**
         * @brief TemporalConstraintViewModel
         * @param id identifier
         * @param model Pointer to the corresponding model object
         * @param parent Parent object (most certainly ScenarioViewModel)
         */
        TemporalConstraintViewModel(const Id<ConstraintViewModel>& id,
                                    const ConstraintModel& model,
                                    QObject* parent);

        virtual TemporalConstraintViewModel* clone(
                const Id<ConstraintViewModel>& id,
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

        QString type() const override;

    signals:
        void eventSelected(const QString&);
};
