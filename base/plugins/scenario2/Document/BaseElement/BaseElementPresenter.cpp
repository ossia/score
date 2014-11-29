#include "BaseElementPresenter.hpp"
#include <Document/Interval/IntervalPresenter.hpp>
#include <Document/BaseElement/BaseElementModel.hpp>
#include <Commands/Interval/Process/AddProcessToIntervalCommand.hpp>

using namespace iscore;

BaseElementPresenter::BaseElementPresenter(DocumentPresenter* parent_presenter,
										   DocumentDelegateModelInterface* model,
										   DocumentDelegateViewInterface* view):
	DocumentDelegatePresenterInterface{parent_presenter, model, view},
	m_baseIntervalPresenter{}
{
	AddProcessToIntervalCommand cmd(
	{
		"BaseElementModel",
		{
			{"IntervalModel", -1}
		}
	}, "Scenario");
	cmd.redo();

	m_baseIntervalPresenter = new IntervalPresenter{this->model()->intervalModel(), this};
}

BaseElementModel* BaseElementPresenter::model()
{
	return static_cast<BaseElementModel*>(m_model);
}
