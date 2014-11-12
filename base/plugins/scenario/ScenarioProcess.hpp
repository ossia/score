#pragma once
#include <interface/process/ProcessFactoryInterface.hpp>

/* TODO @Nico
 * 
 * Ici il faut donc faire le processus scénario, qui permet d'instancier events et relations.
 * Il faut que le présenteur du processus papote avec le présenteur du processus parent, 
 * jusqu'à celui du document, qui fait remonter  à ScenarioCommand (qui va changer de nom en ScenarioControl je pense).
 */
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
