#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegatePresenterInterface.hpp>
namespace iscore
{
    class SelectionDispatcher;
}

class BaseElementModel;
class BaseElementView;
class FullViewConstraintPresenter;
class ConstraintModel;
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

    public slots:
        void setDisplayedConstraint(ConstraintModel*);
        void setDisplayedObject(ObjectPath);

        void on_askUpdate();

        void selectAll();
        void deselectAll();

        void on_displayedConstraintChanged();

    private slots:
        void on_horizontalZoomChanged(int);
        void on_positionSliderChanged(int);

        void on_viewWidthChanged(int);

    private:
        BaseElementView* view() const;
        FullViewConstraintPresenter* m_baseConstraintPresenter {};

        ConstraintModel* m_displayedConstraint {};
        iscore::SelectionDispatcher* m_selectionDispatcher{};


        int m_horizontalZoomValue {50};

};
