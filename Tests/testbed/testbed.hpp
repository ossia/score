#pragma once
#include <Device/Node/DeviceNode.hpp>


class RealNode : public Device::Node
{
    public:
        using Device::Node::Node;
        static constexpr bool is_node = true;
        using node_type = Device::Node;
};

// Par défaut utiliser TSerializer, et avoir le cas non-template par défaut
template<typename T>
struct TSerializer<DataStream, std::enable_if_t<T::is_node>, T>
{
        static void readFrom(
                DataStream::Serializer& s,
                const T& n)
        {
            s.readFrom(static_cast<const typename T::node_type&>(n));
        }


        static void writeTo(
                DataStream::Deserializer& s,
                TreeNode<T>& n)
        {
            s.writeTo(static_cast<const typename T::node_type&>(n));
        }
};
