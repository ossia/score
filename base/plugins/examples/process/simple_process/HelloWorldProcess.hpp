#pragma once
#include <interface/processes/Process.hpp>

class HelloWorldProcess : public iscore::Process
{
public:
	HelloWorldProcess();
	~HelloWorldProcess() = default;

	virtual QStringList availableViews() override;
	virtual iscore::ProcessView* makeView(QString view) override;
	// Mission : transmettre au présenteur global pour validation de l'action.
	// Ou bien c'est directement la vue qui s'en charge?
	// Risque de duplication dans le cas SmallView / StandardView / FullView...
	virtual iscore::ProcessPresenter* makePresenter() override;
	virtual iscore::ProcessModel* makeModel(unsigned int id, QObject* parent)  override; // Accédé par les commandes uniquement.
};
