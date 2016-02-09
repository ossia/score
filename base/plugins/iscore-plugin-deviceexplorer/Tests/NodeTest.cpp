#include <Device/Node/DeviceNode.hpp>
#include <QObject>
#include <QString>
#include <algorithm>
#include <QtTest/QtTest>

class NodeTest: public QObject
{
        Q_OBJECT

    private slots:
        void NodeTest_1()
        {
            Device::Node root;

            ISCORE_ASSERT(root.is<InvisibleRootNodeTag>());

            {
                Device::Node child(Device::AddressSettings{}, nullptr);
                root.push_back(child);

                ISCORE_ASSERT(root.childCount() == 1);
                ISCORE_ASSERT(root.children().back().parent() == &root);
            }

            {
                Device::Node child(Device::AddressSettings{},&root);
                root.push_back(child);

                ISCORE_ASSERT(root.childCount() == 2);
                ISCORE_ASSERT(root.children().back().parent() == &root);
            }

            {
                Device::Node child(Device::DeviceSettings{}, &root);
                root.push_back(child);

                ISCORE_ASSERT(root.childCount() == 3);
                ISCORE_ASSERT(root.children().back().parent() == &root);
            }

            {
                Device::Node root_copy(root);

                ISCORE_ASSERT(root.childCount() == 3);
                ISCORE_ASSERT(root_copy.childCount() == 3);

                for(const auto& child : root_copy)
                {
                    ISCORE_ASSERT(child.parent() == &root_copy);
                }

                for(int i = 0; i < root.childCount(); i++)
                {
                    ISCORE_ASSERT(&root.children().at(i) != &root_copy.children().at(i));
                    ISCORE_ASSERT(root.children().at(i).which() == root_copy.children().at(i).which());
                }
            }

            {
                Device::Node root_copy;
                root_copy = root;
                root_copy = root;
                root_copy = root;

                ISCORE_ASSERT(root.childCount() == 3);
                ISCORE_ASSERT(root_copy.childCount() == 3);

                for(const auto& child : root_copy)
                {
                    ISCORE_ASSERT(child.parent() == &root_copy);
                }

                for(int i = 0; i < root.childCount(); i++)
                {
                    ISCORE_ASSERT(&root.children().at(i) != &root_copy.children().at(i));
                    ISCORE_ASSERT(root.children().at(i).which() == root_copy.children().at(i).which());
                }
            }

            {
                Device::Node root_copy(std::move(root));

                ISCORE_ASSERT(root.childCount() == 0);
                ISCORE_ASSERT(root_copy.childCount() == 3);

                for(const auto& child : root_copy)
                {
                    ISCORE_ASSERT(child.parent() == &root_copy);
                }

                ISCORE_ASSERT(root_copy.children().at(0).is<Device::AddressSettings>());
                ISCORE_ASSERT(root_copy.children().at(1).is<Device::AddressSettings>());
                ISCORE_ASSERT(root_copy.children().at(2).is<Device::DeviceSettings>());

                root = std::move(root_copy);

                ISCORE_ASSERT(root_copy.childCount() == 0);
                ISCORE_ASSERT(root.childCount() == 3);

                for(const auto& child : root)
                {
                    ISCORE_ASSERT(child.parent() == &root);
                }

                ISCORE_ASSERT(root.children().at(0).is<Device::AddressSettings>());
                ISCORE_ASSERT(root.children().at(1).is<Device::AddressSettings>());
                ISCORE_ASSERT(root.children().at(2).is<Device::DeviceSettings>());
            }
        }


        void NodeTest_2()
        {
            Device::Node root;

            Device::DeviceSettings dev_base{UuidKey<Device::ProtocolFactory>{"NoProtocol"}, "ADevice", {}};
            auto& dev = root.emplace_back(std::move(dev_base), nullptr);

            ISCORE_ASSERT(root.childCount() == 1);
            ISCORE_ASSERT(dev.parent() == &root);
            ISCORE_ASSERT(&dev == &root.children()[0]);
            ISCORE_ASSERT(&dev == &root.childAt(0));

            auto& dev_2 = root.emplace_back(Device::DeviceSettings{UuidKey<Device::ProtocolFactory>{"Bla"}, "OtherDevice", {}}, nullptr);
            ISCORE_ASSERT(root.childCount() == 2);
            ISCORE_ASSERT(dev_2.parent() == &root);

            Device::Node child(Device::AddressSettings{}, &dev_2);
            dev_2.push_back(child);

            ISCORE_ASSERT(root.childCount() == 2);
            ISCORE_ASSERT(dev.childCount() == 0);
            ISCORE_ASSERT(dev_2.childCount() == 1);

            {
                Device::Node root_copy(root);

                ISCORE_ASSERT(root.childCount() == 2);
                ISCORE_ASSERT(root_copy.childCount() == 2);

                for(const auto& child : root_copy)
                {
                    ISCORE_ASSERT(child.parent() == &root_copy);
                }

                for(int i = 0; i < root.childCount(); i++)
                {
                    ISCORE_ASSERT(&root.children().at(i) != &root_copy.children().at(i));
                    ISCORE_ASSERT(root.children().at(i).which() == root_copy.children().at(i).which());
                }

                ISCORE_ASSERT(root_copy.childAt(0).childCount() == 0);
                ISCORE_ASSERT(root_copy.childAt(1).childCount() == 1);

                ISCORE_ASSERT(root_copy.childAt(1).childAt(0).is<Device::AddressSettings>());
                ISCORE_ASSERT(root_copy.childAt(1).childAt(0).parent() == &root_copy.childAt(1));
            }


        }

};

QTEST_MAIN(NodeTest)
#include <Device/Address/AddressSettings.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include "NodeTest.moc"
#include <iscore/tools/TreeNode.hpp>

