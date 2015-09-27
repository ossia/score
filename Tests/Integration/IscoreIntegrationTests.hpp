#include <QtWidgets>
#include <QtTest/QtTest>
#include <core/application/Application.hpp>

#include <OSSIAControl.hpp>
#include <DocumentPlugin/OSSIABaseScenarioElement.hpp>
#include <DocumentPlugin/OSSIAConstraintElement.hpp>
#include <DocumentPlugin/OSSIAEventElement.hpp>
#include <DocumentPlugin/OSSIAStateElement.hpp>
#include <DocumentPlugin/OSSIATimeNodeElement.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/presenter/Presenter.hpp>
#include <iscore_static_plugins.hpp>

QT_BEGIN_NAMESPACE
QTEST_ADD_GPU_BLACKLIST_SUPPORT_DEFS
QT_END_NAMESPACE
class IscoreTestBase : public QObject
{
    public:
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

        iscore::Application app;

    private:
        OSSIAControl* getOSSIAControl() const
        {
            const auto& ctrls = app.presenter()->pluginControls();
            auto it = std::find_if(ctrls.begin(), ctrls.end(), [] (QObject* obj)
            { return dynamic_cast<OSSIAControl*>(obj); });

            return static_cast<OSSIAControl*>(*it);
        }
};

#define ISCORE_INTEGRATION_TEST(TestObject) \
int main(int argc, char** argv) \
{ \
    TestObject tc(argc, argv); \
    QTEST_SET_MAIN_SOURCE_PATH \
    return QTest::qExec(&tc, argc, argv); \
}
