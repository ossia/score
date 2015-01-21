#include "PluginCurveViewModel.hpp"
#include "PluginCurveModel.hpp"

PluginCurveViewModel::PluginCurveViewModel(id_type<ProcessViewModelInterface> id,
										   PluginCurveModel* model,
										   QObject* parent):
	ProcessViewModelInterface{id, "PluginCurveViewModel", model, parent}
{
}

void PluginCurveViewModel::serialize(SerializationIdentifier identifier, void* data) const
{
	qDebug() << "TODO (will crash): " << Q_FUNC_INFO;
}
