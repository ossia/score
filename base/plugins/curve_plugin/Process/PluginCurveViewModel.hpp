#pragma once
#include <ProcessInterface/ProcessViewModelInterface.hpp>

class PluginCurveModel;
class PluginCurveViewModel : public ProcessViewModelInterface
{
	public:
		PluginCurveViewModel(int id,
							 PluginCurveModel* model,
							 QObject* parent);

	public:
		virtual void serialize(SerializationIdentifier identifier, void* data) const override;
};
