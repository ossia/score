// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QtTest/QtTest>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Rack/RackModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <score/model/path/Path.hpp>

class IntervalModelTests : public QObject
{
  Q_OBJECT
public:
  IntervalModelTests() : QObject{}
  {
  }

private Q_SLOTS:

  void CreateSlotTest()
  {
    IntervalModel model{Id<IntervalModel>{0}, Id<IntervalViewModel>{0},
                          this};
    auto content_id = getStrongId(model.rackes());
    model.createRack(content_id);
    auto rack = model.rack(content_id);
    QVERIFY(rack != nullptr);

    auto slot_id = getStrongId(rack->getSlots());
    rack->addSlot(new SlotModel{slot_id, rack});
    auto slot = rack->slot(slot_id);
    QVERIFY(slot != nullptr);
  }

  void DeleteSlotTest()
  {
    /////
    {
      IntervalModel model{Id<IntervalModel>{0}, Id<IntervalViewModel>{0},
                            this};
      auto content_id = getStrongId(model.rackes());
      model.createRack(content_id);
      auto rack = model.rack(content_id);

      auto slot_id = getStrongId(rack->getSlots());
      rack->addSlot(new SlotModel{slot_id, rack});
      rack->slotmodels.remove(slot_id);
      model.removeRack(content_id);
    }

    //////
    {
      IntervalModel model{Id<IntervalModel>{0}, Id<IntervalViewModel>{0},
                            this};
      auto content_id = getStrongId(model.rackes());
      model.createRack(content_id);
      auto rack = model.rack(content_id);

      rack->addSlot(new SlotModel{getStrongId(rack->getSlots()), rack});

      rack->addSlot(new SlotModel{getStrongId(rack->getSlots()), rack});

      rack->addSlot(new SlotModel{getStrongId(rack->getSlots()), rack});

      model.removeRack(content_id);
    }
  }

  void FindSubProcessTest()
  {
    IntervalModel i0{Id<IntervalModel>{0}, Id<IntervalViewModel>{0},
                       qApp};
    i0.setObjectName("OriginalInterval");
    auto s0 = new ScenarioModel{std::chrono::seconds(15), Id<ProcessModel>{0},
                                &i0};

    auto int_0_id = getStrongId(s0->intervals());
    auto ev_0_id = getStrongId(s0->events());
    auto fv_0_id = Id<IntervalViewModel>{234};
    auto tb_0_id = getStrongId(s0->timeSyncs());
    StandardCreationPolicy::createIntervalAndEndEventFromEvent(
        *s0,
        s0->startEvent()->id(),
        std::chrono::milliseconds{34},
        10,
        int_0_id,
        fv_0_id,
        ev_0_id);

    auto int_2_id = getStrongId(s0->intervals());
    auto fv_2_id = Id<IntervalViewModel>{454};
    auto ev_2_id = getStrongId(s0->events());
    auto tb_2_id = getStrongId(s0->timeSyncs());
    StandardCreationPolicy::createIntervalAndEndEventFromEvent(
        *s0,
        s0->startEvent()->id(),
        std::chrono::milliseconds{46},
        10,
        int_2_id,
        fv_2_id,
        ev_2_id);

    auto i1 = s0->interval(int_0_id);
    auto s1
        = new ScenarioModel{std::chrono::seconds(15), Id<ProcessModel>{0}, i1};
    (void)s1;
    auto s2
        = new ScenarioModel{std::chrono::seconds(15), Id<ProcessModel>{1}, i1};

    ObjectPath p{{"OriginalInterval", {0}},
                 {"ScenarioModel", 0},
                 {"IntervalModel", int_0_id},
                 {"ScenarioModel", 1}};
    QCOMPARE(p.find<QObject>(), s2);

    ObjectPath p2{{"OriginalInterval", {0}},
                  {"ScenarioModel", 0},
                  {"IntervalModel", int_0_id},
                  {"ScenarioModel", 7}};

    try
    {
      p2.find<QObject>();
      QFAIL("Exception not thrown");
    }
    catch (...)
    {
    }

    ObjectPath p3{{"OriginalInterval", {0}},
                  {"ScenarioModel", 0},
                  {"IntervalModel0xBADBAD", int_0_id},
                  {"ScenarioModel", 1}};

    try
    {
      p3.find<QObject>();
      QFAIL("Exception not thrown");
    }
    catch (...)
    {
    }

    ObjectPath p4{{"OriginalInterval", {0}},
                  {"ScenarioModel", 0},
                  {"IntervalModel", int_0_id},
                  {"ScenarioModel", 1},
                  {"ScenarioModel", 1}};

    try
    {
      p4.find<QObject>();
      QFAIL("Exception not thrown");
    }
    catch (...)
    {
    }
  }
};

QTEST_MAIN(IntervalModelTests)
#include "IntervalModelTests.moc"
