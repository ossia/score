#include "ScenarioProcessPresenter.hpp"
#include "ScenarioProcessViewModel.hpp"
#include <QDebug>

ScenarioProcessPresenter::ScenarioProcessPresenter(iscore::ProcessViewModelInterface* model, QObject* parent):
	iscore::ProcessPresenterInterface{parent, "ScenarioProcessPresenter"},
	m_model{static_cast<ScenarioProcessViewModel*>(model)}
{
	qDebug(Q_FUNC_INFO);
}
