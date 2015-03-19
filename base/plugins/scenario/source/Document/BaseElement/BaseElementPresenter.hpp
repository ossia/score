#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegatePresenterInterface.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <ProcessInterface/TimeValue.hpp>
class ProgressBar;
namespace iscore
{
    class SelectionDispatcher;
}

class BaseElementModel;
class BaseElementView;
class FullViewConstraintPresenter;
class ConstraintModel;
class TimeRulerPresenter;

/**
 * @brief The BaseElementPresenter class
 *
 * A bit special because we connect it to the presenter of the content model
 * inside the constraint model of the base element model.
 */
class BaseElementPresenter : public iscore::DocumentDelegatePresenterInterface
{
        Q_OBJECT

    public:
        BaseElementPresenter(iscore::DocumentPresenter* parent_presenter,
                             iscore::DocumentDelegateModelInterface* model,
                             iscore::DocumentDelegateViewInterface* view);
        virtual ~BaseElementPresenter() = default;

        ConstraintModel* displayedConstraint() const;
        BaseElementModel* model() const;

        // The height in pixels of the displayed constraint with its box.
        double height() const;
    public slots:
        void setDisplayedConstraint(ConstraintModel*);
        void setDisplayedObject(ObjectPath);

        void on_askUpdate();

        void selectAll();
        void deselectAll();

        void on_displayedConstraintChanged();

        void setProgressBarTime(TimeValue t);

        void setMillisPerPixel(double newFactor);

        void on_newSelection(Selection);

    private slots:
        // Value : the number of milliseconds per pixels
        void on_zoomSliderChanged(double);
        void on_viewSizeChanged(QSize s);

        void updateRect(QRectF rect);

    private:
        BaseElementView* view() const;
        FullViewConstraintPresenter* m_displayedConstraintPresenter {};

        ConstraintModel* m_displayedConstraint {};
        iscore::SelectionDispatcher m_selectionDispatcher;

        ProgressBar* m_progressBar{};

        TimeRulerPresenter* m_mainTimeRuler{};
        TimeRulerPresenter* m_localTimeRuler {};

        // 30s displayed by default on average
        double m_millisecondsPerPixel{0.03};
};
