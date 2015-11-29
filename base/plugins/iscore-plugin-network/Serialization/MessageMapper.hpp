#pragma once
#include <QList>
#include <QMap>
#include <QString>
#include <functional>

struct NetworkMessage;

class MessageMapper
{
    public:
        void addHandler(QString addr, std::function<void(NetworkMessage)> fun);
        void map(NetworkMessage m);
        QList<QString> addresses() const;

    private:
        QMap<QString, std::function<void(NetworkMessage)>> m_handlers;
};
