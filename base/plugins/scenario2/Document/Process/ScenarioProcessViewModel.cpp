#include "ScenarioProcessViewModel.hpp"


ScenarioProcessViewModel::ScenarioProcessViewModel(int id, QObject* parent):
	iscore::ProcessViewModelInterface(parent, "ScenarioProcessViewModel", id)
{
	qDebug(Q_FUNC_INFO);
}
