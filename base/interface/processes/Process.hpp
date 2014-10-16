#pragma once
#include <memory>
#include <QStringList>
#include "ProcessModel.hpp"


namespace iscore
{
	class ProcessView { };

	class ProcessSmallView { };
	class ProcessStandardView { };
	class ProcessFullView { };

	// Pour signaux / slots auto-connectants :
	// Comme on ne peut pas mettre d'héritage depuis QObject,
	// il faut encapsuler le QObject qui contiendra les signaux & slots, et avoir une
	// manière de préciser à quel objet / classe on veut qu'ils soient connectés automatiquement.
	// On peut peut-être utiliser le méchanisme d'auto-connection de Qt ?
	// mais ça ne semble marcher qu'à la compilation... http://qtway.blogspot.fr/2010/08/automatic-connections-using-qt-signals.html

	// Pour faire de l'introspection sur les signaux/slots dispos au runtime,
	// on peut utiliser QMetaObject qui permet de faire des checks. (On peut direct essayer de connecter et voir les erreurs...)

	// Premier example simple à faire : panel qui se met à jour à la
	// sélection d'un process.
	class ProcessPresenter
	{
		// Gérer ici actions sur GUI, drag&drop, et modifications du modèle.
	};


	class Process
	{
	public:
		virtual ~Process() = default;
		// Behind the scene, an API object.
		// Also contains all the drag&drop stuff? Or is more specifically in TimeProcess?

		virtual QStringList availableViews() = 0;
		virtual std::unique_ptr<ProcessView> makeView(QString view) = 0;
		// Mission : transmettre au présenteur global pour validation de l'action.
		// Ou bien c'est directement la vue qui s'en charge?
		// Risque de duplication dans le cas SmallView / StandardView / FullView...
		virtual std::unique_ptr<ProcessPresenter> makePresenter() = 0;
		virtual std::unique_ptr<ProcessModel> makeModel() = 0; // Accédé par les commandes uniquement.
	};
}
