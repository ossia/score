#pragma once
#include <QObject>

namespace iscore
{
	// Offre des actions, qui peuvent s'afficher dans:
	// - menu
	// - toolbar...
	// Les actions, lorsque déclenchées, appellent un slot de la classe Commande correspondante qui génère une action et la soumet au présenteur qui la place dans le stack.
	// A l'ownsership des actions.
	class Command;
	class Presenter;
	class MenubarManager;
	class CustomCommand : public QObject
	{
			Q_OBJECT
			// Menus : trouver un moyen pour créer automatiquement si n'existe pas ?
			// Dire chemin : File/Export/SomeCustomExport ?
			// Pb. : traduction ? (ex. : soft traduit & plug pas traduit ?)
			// Fournir menus de base : Fichier Edition Affichage Objet Arrangement Devices Fenêtre Paramètres Aide
		public:
			virtual ~CustomCommand() = default;
			virtual void populateMenus(iscore::MenubarManager*) = 0;
			virtual void populateToolbars() = 0;
			virtual void setPresenter(Presenter*) = 0;

			virtual Command* instantiateUndoCommand(QString name, QByteArray data) {}

		signals:
			void submitCommand(Command*);
	};
}
