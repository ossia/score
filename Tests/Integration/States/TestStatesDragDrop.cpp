#include <IscoreIntegrationTests.hpp>
#include <Mocks/MockDevice.hpp>

class TestStatesDragDrop: public TestBase
{
        Q_OBJECT
    public:
        TestStatesDragDrop(int& argc, char** argv):
            TestBase(argc, argv)
        {
            SingletonProtocolList::instance().registerFactory(new MockDeviceFactory);
        }

        void setupDevice()
        {
            // We load a device
            score::DeviceSettings s;
            s.protocol = "Mock";
            s.name = "MockDevice";

            score::Node n{s, nullptr};
            loadDeviceFromXML("TestData/test.namespace.xml", n);
            redo(new LoadDevice{pluginModel<DeviceDocumentPlugin>(), std::move(n)});
        }

    private Q_SLOTS:
        void testDragUndoRedo()
        {
            SCORE_TODO;
            /*
            // We add a constraint
            using namespace Scenario::Command;
            auto& scenar = static_cast<ScenarioModel&>(*getScenarioDocumentModel().baseConstraint().processes.begin());
            auto newStateCmd = new CreateState(scenar, scenar.startEvent().id(), 0);
            redo(newStateCmd);

            score::Address addr{"MockDevice", {"test1"}};

            // We add states
            auto& stateMessages = scenar.states.at(newStateCmd->createdState()).messages();
            auto addStartState = new AddMessagesToModel{
                                     stateMessages,
                                     score::MessageList{score::Message{addr, -30.}}};
            redo(addStartState);

            // Check correct insertion of the state
            QVERIFY(stateMessages.rootNode().childCount() == 1);

            auto& dev_node = stateMessages.rootNode().childAt(0);
            QVERIFY(dev_node.is<score::DeviceSettings>());

            auto& dev_var = dev_node.get<score::DeviceSettings>();
            QVERIFY(dev_var.name == addr.device);

            QVERIFY(dev_node.childCount() == 1);

            auto& addr_node = dev_node.childAt(0);
            QVERIFY(addr_node.is<score::AddressSettings>());
            auto& addr_var = addr_node.get<score::AddressSettings>();
            QVERIFY(addr_var.name == "test1");
            QVERIFY(addr_var.value.val == -30.);

            // Check that undo is done correctly
            undo();

            QVERIFY(stateMessages.rootNode().childCount() == 0);
            */
        }

        void testDragUndoRedoWithDevice()
        {
            setupDevice();

            testDragUndoRedo();
        }

};

SCORE_INTEGRATION_TEST(TestStatesDragDrop)

#include "TestStatesDragDrop.moc"
