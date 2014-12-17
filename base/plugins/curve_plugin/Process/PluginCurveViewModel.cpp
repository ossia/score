#include "PluginCurveViewModel.hpp"

PluginCurveViewModel::PluginCurveViewModel(int id,
										   int processId,
										   PluginCurveModel* model,
										   QObject* parent):
	ProcessViewModelInterface{parent, "PluginCurveViewModel", id, processId},
	m_model{model}
{
}

PluginCurveModel* PluginCurveViewModel::model()
{
	return m_model;
}
