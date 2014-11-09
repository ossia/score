#pragma once
#include <interface/process/ProcessFactoryInterface.hpp>

class ScenarioProcess : public iscore::ProcessFactoryInterface
{
public:
	ScenarioProcess();
	~ScenarioProcess() = default;

	virtual QString name() const override;

	virtual QStringList availableViews() override;
	virtual iscore::ProcessViewInterface* makeView(QString view) override;
	// Mission : transmettre au présenteur global pour validation de l'action.
	// Ou bien c'est directement la vue qui s'en charge?
	// Risque de duplication dans le cas SmallView / StandardView / FullView...
	virtual iscore::ProcessPresenterInterface* makePresenter() override;
	virtual iscore::ProcessModelInterface* makeModel(unsigned int id, QObject* parent)  override; // Accédé par les commandes uniquement.
};
