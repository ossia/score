#pragma once

#include <QDebug>
namespace iscore
{
	class Command;
	// Pour l'instant, envoyer les actions à tous ?
	// Plus tard, faire du fine-grain

	// Comme clients individuels ne connaissent pas tout le monde,
	// envoyer au master qui se charge de répercuter ?
	class RemoteActionEmitter
	{
		public:
			virtual void sendCommand(Command*) = 0;
	};
}
