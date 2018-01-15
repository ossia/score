#include <IscoreIntegrationTests.hpp>
#include <Mocks/MockDevice.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/Linear/LinearCurveSegmentModel.hpp>

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
            score::DeviceSettings s;
            s.protocol = "Mock";
            s.name = "MockDevice";

            score::Node n{s, nullptr};
            loadDeviceFromXML("TestData/test.namespace.xml", n);
            redo(new LoadDevice{pluginModel<DeviceDocumentPlugin>(), std::move(n)});
        }

    private Q_SLOTS:
        void testStatesChanges()
        {
            setupDevice();

            // We add a constraint
            using namespace Scenario::Command;
            auto& scenar = static_cast<ScenarioModel&>(*getScenarioDocumentModel().baseConstraint().processes.begin());
            auto newStateCmd = new CreateState(scenar, scenar.startEvent().id(), 0);
            redo(newStateCmd);

            auto newConstraintCmd = new CreateConstraint_State_Event_TimeSync(scenar, newStateCmd->createdState(), TimeValue::fromMsecs(500), 0);
            redo(newConstraintCmd);

            score::Address addr{"MockDevice", {"test1"}};
            score::Address bad_addr{"MockDevice", {"this", "does", "not", "exist"}};
            // The address is in [-10; 10]

            SCORE_TODO;
            /*
            // We add start and end states -3, 7
            auto& startMessages = scenar.states.at(newStateCmd->createdState()).messages();
            auto addStartState = new AddMessagesToModel(
                                     startMessages,
                                     score::MessageList{score::Message{addr, -3.}});
            redo(addStartState);

            auto& endMessages = scenar.states.at(newConstraintCmd->createdState()).messages();
            auto addEndState = new AddMessagesToModel(
                                     endMessages,
                                     score::MessageList{score::Message{addr, 7.}});
            redo(addEndState);
            */


            auto& createdConstraint = *scenar.constraints.begin();

            // We add an automation
            auto addProc = new AddProcessToConstraint(createdConstraint, "Automation");
            redo(addProc);
            auto& autom = static_cast<AutomationModel&>(*createdConstraint.processes.begin());

            // We set the automation's address
            auto setAddr = new ChangeAddress(autom, addr);
            redo(setAddr);

            // We check that the address min/max does not change
            // TODO: it should become min/max(addr) % min/max(state) ?
            QVERIFY(autom.min() == -10);
            QVERIFY(autom.max() == 10);

            SCORE_TODO;
            /*
            // The states change with the new address
            {
                auto sml = startMessages.flatten();
                QVERIFY(sml.size() == 1);
                QVERIFY(sml.at(0).value.val == -10.);

                auto eml = endMessages.flatten();
                QVERIFY(eml.size() == 1);
                QVERIFY(eml.at(0).value.val == 10.);
            }
            */

            // We change the curve manually.
            auto curveUpd = new UpdateCurve{autom.curve(), {{
                    Id<CurveSegmentModel>{0},
                    {0, 0.5}, {1, 0.2},
                    Id<CurveSegmentModel>{}, Id<CurveSegmentModel>{},
                    "Linear", QVariant::fromValue(LinearCurveSegmentData{})}}
            };
            redo(curveUpd);

            // Min and max does not change
            QVERIFY(autom.min() == -10);
            QVERIFY(autom.max() == 10);

            SCORE_TODO;
            /*
            // We check that the states have changed :
            {
                auto sml = startMessages.flatten();
                QVERIFY(sml.size() == 1);
                QVERIFY(sml.at(0).value.val == 0.); // Because (-10 + 10) * 0.5 == 0

                auto eml = endMessages.flatten();
                QVERIFY(eml.size() == 1);
                QVERIFY(eml.at(0).value.val == 0.2 * 20 - 10); // Idem for end state
            }
            */
            undo(); // Undo curve change

            SCORE_TODO;
            /*
            // We get out old states back
            {
                auto sml = startMessages.flatten();
                QVERIFY(sml.size() == 1);
                QVERIFY(sml.at(0).value.val == -10.);

                auto eml = endMessages.flatten();
                QVERIFY(eml.size() == 1);
                QVERIFY(eml.at(0).value.val == 10.);
            }
            */

            undo(); // Undo address change

            SCORE_TODO;
            /*
            // We get out old states back
            {
                auto sml = startMessages.flatten();
                QVERIFY(sml.size() == 1);
                QVERIFY(sml.at(0).value.val == -3.);

                auto eml = endMessages.flatten();
                QVERIFY(eml.size() == 1);
                QVERIFY(eml.at(0).value.val == 7.);
            }
            */
        }
};

SCORE_INTEGRATION_TEST(TestStatesCurve)

#include "TestStatesCurve.moc"
