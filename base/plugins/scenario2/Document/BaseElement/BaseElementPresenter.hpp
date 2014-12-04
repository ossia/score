#pragma once
#include <QNamedObject>
#include <interface/documentdelegate/DocumentDelegatePresenterInterface.hpp>

class BaseElementModel;
class BaseElementView;
class IntervalPresenter;
/**
 * @brief The BaseElementPresenter class
 *
 * A bit special because we connect it to the presenter of the content model
 * inside the interval model of the base element model.
 */
class BaseElementPresenter : public iscore::DocumentDelegatePresenterInterface
{
	Q_OBJECT

	public:
		BaseElementPresenter(iscore::DocumentPresenter* parent_presenter,
							 iscore::DocumentDelegateModelInterface* model,
							 iscore::DocumentDelegateViewInterface* view);
		virtual ~BaseElementPresenter() = default;

	private:
		BaseElementModel* model();
		BaseElementView* view();
		IntervalPresenter* m_baseIntervalPresenter;

};

