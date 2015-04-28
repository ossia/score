#pragma once

#include <QDataStream>
#include "DeviceExplorer/Node/Node.hpp"

class BinaryWriter
{
        QDataStream& m_stream;

    public:
        BinaryWriter(QDataStream& stream)
            : m_stream(stream)
        {

        }

        void write(const Node* n)
        {
            Q_ASSERT(n);

            m_stream << n->name();
            //TODO: type !
            m_stream << n->value();
            m_stream << (qint32) n->ioType();
            m_stream << n->minValue();
            m_stream << n->maxValue();
            m_stream << (quint32) n->priority();
            //TODO: cropMode, tags, ...

            m_stream << (quint32) n->childCount();
            foreach(Node * child, n->children())
            write(child);

        }

};
