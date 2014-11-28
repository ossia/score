#include "BaseElementPresenter.hpp"
#include <Document/Interval/IntervalPresenter.hpp>
#include <Document/BaseElement/BaseElementModel.hpp>

using namespace iscore;

BaseElementPresenter::BaseElementPresenter(DocumentPresenter* parent_presenter,
										   DocumentDelegateModelInterface* model,
										   DocumentDelegateViewInterface* view):
	DocumentDelegatePresenterInterface{parent_presenter, model, view},
	m_baseIntervalPresenter{new IntervalPresenter{this->model()->intervalModel(), this}}
{
}

BaseElementModel* BaseElementPresenter::model()
{
	return static_cast<BaseElementModel*>(m_model);
}
