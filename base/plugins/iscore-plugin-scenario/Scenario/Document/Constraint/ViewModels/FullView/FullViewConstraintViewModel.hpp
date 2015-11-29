#pragma once
#include <Process/ZoomHelper.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <QPoint>
#include <QString>

class ConstraintModel;
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

/**
 * @brief The FullViewConstraintViewModel class
 *
 * The ViewModel of a Constraint shown in full view.
 * It should show a TimeBar.
 *
 * In addition if it's the base constraint, it should be extensible.
 */
class FullViewConstraintViewModel final : public ConstraintViewModel
{
        Q_OBJECT

    public:

        /**
         * @brief FullViewConstraintViewModel
         * @param id identifier
         * @param model Pointer to the corresponding model object
         * @param parent Parent object (most certainly ScenarioViewModel)
         */
        FullViewConstraintViewModel(
                const Id<ConstraintViewModel>& id,
                const ConstraintModel& model,
                QObject* parent);

        virtual FullViewConstraintViewModel* clone(
                const Id<ConstraintViewModel>& id,
                const ConstraintModel& cm,
                QObject* parent) override;

        template<typename DeserializerVisitor>
        FullViewConstraintViewModel(DeserializerVisitor&& vis,
                                    const ConstraintModel& model,
                                    QObject* parent) :
            ConstraintViewModel {vis, model, parent}
        {
            vis.writeTo(*this);
        }

        QString type() const override;

        ZoomRatio zoom() const;
        void setZoom(const ZoomRatio& zoom);

        QPointF center() const;
        void setCenter(const QPointF& value);

        bool isActive();

    private:
        ZoomRatio m_zoom{60};
        QPointF m_center{0,0};
};
