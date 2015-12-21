#pragma once
#include <iscore/plugins/documentdelegate/DocumentDelegatePresenterInterface.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <QPoint>
#include <QRect>
#include <memory>

#include <Process/ZoomHelper.hpp>
#include <iscore/selection/Selection.hpp>

class DisplayedElementsPresenter;
class GraphicsSceneToolPalette;
class ObjectPath;
class QSize;
namespace iscore {
class DocumentDelegateModelInterface;
class DocumentDelegateViewInterface;
class DocumentPresenter;
}  // namespace iscore

namespace iscore
{
}

class ConstraintModel;
class ScenarioDocumentModel;
class ScenarioDocumentView;
class TimeRulerPresenter;

/**
 * @brief The ScenarioDocumentPresenter class
 *
 * A bit special because we connect it to the presenter of the content model
 * inside the constraint model of the base element model.
 */
class ScenarioDocumentPresenter final : public iscore::DocumentDelegatePresenterInterface
{
        Q_OBJECT
    friend class DisplayedElementsPresenter;
    public:
        ScenarioDocumentPresenter(iscore::DocumentPresenter* parent_presenter,
                             const iscore::DocumentDelegateModelInterface& model,
                             iscore::DocumentDelegateViewInterface& view);
        virtual ~ScenarioDocumentPresenter() = default;

        const ConstraintModel& displayedConstraint() const;
        const DisplayedElementsPresenter& presenters() const
        { return *m_scenarioPresenter; }

        const ScenarioDocumentModel& model() const;
        ScenarioDocumentView& view() const;

        // The height in pixels of the displayed constraint with its rack.
        //double height() const;
        ZoomRatio zoomRatio() const;


        void setDisplayedObject(const ObjectPath&);

        void on_askUpdate();

        void selectAll();
        void deselectAll();

        void setMillisPerPixel(ZoomRatio newFactor);

        void on_newSelection(const Selection &);

        void updateRect(const QRectF& rect);


    signals:
        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);
        void escPressed();

        void requestDisplayedConstraintChange(const ConstraintModel&);

    private:
        void on_displayedConstraintChanged();
        void on_zoomSliderChanged(double);
        void on_zoomOnWheelEvent(QPoint, QPointF);
        void on_viewSizeChanged(const QSize& s);
        void on_horizontalPositionChanged(int dx);

        //void updateGrid();
        void updateZoom(ZoomRatio newZoom, QPointF focus);

        DisplayedElementsPresenter* m_scenarioPresenter{};

        iscore::SelectionDispatcher m_selectionDispatcher;

        // State machine
        std::unique_ptr<GraphicsSceneToolPalette> m_stateMachine;

        // Various widgets
        TimeRulerPresenter* m_mainTimeRuler{};

        ZoomRatio m_zoomRatio;

};
