#pragma once
#include <QObject>

namespace iscore
{
	class Command;
	class Presenter;
	class MenubarManager;

	/**
	 * @brief The PluginControlInterface class
	 *
	 * This class's goal is to :
	 * * Instantiate some elements that are deeply intertwined with Qt : menus, toolbars
	 * * Manage the Commands of the plug-in : it has to be able to instantiate any meaningful
	 *   Command, if received by the network.
	 *
	 * It is instatiated exactly once by the Presenter class in i-score.
	 */
	class PluginControlInterface : public QObject
	{
			Q_OBJECT
			// Menus : trouver un moyen pour créer automatiquement si n'existe pas ?
			// Dire chemin : File/Export/SomeCustomExport ?
			// Pb. : traduction ? (ex. : soft traduit & plug pas traduit ?)
			// Fournir menus de base : Fichier Edition Affichage Objet Arrangement Devices Fenêtre Paramètres Aide
		public:
			virtual ~PluginControlInterface() = default;
			virtual void populateMenus(iscore::MenubarManager*) = 0;
			virtual void populateToolbars() = 0;
			virtual void setPresenter(Presenter*) = 0;

			virtual Command* instantiateUndoCommand(QString name, QByteArray data) { return nullptr; }

		signals:
			void submitCommand(Command*);
	};
}
