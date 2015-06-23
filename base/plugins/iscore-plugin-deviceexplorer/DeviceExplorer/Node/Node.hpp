#pragma once
#include <QString>
#include <QList>
#include <QJsonObject>

#include <DeviceExplorer/Protocol/DeviceSettings.hpp>
#include <Plugin/Common/AddressSettings/AddressSettings.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

struct InvisibleRootNodeTag{};
class Node
{
        friend void Visitor<Reader<DataStream>>::readFrom<Node>(const Node& ev);
        friend void Visitor<Reader<JSONObject>>::readFrom<Node>(const Node& ev);
        friend void Visitor<Writer<DataStream>>::writeTo<Node>(Node& ev);
        friend void Visitor<Writer<JSONObject>>::writeTo<Node>(Node& ev);

    public:
        Node() = default;

        Node(InvisibleRootNodeTag);
        bool isInvisibleRoot() const;

        // Address
        Node(const AddressSettings& settings,
             Node* parent = nullptr);

        // Device
        Node(const DeviceSettings& settings,
             Node* parent = nullptr);

        // Clone
        Node(const Node& source,
             Node* parent = nullptr);
        Node& operator=(const Node& source);

        ~Node();

        QStringList fullPathWithDevice() const;
        QStringList fullPathWithoutDevice() const;

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
        QString displayName() const;

        bool isSelectable() const;
        bool isEditable() const;

        bool isDevice() const;
        void setDeviceSettings(const DeviceSettings& settings);
        const DeviceSettings& deviceSettings() const;
        DeviceSettings& deviceSettings();

        void setAddressSettings(const AddressSettings& settings);
        const AddressSettings& addressSettings() const;
        AddressSettings& addressSettings();

        Node* clone() const;

    protected:
        Node* m_parent {};
        QList<Node*> m_children;

        // TODO make an union here maybe ?
        DeviceSettings m_deviceSettings;
        AddressSettings m_addressSettings;
};

Node* getNodeFromString(Node* n, QStringList&& str);
