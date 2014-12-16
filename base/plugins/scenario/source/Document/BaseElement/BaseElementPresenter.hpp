#pragma once
#include <tools/NamedObject.hpp>
#include <interface/documentdelegate/DocumentDelegatePresenterInterface.hpp>

class BaseElementModel;
class BaseElementView;
class ConstraintPresenter;
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

	public slots:
		void onReset();

	private:
		BaseElementModel* model();
		BaseElementView* view();
		ConstraintPresenter* m_baseConstraintPresenter;

};

