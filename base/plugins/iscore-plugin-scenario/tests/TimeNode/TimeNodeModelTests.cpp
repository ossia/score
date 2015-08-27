#include <QtTest/QtTest>
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/Event/EventModel.hpp"

#include "Process/ScenarioModel.hpp"
#include "ProcessInterface/TimeValue.hpp"

#include "iscore/tools/ModelPath.hpp"

#include <chrono>

class TimeNodeModelTests : public QObject
{
        Q_OBJECT

    public:

    private slots:
            void AddEventTest()
            {
                TimeNodeModel model {id_type<TimeNodeModel>(1), TimeValue{std::chrono::milliseconds (1)}, 0.5, this};
            }

};

QTEST_MAIN(TimeNodeModelTests)
#include "TimeNodeModelTests.moc"
