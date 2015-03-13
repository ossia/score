#pragma once
#include <QString>
#include <functional>
#include "NetworkMessage.hpp"
#include <QMap>

class MessageMapper
{
    public:
        void addHandler(QString addr, std::function<void(NetworkMessage)> fun);
        void map(NetworkMessage m);
        QList<QString> addresses() const;

    private:
        QMap<QString, std::function<void(NetworkMessage)>> m_handlers;
};
