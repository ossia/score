#pragma once

#include <QDebug>
namespace iscore
{
    class SerializableCommand;
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
        RemoteActionEmitter (Session* session);
        void sendCommand (iscore::SerializableCommand*);

    public slots:
        void undo();
        void redo();

        void on_lock (QByteArray);
        void on_unlock (QByteArray);

    private:
        Session* m_session;
};
