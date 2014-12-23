#include "PluginCurveViewModel.hpp"
#include "PluginCurveModel.hpp"

PluginCurveViewModel::PluginCurveViewModel(int id,
										   PluginCurveModel* model,
										   QObject* parent):
	ProcessViewModelInterface{id, "PluginCurveViewModel", model, parent}
{
}
