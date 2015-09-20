#include "AddMessagesToModel.hpp"
#include <DeviceExplorer/ItemModels/MessageItemModel.hpp>

// TODO write tests for this.
static iscore::Node merge(
        iscore::Node base,
        const iscore::MessageList& other)
{
    using namespace iscore;
    // For each node in other, if we can also find a similar node in
    // base, we replace its data
    // Else, we insert it.

    ISCORE_ASSERT(base.is<InvisibleRootNodeTag>());

    for(const auto& message : other)
    {
        QStringList path = message.address.path;
        path.prepend(message.address.device);

        ptr<Node> node = &base;
        for(int i = 0; i < path.size(); i++)
        {
            auto& children = node->children();
            auto it = std::find_if(
                        children.begin(), children.end(),
                        [&] (const auto& cur_node) {
                return cur_node.displayName() == path[i];
            });

            if(it == children.end())
            {
                // We have to start adding sub-nodes from here.
                ptr<Node> parentnode{node};
                for(int k = i; k < path.size(); k++)
                {
                    ptr<Node> newNode;
                    if(k == 0)
                    {
                        // We're adding a device
                        iscore::DeviceSettings dev;
                        dev.name = path[k];
                        newNode = &parentnode->emplace_back(std::move(dev), nullptr);
                    }
                    else
                    {
                        // We're adding an address
                        iscore::AddressSettings addr;
                        addr.name = path[k];

                        if(k == path.size() - 1)
                        {
                            // End of the address
                            addr.value = message.value;

                            // Note : since we don't have this
                            // information in messagelist's,
                            // we assign a default Out value
                            // so that we only send the nodes that actually had messages
                            // via the OSSIA api.
                            addr.ioType = IOType::Out;
                        }

                        newNode = &parentnode->emplace_back(std::move(addr), nullptr);
                    }

                    parentnode = newNode;
                }

                break;
            }
            else
            {
                node = &*it;
            }
        }
    }


    return base;
}



AddMessagesToModel::AddMessagesToModel(
        Path<iscore::MessageItemModel>&& path,
        const iscore::MessageList& messages) :
    SerializableCommand {factoryName(),
                         commandName(),
                         description()},
    m_path{std::move(path)}
{
    // Here the state might not exist yet,
    // if drag'n'dropping in a scenario for instance.
    // We can them assume that the created state would have been an empty state.
    auto model = m_path.try_find();
    if(model)
    {
        m_old = model->rootNode();
    }
    m_new = merge(m_old, messages);
}


void AddMessagesToModel::undo()
{
    auto& model = m_path.find();
    model = m_old;
}

void AddMessagesToModel::redo()
{
    auto& model = m_path.find();
    model = m_new;
}

void AddMessagesToModel::serializeImpl(QDataStream& s) const
{
    s << m_path << m_old << m_new;
}

void AddMessagesToModel::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_old >> m_new;
}
