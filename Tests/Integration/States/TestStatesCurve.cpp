#include <IscoreIntegrationTests.hpp>
#include <Mocks/MockDevice.hpp>

class TestStatesCurve: public IscoreTestBase
{
        Q_OBJECT
    public:
        TestStatesCurve(int& argc, char** argv):
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
        void testStatesChanges()
        {
            setupDevice();

            // We add a constraint
            using namespace Scenario::Command;
            auto& scenar = static_cast<ScenarioModel&>(*getBaseElementModel().baseConstraint().processes.begin());
            auto newStateCmd = new CreateState(scenar, scenar.startEvent().id(), 0);
            redo(newStateCmd);

            auto newConstraintCmd = new CreateConstraint_State_Event_TimeNode(scenar, newStateCmd->createdState(), TimeValue::fromMsecs(500), 0);
            redo(newConstraintCmd);

            iscore::Address addr{"MockDevice", {"test1"}};
            // The address is in [-10; 10]

            // We add start and end states -3, 7
            auto& startMessages = scenar.states.at(newStateCmd->createdState()).messages();
            auto addStartState = new AddMessagesToModel(
                                     startMessages,
                                     iscore::MessageList{iscore::Message{addr, -3.}});
            redo(addStartState);

            auto& endMessages = scenar.states.at(newConstraintCmd->createdState()).messages();
            auto addEndState = new AddMessagesToModel(
                                     endMessages,
                                     iscore::MessageList{iscore::Message{addr, 7.}});
            redo(addEndState);


            auto& createdConstraint = *scenar.constraints.begin();

            // We add an automation
            auto addProc = new AddProcessToConstraint(createdConstraint, "Automation");
            redo(addProc);
            auto& autom = static_cast<AutomationModel&>(*createdConstraint.processes.begin());

            // We set the automation's address
            auto setAddr = new ChangeAddress(autom, addr);
            redo(setAddr);

            // We check that the address min/max does not change
            // Note: it should become min/max(addr) % min/max(state)
            QVERIFY(autom.min() == -10);
            QVERIFY(autom.max() == 10);

            //auto curveUpd = new UpdateCurve();

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

ISCORE_INTEGRATION_TEST(TestStatesCurve)

#include "TestStatesCurve.moc"
