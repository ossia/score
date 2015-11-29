#pragma once
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

class Client;
class ClientSession;
class NetworkSocket;
class Session;
struct NetworkMessage;

class ClientSessionBuilder : public QObject
{
        Q_OBJECT
    public:
        ClientSessionBuilder(QString ip, int port);

        void initiateConnection();
        ClientSession* builtSession() const;
        QByteArray documentData() const;
        const QList<QPair<QPair<CommandParentFactoryKey, CommandFactoryKey>, QByteArray> >& commandStackData() const;

    public slots:
        void on_messageReceived(const NetworkMessage& m);

    signals:
        void sessionReady(ClientSessionBuilder*, ClientSession*);

    private:
        QString m_clientName{"A Client"};
        Id<Client> m_masterId, m_clientId;
        Id<Session> m_sessionId;
        NetworkSocket* m_mastersocket{};


        QList<QPair <QPair <CommandParentFactoryKey, CommandFactoryKey>, QByteArray> > m_commandStack;
        QByteArray m_documentData;

        ClientSession* m_session{};
};
