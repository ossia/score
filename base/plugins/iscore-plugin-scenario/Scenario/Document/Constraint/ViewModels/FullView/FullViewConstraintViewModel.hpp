#pragma once
#include <Process/ZoomHelper.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <QPoint>
#include <QString>
#include <QRectF>

#include <iscore/tools/SettableIdentifier.hpp>


namespace Scenario
{
class ConstraintModel;
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
                ConstraintModel& model,
                QObject* parent);

        virtual FullViewConstraintViewModel* clone(
                const Id<ConstraintViewModel>& id,
                ConstraintModel& cm,
                QObject* parent) override;

        template<typename DeserializerVisitor>
        FullViewConstraintViewModel(DeserializerVisitor&& vis,
                                    ConstraintModel& model,
                                    QObject* parent) :
            ConstraintViewModel {vis, model, parent}
        {
            vis.writeTo(*this);
        }

        QString type() const override;

        ZoomRatio zoom() const;
        void setZoom(const ZoomRatio& zoom);

        QRectF visibleRect() const;
        void setVisibleRect(const QRectF& value);

        bool isActive();

    private:
        ZoomRatio m_zoom{-1};
        QRectF m_center{};
};
}
