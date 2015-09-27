#include <IscoreIntegrationTests.hpp>
#include <Editor/TimeConstraint.h>

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
        }
};

ISCORE_INTEGRATION_TEST(Test1)

#include "Test1.moc"
