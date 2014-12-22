#pragma once
#include <ProcessInterface/ProcessViewModelInterface.hpp>

class PluginCurveModel;
class PluginCurveViewModel : public ProcessViewModelInterface
{
	public:
		PluginCurveViewModel(int id,
							 PluginCurveModel* model,
							 QObject* parent);

		// ProcessViewModelInterface interface
	public:
		virtual void serialize(QDataStream&) const
		{
			qDebug() << "TODO (will crash): " << Q_FUNC_INFO;
		}
		virtual void deserialize(QDataStream&)
		{
			qDebug() << "TODO (will crash): " << Q_FUNC_INFO;
		}
};
