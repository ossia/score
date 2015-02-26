#include <core/presenter/command/SerializableCommand.hpp>
using namespace iscore;

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

    int stmp;
    s >> stmp;

    setTimestamp(stmp);

    deserializeImpl(s);
}
