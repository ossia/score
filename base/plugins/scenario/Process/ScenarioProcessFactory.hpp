#pragma once
#include <interface/process/ProcessFactoryInterface.hpp>

class ScenarioProcessFactory : public ProcessFactoryInterface
{
	public:
		virtual QString name() const override;
		virtual QStringList availableViews();
		virtual ProcessViewInterface* makeView(QString view, QObject* parent) override;
		virtual ProcessPresenterInterface* makePresenter(ProcessViewModelInterface*,
														 ProcessViewInterface*,
														 QObject* parent) override;

		virtual ProcessSharedModelInterface* makeModel(int id,
													   QObject* parent) override;
		virtual ProcessSharedModelInterface* makeModel(QDataStream& data,
													   QObject* parent) override;
};
