#pragma once
#include <interface/documentdelegate/DocumentDelegateFactoryInterface.hpp>

/*
 * TODO @Nico
 * Ici il faut faire la création du MVP pour le BaseTimebox/BaseInterval/BaseRelation (à voir selon le nom à la mode...).
 * Il consiste en une timebox à laquelle on peut rajouter des processus, et qui possède un évènement à t=0 au début, et 
 * pas de date de fin d'évènement. C'est aussi là qu'est gérée la GraphicsScene (et tous les trucs "de base", plus généralement).
 * 
 * Je te propose de le faire dans un dossier ScenarioDocument (et de donner ce nom de base aux "sous-classes", du genre ScenarioDocumentView, etc.)
*/
class ScenarioCentralPanel :  public iscore::DocumentDelegateFactoryInterface
{
	public:
		virtual iscore::DocumentDelegateViewInterface* makeView() override;
		virtual iscore::DocumentDelegatePresenterInterface* makePresenter(iscore::DocumentPresenter* parent_presenter,
															  iscore::DocumentDelegateModelInterface* model,
															  iscore::DocumentDelegateViewInterface* view) override;
		virtual iscore::DocumentDelegateModelInterface* makeModel() override;
};
