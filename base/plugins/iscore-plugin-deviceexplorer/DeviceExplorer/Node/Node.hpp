#pragma once
#include <QString>
#include <QList>
#include <QJsonObject>

#include <DeviceExplorer/Protocol/DeviceSettings.hpp>
#include <DeviceExplorer/Address/AddressSettings.hpp>

#include <iscore/tools/TreeNode.hpp>

template<typename... Args>
class VariantBasedNode
{
    public:
        VariantBasedNode(const VariantBasedNode& t) = default;
        VariantBasedNode(VariantBasedNode&& t) = default;
        VariantBasedNode& operator=(const VariantBasedNode& t) = default;
        template<typename T>
        VariantBasedNode(const T& t):
            m_data{t}
        {

        }

        template<typename T>
        bool is() const { return m_data.template target<T>() != nullptr; }

        template<typename T>
        void set(const T& t) { m_data = t; }

        template<typename T>
        const T& get() const { return *m_data.template target<T>(); }

        template<typename T>
        T& get() { return *m_data.template target<T>(); }

        auto which() const
        { return m_data.which(); }

    protected:
        eggs::variant<Args...> m_data;
};

#include <boost/mpl/list.hpp>
#include <boost/mpl/for_each.hpp>
template<typename... Args>
void Visitor<Reader<DataStream>>::readFrom(const eggs::variant<Args...>& var)
{
    m_stream << (int32_t)var.which();

    // This trickery iterates over all the types in Args...
    // A single type should be serialized, even if we cannot break.
    typedef boost::mpl::list<Args...> typelist;
    bool done = false;
    boost::mpl::for_each<typelist>([&] (auto&& elt) {
        if(done)
            return;

        if(auto res = var.template target<decltype(elt)>())
        {
            readFrom(res);
            done = true;
        }
    });

    insertDelimiter();
}


template<typename... Args>
void Visitor<Writer<DataStream>>::writeTo(eggs::variant<Args...>& var)
{
    int32_t which;
    m_stream >> which;

    // Here we iterate until we are on the correct type, and we deserialize it.
    int i = 0;
    typedef boost::mpl::list<Args...> typelist;
    boost::mpl::for_each<typelist>([&] (auto&& elt) {
        if(i++ != which)
            return;

        decltype(elt) data;
        writeTo(data);
        var = data;
    });

    checkDelimiter();
}

template<typename... Args>
void Visitor<Reader<JSONObject>>::readFrom(const eggs::variant<Args...>&)
{/*
    readFrom(static_cast<const T&>(n));
    m_obj["Children"] = toJsonArray(n.children());*/
}

template<typename... Args>
void Visitor<Writer<JSONObject>>::writeTo(eggs::variant<Args...>&)
{/*
    writeTo(static_cast<T&>(n));
    for (const auto& val : m_obj["Children"].toArray())
    {
        auto child = new TreeNode<T>;
        Deserializer<JSONObject> nodeWriter(val.toObject());

        nodeWriter.writeTo(*child);
        n.addChild(child);
    }*/
}


namespace iscore
{

// TODO rename the file
class DeviceExplorerNode : public VariantBasedNode<
        iscore::DeviceSettings,
        iscore::AddressSettings,
        InvisibleRootNodeTag>
{
        ISCORE_SERIALIZE_FRIENDS(DeviceExplorerNode, DataStream)
        ISCORE_SERIALIZE_FRIENDS(DeviceExplorerNode, JSONObject)

    public:
            enum class Type { Device, Address, RootNode };
        using device_type = DeviceSettings;
        using address_type = AddressSettings;
        using root_type = InvisibleRootNodeTag;

        DeviceExplorerNode(const DeviceExplorerNode& t) = default;
        DeviceExplorerNode(DeviceExplorerNode&& t) = default;
        DeviceExplorerNode& operator=(const DeviceExplorerNode& t) = default;
        DeviceExplorerNode():
            VariantBasedNode{InvisibleRootNodeTag{}}
        {

        }

        template<typename T>
        DeviceExplorerNode(const T& t):
            VariantBasedNode{t}
        {

        }

        //- accessors
        QString displayName() const;

        bool isSelectable() const;
        bool isEditable() const;

        Type type() const
        { return Type(m_data.which()); }

};

using Node = TreeNode<DeviceExplorerNode>;

iscore::Address address(const Node& treeNode);


Node* try_getNodeFromString(Node* n, QStringList&& str);
Node* getNodeFromString(Node* n, QStringList&& str); // Fails if not present.
}



