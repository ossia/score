#pragma once
#include <Process/TimeValue.hpp>
#include <Scenario/Document/BaseElement/DisplayedElements/DisplayedElementsPresenter.hpp>

#include <iscore/plugins/documentdelegate/DocumentDelegatePresenterInterface.hpp>
#include <iscore/tools/NamedObject.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <QSize>
#include <QRectF>
class ProgressBar;
namespace iscore
{
    class SelectionDispatcher;
}

class BaseElementModel;
class BaseElementView;
class ConstraintModel;
class TimeRulerPresenter;
class LocalTimeRulerPresenter;

class BaseScenarioStateMachine;

/**
 * @brief The BaseElementPresenter class
 *
 * A bit special because we connect it to the presenter of the content model
 * inside the constraint model of the base element model.
 */
class BaseElementPresenter final : public iscore::DocumentDelegatePresenterInterface
{
        Q_OBJECT
    friend class DisplayedElementsPresenter;
    public:
        BaseElementPresenter(iscore::DocumentPresenter* parent_presenter,
                             const iscore::DocumentDelegateModelInterface& model,
                             iscore::DocumentDelegateViewInterface& view);
        virtual ~BaseElementPresenter() = default;

        const ConstraintModel& displayedConstraint() const;
        const DisplayedElementsPresenter& presenters() const
        { return *m_scenarioPresenter; }

        const BaseElementModel& model() const;
        BaseElementView& view() const;

        // The height in pixels of the displayed constraint with its rack.
        //double height() const;
        ZoomRatio zoomRatio() const;

    signals:
        void displayedConstraintPressed(const QPointF&);
        void displayedConstraintMoved(const QPointF&);
        void displayedConstraintReleased(const QPointF&);

        void requestDisplayedConstraintChange(const ConstraintModel&);

    public slots:
        void setDisplayedObject(const ObjectPath&);

        void on_askUpdate();

        void selectAll();
        void deselectAll();

        void setMillisPerPixel(ZoomRatio newFactor);

        void on_newSelection(const Selection &);

        void updateRect(const QRectF& rect);

    private slots:
        void on_displayedConstraintChanged();
        void on_zoomSliderChanged(double);
        void on_zoomOnWheelEvent(QPoint, QPointF);
        void on_viewSizeChanged(const QSize& s);
        void on_horizontalPositionChanged(int dx);

        //void updateGrid();


    private:
        void updateZoom(ZoomRatio newZoom, QPointF focus);

        DisplayedElementsPresenter* m_scenarioPresenter{};

        iscore::SelectionDispatcher m_selectionDispatcher;

        // State machine
        BaseScenarioStateMachine* m_stateMachine{};

        // Various widgets
        TimeRulerPresenter* m_mainTimeRuler{};

        ZoomRatio m_zoomRatio;

};
