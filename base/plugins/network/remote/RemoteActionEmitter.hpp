#pragma once

#include <QDebug>
namespace iscore
{
	class Command;
}
class Session;
// Pour l'instant, envoyer les actions à tous ?
// Plus tard, faire du fine-grain

// Comme clients individuels ne connaissent pas tout le monde,
// envoyer au master qui se charge de répercuter ?
class RemoteActionEmitter : public QObject
{
		Q_OBJECT
	public:
		using QObject::QObject;
		RemoteActionEmitter(Session* session);
		virtual void sendCommand(iscore::Command*);

	public slots:
		virtual void undo();
		virtual void redo();

	private:
		Session* m_session;
};
