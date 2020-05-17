// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/model/path/Path.hpp>

#include <chrono>

class TimeSyncModelTests : public QObject
{
  Q_OBJECT

public:
private:
  void AddEventTest()
  {
    TimeSyncModel model{Id<TimeSyncModel>(1), TimeValue{std::chrono::milliseconds(1)}, 0.5, this};
  }
};

QTEST_MAIN(TimeSyncModelTests)
#include "TimeSyncModelTests.moc"
