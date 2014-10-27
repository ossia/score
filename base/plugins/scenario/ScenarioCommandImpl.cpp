#include "ScenarioCommandImpl.hpp"
#include "ScenarioCommand.hpp"

ScenarioCommandImpl::ScenarioCommandImpl(QPointF pos):
    Command{QString("ScenarioCommand"),
            QString("ScenarioCommandImpl"),
            QString("Increment process")}
{
    m_position = pos;
}

virtual QByteArray serialize()
{
    auto arr = Command::serialize();
    {
        QDataStream s(&arr, QIODevice::Append);
        s.setVersion(QDataStream::Qt_5_3);

        s << 42;
    }

    return arr;
}

void deserialize(QByteArray arr)
{
    QBuffer buf(&arr, nullptr);
    buf.open(QIODevice::ReadOnly);
    cmd_deserialize(&buf);

    QDataStream stream(&buf);
    int test;
    stream >> test;
}

virtual void undo()
{
    qDebug(Q_FUNC_INFO);
    auto target = qApp->findChild<ScenarioCommand*>(parentName());
    if(target)
        target->decrementProcesses();
}

virtual void redo()
{
    qDebug(Q_FUNC_INFO);
    auto target = qApp->findChild<ScenarioCommand*>(parentName());
    if(target)
        target->incrementProcesses();
        target->emitCreateTimeEvent(m_position);
}

