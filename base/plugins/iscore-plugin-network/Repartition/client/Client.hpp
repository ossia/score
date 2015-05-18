#pragma once
#include <iscore/tools/IdentifiedObject.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

class Client : public IdentifiedObject<Client>
{
        Q_OBJECT
        Q_PROPERTY(QString name
                   READ name
                   WRITE setName
                   NOTIFY nameChanged)
    public:
        Client(id_type<Client> id, QObject* parent = nullptr):
            IdentifiedObject<Client>{id, "Client", parent}
        {

        }

        template<typename Deserializer>
        Client(Deserializer&& vis, QObject* parent) :
            IdentifiedObject<Client> {vis, parent}
        {
            vis.writeTo(*this);
        }

        QString name() const
        {
            return m_name;
        }

    public slots:
        void setName(QString arg)
        {
            if (m_name == arg)
                return;

            m_name = arg;
            emit nameChanged(arg);
        }

    signals:
        void nameChanged(QString arg);

    private:
        QString m_name;
};

