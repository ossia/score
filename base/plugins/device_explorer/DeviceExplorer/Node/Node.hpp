#pragma once
#include <QString>
#include <QList>
#include <QJsonObject>
//#include <QStringList>

#include <DeviceExplorer/Protocol/DeviceSettings.hpp>
#include <Plugin/Common/AddressSettings/AddressSettings.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
/**
 * Note: JM : the Commands should operate on the Node hierarchy.
 * When updated, a Node should update both :
 *  - The DeviceExplorerModel (via signals / slots)
 *  - The sub-jacent Device.
 *
 * The sub-jacent device should also update the Node.
 */
class Node
{
        friend void Visitor<Reader<DataStream>>::readFrom<Node>(const Node& ev);
        friend void Visitor<Reader<JSONObject>>::readFrom<Node>(const Node& ev);
        friend void Visitor<Writer<DataStream>>::writeTo<Node>(Node& ev);
        friend void Visitor<Writer<JSONObject>>::writeTo<Node>(Node& ev);

    public:
        static QString INVALID_STR;
        static float INVALID_FLOAT;

        typedef enum {Invalid, In, Out, InOut} IOType;

        Node(const QString& name = QString(),
             Node* parent = nullptr);
        Node(const DeviceSettings& devices,
             const QString& name = QString(),
             Node* parent = nullptr);

        ~Node();

        QString fullPath() const;

        void setParent(Node* parent);
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
        void removeChild(Node* child);

        //- accessors

        QString name() const;
        QString value() const;
        Node::IOType ioType() const;
        float minValue() const;
        float maxValue() const;
        unsigned int priority() const;
        QString tags() const;

        void setName(const QString& name);
        void setValue(const QString& value);
        void setValueType(const QString& value);
        void setIOType(const Node::IOType ioType);
        void setIOType(const QString ioType);
        void setMinValue(float minV);
        void setMaxValue(float maxV);
        void setPriority(unsigned int priority);
        void setTags(const QString &tags);

        bool isSelectable() const; //TODO: or has a child of ioType != Node::In !!!

        bool isEditable() const;
        bool isDevice() const;
        void setDeviceSettings(const DeviceSettings& settings);
        const DeviceSettings& deviceSettings() const;
        void setAddressSettings(const AddressSettings& settings);
        const AddressSettings& addressSettings() const;
        Node* clone() const;

protected:
        Node* m_parent {};
        QList<Node*> m_children;

        DeviceSettings m_deviceSettings;
        AddressSettings m_addressSettings;
};

Node* makeNode(const AddressSettings& addressSettings);

Node* getNodeFromString(Node* n, QStringList&& str);
