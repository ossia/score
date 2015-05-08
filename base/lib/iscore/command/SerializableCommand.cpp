#include <iscore/command/SerializableCommand.hpp>
using namespace iscore;

SerializableCommand::~SerializableCommand()
{
    // This is here in order to have the vtable emitted only in this file
    // and reduce binary size.
}

const QString& SerializableCommand::name() const
{
    return m_name;
}

const QString& SerializableCommand::parentName() const
{
    return m_parentName;
}

const QString& SerializableCommand::text() const
{
    return m_text;
}

void SerializableCommand::setText(const QString &t)
{
    m_text = t;
}

QByteArray SerializableCommand::serialize() const
{
    QByteArray arr;
    {
        QDataStream s(&arr, QIODevice::Append);
        s.setVersion(QDataStream::Qt_5_3);

        s << timestamp();
        serializeImpl(s);
    }

    return arr;
}

void SerializableCommand::deserialize(const QByteArray& arr)
{
    QDataStream s(arr);
    s.setVersion(QDataStream::Qt_5_3);

    quint32 stmp;
    s >> stmp;

    setTimestamp(stmp);

    deserializeImpl(s);
}
