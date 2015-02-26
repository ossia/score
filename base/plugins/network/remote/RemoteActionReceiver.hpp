#pragma once
#include <string>
#include <QObject>
#include <Repartition/session/Session.h>

namespace iscore
{
    class SerializableCommand;
}

class RemoteActionReceiver : public QObject
{
        Q_OBJECT
    public:
        RemoteActionReceiver(Session*);

    signals:
        void undo();
        void redo();

        void commandReceived(QString, QString, QByteArray);

        // Note : we emit a QByteArray instead of a ObjectPath here because
        // due to the NetworkPlugin being dynamically loaded, there are a ton of hacks to apply
        // in order to have ObjectPath registered with Q_DECLARE_METATYPE in both shared objects.
        // So, it is the responsibility of the Presenter to deserialize the QByteArray into a ObjectPath.
        void lock(QByteArray);
        void unlock(QByteArray);

    protected:
        virtual void handle__edit_command(osc::ReceivedMessageArgumentStream);
        virtual void handle__edit_undo(osc::ReceivedMessageArgumentStream);
        virtual void handle__edit_redo(osc::ReceivedMessageArgumentStream);
        virtual void handle__edit_lock(osc::ReceivedMessageArgumentStream);
        virtual void handle__edit_unlock(osc::ReceivedMessageArgumentStream);

        virtual Session* session() = 0;
};
