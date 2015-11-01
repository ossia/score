#include <QtWidgets>
#include <QtTest/QtTest>
#include <core/application/Application.hpp>

#include <OSSIAControl.hpp>
#include <DocumentPlugin/OSSIABaseScenarioElement.hpp>
#include <DocumentPlugin/OSSIAConstraintElement.hpp>
#include <DocumentPlugin/OSSIAEventElement.hpp>
#include <DocumentPlugin/OSSIAStateElement.hpp>
#include <DocumentPlugin/OSSIATimeNodeElement.hpp>

#include <Editor/TimeConstraint.h>
#include <Editor/Scenario.h>

#include <core/presenter/Presenter.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore_static_plugins.hpp>

#include <Automation/AutomationModel.hpp>
#include <Curve/Commands/UpdateCurve.hpp>

#include <Plugin/Commands/Add/LoadDevice.hpp>
#include <DeviceExplorer/XML/XMLDeviceLoader.hpp>
#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Commands/ChangeAddress.hpp>

#include <Scenario/Document/BaseElement/BaseElementModel.hpp>
#include <Scenario/Document/BaseElement/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateState.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateConstraint_State_Event_TimeNode.hpp>
#include <Scenario/Commands/Constraint/AddProcessToConstraint.hpp>

QT_BEGIN_NAMESPACE
QTEST_ADD_GPU_BLACKLIST_SUPPORT_DEFS
QT_END_NAMESPACE
class IscoreTestBase : public QObject
{
    public:
        iscore::Application app;

        IscoreTestBase(int& argc, char** argv):
            app{ApplicationSettings{false, false, {}}, argc, argv}
        {
            qApp->setAttribute(Qt::AA_Use96Dpi, true);
            QTEST_DISABLE_KEYPAD_NAVIGATION
            QTEST_ADD_GPU_BLACKLIST_SUPPORT
        }

        OSSIABaseScenarioElement& getBaseScenario() const
        {
            return getOSSIAControl()->baseScenario();
        }

        std::shared_ptr<OSSIA::Device> getLocalDevice() const
        {
            return getOSSIAControl()->localDevice();
        }


        void redo(iscore::SerializableCommand* cmd)
        {
            getDocument().commandStack().redoAndPush(cmd);
        }

        void undo()
        {
            getDocument().commandStack().undo();
        }


        OSSIAControl* getOSSIAControl() const
        {
            const auto& ctrls = app.presenter()->pluginControls();
            auto it = std::find_if(ctrls.begin(), ctrls.end(), [] (QObject* obj)
            { return dynamic_cast<OSSIAControl*>(obj); });

            return static_cast<OSSIAControl*>(*it);
        }

        iscore::Document& getDocument() const
        {
            return *app.presenter()->documents()[0];
        }

        BaseElementModel& getBaseElementModel() const
        {
            return iscore::IDocument::get<BaseElementModel>(getDocument());
        }

        template<typename T>
        T& pluginModel()
        {
            return *getDocument().model().pluginModel<T>();
        }

        int exec()
        {
            int cnt = this->metaObject()->methodCount();
            for(int i = 0; i < cnt; i++)
            {
                auto method = this->metaObject()->method(i);
                method.invoke(this);

                auto& stack = getDocument().commandStack();
                while(stack.canUndo())
                    stack.undo();
            }

            return 0;
        }
};

#define ISCORE_INTEGRATION_TEST(TestObject) \
int main(int argc, char** argv) \
{ \
    TestObject tc(argc, argv); \
    QTEST_SET_MAIN_SOURCE_PATH \
    return QTest::qExec(&tc, argc, argv); \
}
