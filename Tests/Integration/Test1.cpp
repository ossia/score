#include <IscoreIntegrationTests.hpp>
#include <Editor/TimeConstraint.h>
#include <Editor/Scenario.h>

class Test1: public IscoreTestBase
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

ISCORE_INTEGRATION_TEST(Test1)

#include "Test1.moc"
