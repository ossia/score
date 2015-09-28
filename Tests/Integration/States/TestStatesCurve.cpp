#include <IscoreIntegrationTests.hpp>
#include <Editor/TimeConstraint.h>
#include <Editor/Scenario.h>

class TestStatesCurve: public IscoreTestBase
{
        Q_OBJECT
    public:
        using IscoreTestBase::IscoreTestBase;

    private slots:
        void testGui()
        {
            const auto& baseconstraint = getBaseScenario().baseConstraint()->OSSIAConstraint();
            QVERIFY(baseconstraint->timeProcesses().size() == 1); // There is a scenario at the beginning
            QVERIFY(dynamic_cast<OSSIA::Scenario*>(baseconstraint->timeProcesses().front().get()));
        }
};

ISCORE_INTEGRATION_TEST(TestStatesCurve)

#include "TestStatesCurve.moc"
