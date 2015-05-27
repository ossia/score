#pragma once

#include <QDataStream>
#include "DeviceExplorer/Node/Node.hpp"

class BinaryReader
{
        QDataStream& m_stream;

    public:
        BinaryReader(QDataStream& stream)
            : m_stream(stream)
        {
        }

        void read(Node* parent)
        {
            Q_ASSERT(parent);

            QString name;
            m_stream >> name;

            Node* n = new Node(name, parent);

            //TODO: type !
            QString value;
            m_stream >> value;
            qint32 ioType;
            m_stream >> ioType;
            float minValue, maxValue;
            m_stream >> minValue;
            m_stream >> maxValue;
            quint32 priority;
            m_stream >> priority;
            //TODO: cropMode, tags, ...

            n->setValue(value);
            n->setIOType(static_cast<IOType>(ioType));
            n->setMinValue(minValue);
            n->setMaxValue(maxValue);
            n->setPriority(priority);

            quint32 childCount;
            m_stream >> childCount;

            for(quint32 i = 0; i < childCount; ++i)
            {
                read(n);
            }

        }
};
