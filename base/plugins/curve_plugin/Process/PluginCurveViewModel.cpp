#include "PluginCurveViewModel.hpp"
#include "PluginCurveModel.hpp"

PluginCurveViewModel::PluginCurveViewModel(int id,
										   PluginCurveModel* model,
										   QObject* parent):
	ProcessViewModelInterface{parent, "PluginCurveViewModel", id, model}
{
}
