#pragma once
#include <QString>
#include <QList>
#include <QJsonObject>
#include <QStringList>

class Node
{
    public:
        static QString INVALID_STR;
        static float INVALID_FLOAT;

        typedef enum {Invalid, In, Out, InOut} IOType;

        Node(const QString& name = QString(),
             Node* parent = nullptr);
        Node(const QList<QString>& devices,
             const QString& name = QString(),
             Node* parent = nullptr);

        ~Node();

        Node* parent() const;
        Node* childAt(int index) const;  //return 0 if invalid index
        int indexOfChild(Node* child) const;  //return -1 if not found
        int childCount() const;
        bool hasChildren() const;
        QList<Node*> children() const;

        void insertChild(int index, Node* n);
        void addChild(Node* n);
        void swapChildren(int oldIndex, int newIndex);
        Node* takeChild(int index);

        //- accessors

        QString name() const;
        QString value() const;
        Node::IOType ioType() const;
        float minValue() const;
        float maxValue() const;
        unsigned int priority() const;

        void setName(const QString& name);
        void setValue(const QString& value);
        void setIOType(const Node::IOType ioType);
        void setMinValue(float minV);
        void setMaxValue(float maxV);
        void setPriority(unsigned int priority);

        bool isSelectable() const; //TODO: or has a child of ioType != Node::In !!!

        bool isEditable() const;
        bool isDevice() const;
        const QStringList& deviceSettings() const;
        Node* clone() const;


    protected:
        QString m_name;
        QString m_value;
        IOType m_ioType {};
        float m_min {};
        float m_max {};
        unsigned int m_priority {};
        Node* m_parent {};
        QList<Node*> m_children;

        QStringList m_deviceSettings;
};

QJsonObject nodeToJson(const Node* n);
QDataStream& operator<<(QDataStream& s, const Node& n);
