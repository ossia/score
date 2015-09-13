#include <QtTest/QtTest>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>
class NodeTest: public QObject
{
        Q_OBJECT

    private slots:
        void NodeTest_1()
        {
            iscore::Node root;

            ISCORE_ASSERT(root.is<InvisibleRootNodeTag>());

            {
                iscore::Node child(iscore::AddressSettings{}, nullptr);
                root.push_back(child);

                ISCORE_ASSERT(root.childCount() == 1);
                ISCORE_ASSERT(root.children().back().parent() == &root);
            }
        }

};

QTEST_MAIN(NodeTest)
#include "NodeTest.moc"

