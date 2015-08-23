#pragma once
#include <QString>
#include <QList>
#include <QJsonObject>

#include <DeviceExplorer/Protocol/DeviceSettings.hpp>
#include <DeviceExplorer/Address/AddressSettings.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include <iscore/tools/TreeNode.hpp>
namespace iscore
{

class DeviceExplorerNode
{
        ISCORE_SERIALIZE_FRIENDS(DeviceExplorerNode, DataStream)
        ISCORE_SERIALIZE_FRIENDS(DeviceExplorerNode, JSONObject)

    public:
            enum class Type { Device, Address, RootNode };
        DeviceExplorerNode():
            m_data{InvisibleRootNodeTag{}}
        {

        }

        DeviceExplorerNode(InvisibleRootNodeTag t):
            m_data{t}
        {
        }

        bool isInvisibleRoot() const
        {
            return m_data.which() == (int)Type::RootNode;
        }

        // Address
        DeviceExplorerNode(const iscore::AddressSettings& settings):
            m_data{settings}
        {
        }

        // Device
        DeviceExplorerNode(const DeviceSettings& settings) :
            m_data{settings}
        {
        }


        bool isDevice() const;

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

        Type type() const
        {
            return Type(m_data.which());
        }

    private:
        eggs::variant<
            iscore::DeviceSettings,
            iscore::AddressSettings,
            InvisibleRootNodeTag> m_data;
};

using Node = TreeNode<DeviceExplorerNode>;

iscore::Address address(const Node& treeNode);


Node* try_getNodeFromString(Node* n, QStringList&& str);
Node* getNodeFromString(Node* n, QStringList&& str); // Fails if not present.
}
