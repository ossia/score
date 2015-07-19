#pragma once
#include <QString>
#include <QList>
#include <QJsonObject>

#include <DeviceExplorer/Protocol/DeviceSettings.hpp>
#include <DeviceExplorer/Address/AddressSettings.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

struct InvisibleRootNodeTag{};
namespace iscore
{

class Node
{
        ISCORE_SERIALIZE_FRIENDS(Node, DataStream)
        ISCORE_SERIALIZE_FRIENDS(Node, JSONObject)

    public:
            enum class Type { Device, Address, RootNode };
        Node();

        Node(InvisibleRootNodeTag);
        bool isInvisibleRoot() const;

        // Address
        Node(const iscore::AddressSettings& settings,
             Node* parent = nullptr);

        // Device
        Node(const DeviceSettings& settings,
             Node* parent = nullptr);
        bool isDevice() const;

        // Clone
        Node(const Node& source,
             Node* parent = nullptr);
        Node& operator=(const Node& source);

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
        QString displayName() const;

        bool isSelectable() const;
        bool isEditable() const;

        void setDeviceSettings(const iscore::DeviceSettings& settings);
        const iscore::DeviceSettings& deviceSettings() const;
        iscore::DeviceSettings& deviceSettings();

        void setAddressSettings(const iscore::AddressSettings& settings);
        const iscore::AddressSettings& addressSettings() const;
        iscore::AddressSettings& addressSettings();

        Node* clone() const;

        iscore::Address address() const;

        Type type() const;

    private:
        Node* m_parent {};
        QList<Node*> m_children;

        union {
        iscore::DeviceSettings m_deviceSettings;
        iscore::AddressSettings m_addressSettings;
        bool m_rootNode;
        };

        Type m_type;
};

Node* getNodeFromString(Node* n, QStringList&& str);
}
