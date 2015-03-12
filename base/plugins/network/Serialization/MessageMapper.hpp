#pragma once
#include <QString>
#include <functional>
#include <QMap>

class MessageMapper
{
    public:
        void addHandler(QString addr, std::function<void(QByteArray)> fun);
        void map(QString addr, QByteArray data);
        QList<QString> addresses() const;

    private:
        QMap<QString, std::function<void(QByteArray)>> m_handlers;
};
