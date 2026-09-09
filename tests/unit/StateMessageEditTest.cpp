// Editing a message of a state through the inspector's tree.
//
// MessageItemModel::setData wrapped its command submission in its own
// beginResetModel()/endResetModel() pair, but the command already resets the
// model when it assigns the new tree. The nested pair made Qt print
//   "beginResetModel called on ... without calling endResetModel first"
// and, more importantly, sent the views two overlapping resets for one edit.
//
// StateModel::sig_statesUpdated -- what the execution component listens to in
// order to push the new messages to the running graph -- is emitted from
// modelReset, so the shape of those resets is exactly what decides whether an
// edit made while playing reaches the engine.

#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/model/tree/TreeNode.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QAbstractItemModel>

#include <catch2/catch_all.hpp>

namespace
{
using Scenario::MessageItemModel;

//! The scenario the empty document starts with, inside the base interval.
Scenario::ProcessModel& base_scenario(score::Document& doc)
{
  auto& itv = score::test::base_interval(doc);
  for(auto& proc : itv.processes)
    if(auto* s = qobject_cast<Scenario::ProcessModel*>(&proc))
      return *s;
  FAIL("no scenario in the base interval");
  throw;
}

Scenario::StateModel& some_state(score::Document& doc)
{
  auto& scenar = base_scenario(doc);
  REQUIRE(scenar.states.size() > 0);
  return *scenar.states.begin();
}

void addMessage(score::Document& doc, Scenario::StateModel& st, const char* addr, float v)
{
  State::Message m;
  m.address = State::AddressAccessor{State::Address::fromString(addr).value()};
  m.value = v;
  CommandDispatcher<>{doc.context().commandStack}
      .submit<Scenario::Command::AddMessagesToState>(st, State::MessageList{m});
}

//! Depth-first walk to the first index whose node actually carries a value.
QModelIndex firstValued(const MessageItemModel& m, QModelIndex parent = {})
{
  for(int i = 0; i < m.rowCount(parent); i++)
  {
    auto idx = m.index(i, 0, parent);
    const auto& n = m.nodeFromModelIndex(idx);
    if(n.hasValue())
      return idx;
    if(auto sub = firstValued(m, idx); sub.isValid())
      return sub;
  }
  return {};
}

//! Catches the qWarning() Qt prints on an unbalanced reset.
struct WarningCatcher
{
  static inline QStringList messages;
  static inline QtMessageHandler previous{};

  WarningCatcher()
  {
    messages.clear();
    previous = qInstallMessageHandler(
        [](QtMsgType t, const QMessageLogContext& c, const QString& msg) {
      if(t == QtWarningMsg || t == QtCriticalMsg)
        messages.push_back(msg);
      if(previous)
        previous(t, c, msg);
    });
  }
  ~WarningCatcher() { qInstallMessageHandler(previous); }
};
}

TEST_CASE("editing a state message keeps its resets balanced")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto& st = some_state(*doc);
    auto& model = st.messages();

    addMessage(*doc, st, "dev:/a/b", 0.25f);

    auto idx = firstValued(model);
    REQUIRE(idx.isValid());
    auto valueIdx = model.index(idx.row(), (int)MessageItemModel::Column::Value,
                                idx.parent());

    int aboutTo{}, done{}, updated{};
    QObject::connect(
        &model, &QAbstractItemModel::modelAboutToBeReset, &model, [&] { aboutTo++; });
    QObject::connect(&model, &QAbstractItemModel::modelReset, &model, [&] { done++; });
    QObject::connect(
        &st, &Scenario::StateModel::sig_statesUpdated, &st, [&] { updated++; });

    WarningCatcher warnings;
    REQUIRE(model.setData(valueIdx, QVariant::fromValue(0.75), Qt::EditRole));

    // One edit, one reset: a nested pair leaves the views resetting twice for
    // a single change, and the second modelReset arrives after the first has
    // already told everyone the model is settled.
    CHECK(aboutTo == 1);
    CHECK(done == 1);
    CHECK(updated == 1);

    for(const auto& w : WarningCatcher::messages)
      INFO("qWarning: " << w.toStdString());
    CHECK_FALSE(
        WarningCatcher::messages.filter("without calling endResetModel").size() > 0);

    // ... and the edit actually landed.
    const auto& n = model.nodeFromModelIndex(idx);
    REQUIRE(n.hasValue());
    CHECK(ossia::convert<float>(*n.value()) == Catch::Approx(0.75));
  });
}

TEST_CASE("renaming a state message keeps its resets balanced")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto& st = some_state(*doc);
    auto& model = st.messages();

    addMessage(*doc, st, "dev:/a/b", 0.25f);

    auto idx = firstValued(model);
    REQUIRE(idx.isValid());

    int aboutTo{}, done{};
    QObject::connect(
        &model, &QAbstractItemModel::modelAboutToBeReset, &model, [&] { aboutTo++; });
    QObject::connect(&model, &QAbstractItemModel::modelReset, &model, [&] { done++; });

    WarningCatcher warnings;
    REQUIRE(model.setData(idx, QStringLiteral("c"), Qt::EditRole));

    CHECK(aboutTo == 1);
    CHECK(done == 1);
    CHECK_FALSE(
        WarningCatcher::messages.filter("without calling endResetModel").size() > 0);
  });
}

// Dropping a .cues file or a message list on the tree goes through the same
// shape.
TEST_CASE("dropping messages on a state keeps its resets balanced")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto& st = some_state(*doc);
    auto& model = st.messages();

    int aboutTo{}, done{}, updated{};
    QObject::connect(
        &model, &QAbstractItemModel::modelAboutToBeReset, &model, [&] { aboutTo++; });
    QObject::connect(&model, &QAbstractItemModel::modelReset, &model, [&] { done++; });
    QObject::connect(
        &st, &Scenario::StateModel::sig_statesUpdated, &st, [&] { updated++; });

    WarningCatcher warnings;
    addMessage(*doc, st, "dev:/a/b", 0.25f);

    CHECK(aboutTo == 1);
    CHECK(done == 1);
    CHECK(updated == 1);
    CHECK_FALSE(
        WarningCatcher::messages.filter("without calling endResetModel").size() > 0);
  });
}
