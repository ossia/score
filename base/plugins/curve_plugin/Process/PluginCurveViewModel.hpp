#pragma once
#include <ProcessInterface/ProcessViewModelInterface.hpp>

class PluginCurveModel;
class PluginCurveViewModel : public ProcessViewModelInterface
{
	public:
		// Pointer : VERY BAD for serialization / deserialization.
		PluginCurveViewModel(int id,
							 int processId,
							 PluginCurveModel* model,
							 QObject* parent);
		PluginCurveModel* model();

		// ProcessViewModelInterface interface
	public:
		virtual void serialize(QDataStream&) const
		{
		}
		virtual void deserialize(QDataStream&)
		{
		}

	private:
		PluginCurveModel* m_model{};
};
