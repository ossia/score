#pragma once
#include <tools/NamedObject.hpp>
#include <interface/documentdelegate/DocumentDelegatePresenterInterface.hpp>

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

	public slots:
		void setDisplayedConstraint(ConstraintModel* );
		void setDisplayedObject(ObjectPath);

		void on_askUpdate();

		void selectAll();
		void deselectAll();

		void deleteSelection();

		void on_displayedConstraintChanged();
		void on_horizontalZoomChanged(int);

	private:
		BaseElementModel* model();
		BaseElementView* view();
		FullViewConstraintPresenter* m_baseConstraintPresenter{};

		ConstraintModel* m_displayedConstraint{};

};

