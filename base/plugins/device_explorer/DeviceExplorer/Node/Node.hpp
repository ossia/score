#pragma once
#include <QString>
#include <QList>
#include <QJsonObject>
#include <QStringList>

#include <DeviceExplorer/Protocol/DeviceSettings.hpp>
#include "Common/AddressSettings/AddressSettings.hpp"
/**
 * Note: JM : the Commands should operat on the Node hierarchy.
 * When updated, a Node should update both :
 *  - The DeviceExplorerModel (via signals / slots)
 *  - The sub-jacent Device.
 *
 * The sub-jacent device should also update the Node.
 */
class Node
{
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

        void setName(const QString& name);
        void setValue(const QString& value);
        void setValueType(const QString& value);
        void setIOType(const Node::IOType ioType);
        void setIOType(const QString ioType);
        void setMinValue(float minV);
        void setMaxValue(float maxV);
        void setPriority(unsigned int priority);

        bool isSelectable() const; //TODO: or has a child of ioType != Node::In !!!

        bool isEditable() const;
        bool isDevice() const;
        void setDeviceSettings(const DeviceSettings& settings);
        const DeviceSettings& deviceSettings() const;
        void setAddressSettings(const AddressSettings& settings);
        const AddressSettings addressSettings();
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

        DeviceSettings m_deviceSettings;
        AddressSettings m_addressSettings;
};

Node* makeNode(const AddressSettings& addressSettings);
