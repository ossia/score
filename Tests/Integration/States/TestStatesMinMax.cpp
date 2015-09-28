#include <IscoreIntegrationTests.hpp>
#include <Mocks/MockDevice.hpp>

class TestStatesMinMax: public IscoreTestBase
{
        Q_OBJECT
    public:
        TestStatesMinMax(int& argc, char** argv):
            IscoreTestBase(argc, argv)
        {
            SingletonProtocolList::instance().registerFactory(new MockDeviceFactory);
        }

        void setupDevice()
        {
            // We load a device
            iscore::DeviceSettings s;
            s.protocol = "Mock";
            s.name = "MockDevice";

            iscore::Node n{s, nullptr};
            loadDeviceFromXML("TestData/test.namespace.xml", n);
            redo(new LoadDevice{pluginModel<DeviceDocumentPlugin>(), std::move(n)});
        }

    private slots:
        void testMinMaxChanges()
        {
            setupDevice();

            // We add a constraint
            using namespace Scenario::Command;
            auto& scenar = static_cast<ScenarioModel&>(*getBaseElementModel().baseConstraint().processes.begin());
            auto newStateCmd = new CreateState(scenar, scenar.startEvent().id(), 0);
            redo(newStateCmd);

            auto newConstraintCmd = new CreateConstraint_State_Event_TimeNode(scenar, newStateCmd->createdState(), TimeValue::fromMsecs(500), 0);
            redo(newConstraintCmd);
            auto& createdConstraint = *scenar.constraints.begin();

            // We add an automation
            auto addProc = new AddProcessToConstraint(createdConstraint, "Automation");
            redo(addProc);
            auto& autom = static_cast<AutomationModel&>(*createdConstraint.processes.begin());

            // We set the automation's address
            iscore::Address addr{"MockDevice", {"test1"}};
            auto setAddr = new ChangeAddress(autom, addr);
            redo(setAddr);

            // We add start and end states [-30, 50] while the address is bewteen [-10; 10]
            auto addStartState = new AddMessagesToModel(
                                     scenar.states.at(newStateCmd->createdState()).messages(),
                                     iscore::MessageList{iscore::Message{addr, -30.}});
            redo(addStartState);

            auto addEndState = new AddMessagesToModel(
                                     scenar.states.at(newConstraintCmd->createdState()).messages(),
                                     iscore::MessageList{iscore::Message{addr, 50.}});
            redo(addEndState);

            // We check that the address min/max becomes [-30; 50]
            QVERIFY(autom.min() == -30);
            QVERIFY(autom.max() == 50);

            undo();

            // We check that the address min/max didn't change
            QVERIFY(autom.min() == -30);
            QVERIFY(autom.max() == 50);
        }

        void testMinMaxDoesNotChange()
        {
            setupDevice();

            // We add a constraint
            using namespace Scenario::Command;
            auto& scenar = static_cast<ScenarioModel&>(*getBaseElementModel().baseConstraint().processes.begin());
            auto newStateCmd = new CreateState(scenar, scenar.startEvent().id(), 0);
            redo(newStateCmd);

            auto newConstraintCmd = new CreateConstraint_State_Event_TimeNode(scenar, newStateCmd->createdState(), TimeValue::fromMsecs(500), 0);
            redo(newConstraintCmd);
            auto& createdConstraint = *scenar.constraints.begin();

            // We add an automation
            auto addProc = new AddProcessToConstraint(createdConstraint, "Automation");
            redo(addProc);
            auto& autom = static_cast<AutomationModel&>(*createdConstraint.processes.begin());

            // We set the automation's address
            iscore::Address addr{"MockDevice", {"test1"}};
            auto setAddr = new ChangeAddress(autom, addr);
            redo(setAddr);

            // We add start and end states [-5, 5] while the address is bewteen [-10; 10]
            auto addStartState = new AddMessagesToModel(
                                     scenar.states.at(newStateCmd->createdState()).messages(),
                                     iscore::MessageList{iscore::Message{addr, -5.}});
            redo(addStartState);

            auto addEndState = new AddMessagesToModel(
                                   scenar.states.at(newConstraintCmd->createdState()).messages(),
                                   iscore::MessageList{iscore::Message{addr, 5.}});
            redo(addEndState);

            // We check that the address min/max does not change
            QVERIFY(autom.min() == -10);
            QVERIFY(autom.max() == 10);
        }
};

ISCORE_INTEGRATION_TEST(TestStatesMinMax)

#include "TestStatesMinMax.moc"
