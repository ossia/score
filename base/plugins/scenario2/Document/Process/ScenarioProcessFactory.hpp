#pragma once
#include <interface/process/ProcessFactoryInterface.hpp>

class ScenarioProcessFactory : public iscore::ProcessFactoryInterface
{
	public:
		virtual QString name() const override;
		virtual QStringList availableViews();
		virtual iscore::ProcessViewInterface* makeView(QString view) override;
		virtual iscore::ProcessPresenterInterface* makePresenter(iscore::ProcessViewModelInterface*, QObject* parent) override;

		virtual iscore::ProcessSharedModelInterface* makeModel(unsigned int id,
															   QObject* parent) override;
		virtual iscore::ProcessSharedModelInterface* makeModel(QDataStream& data,
															   QObject* parent) override;
};
