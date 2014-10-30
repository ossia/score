#pragma once
#include <QObject>

namespace iscore
{
	class Command;
	class Presenter;
	class MenubarManager;
	/**
	 * @brief The CustomCommand class
	 *
	 * The name is bad. This is not related to the Command class.
	 * 
	 * TODO: refactor. Too much responsibilities for such a little thing.
	 * 
	 */
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
