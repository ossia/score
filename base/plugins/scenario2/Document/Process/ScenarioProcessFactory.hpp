#pragma once
#include <interface/process/ProcessFactoryInterface.hpp>

class ScenarioProcessFactory : public iscore::ProcessFactoryInterface
{
	public:
		virtual QString name() const override;
		virtual QStringList availableViews();
		virtual iscore::ProcessViewInterface* makeView(QString view, QObject* parent) override;
		virtual iscore::ProcessPresenterInterface* makePresenter(iscore::ProcessViewModelInterface*,
																 iscore::ProcessViewInterface*,
																 QObject* parent) override;

		virtual iscore::ProcessSharedModelInterface* makeModel(int id,
															   QObject* parent) override;
		virtual iscore::ProcessSharedModelInterface* makeModel(QDataStream& data,
															   QObject* parent) override;
};
