#include <iscore/command/SerializableCommand.hpp>
using namespace iscore;

SerializableCommand::~SerializableCommand()
{
}

QByteArray SerializableCommand::serialize() const
{
    QByteArray arr;
    {
        QDataStream s(&arr, QIODevice::Append);
        s.setVersion(QDataStream::Qt_5_3);

        s << timestamp();
        DataStreamInput inp{s};
        serializeImpl(inp);
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

    DataStreamOutput outp{s};
    deserializeImpl(outp);
}
