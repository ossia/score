// Anchors of pasted states move to the copies of the objects they pointed
// into, the first copied object listed winning; the others are kept only when
// pasting in the same document.

#include <State/Address.hpp>
#include <State/Message.hpp>

#include <Process/State/MessageNode.hpp>

#include <Scenario/Commands/Scenario/PasteAnchors.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Scenario/Document/State/StateModel.hpp>

#include <score/model/path/ObjectPath.hpp>

#include <core/document/Document.hpp>

#include <catch2/catch_test_macros.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Process.hpp>

namespace
{
ObjectPath path(std::initializer_list<std::pair<const char*, int>> elts)
{
  ObjectIdentifierVector v;
  for(auto& [name, id] : elts)
    v.emplace_back(QString::fromUtf8(name), id);
  return ObjectPath{std::move(v)};
}

State::Message anchored(int i, ObjectPath target)
{
  State::Message m;
  m.address.address = State::Address{"score", {"a", QString::number(i)}};
  m.address.address.anchor = std::make_shared<const State::Anchor>(
      State::Anchor{std::move(target), QStringLiteral("value")});
  m.value = float(i);
  return m;
}

//! The anchor target of each message, in the order they were added
std::vector<std::optional<ObjectPath>> targets(const Scenario::StateModel& st, int count)
{
  std::vector<std::optional<ObjectPath>> res(count);
  for(auto& m : Process::flatten(st.messages().rootNode()))
  {
    const int i = m.address.address.path.back().toInt();
    if(m.address.address.anchor)
      res[i] = m.address.address.anchor->target;
  }
  return res;
}
}

TEST_CASE(
    "anchors of pasted states follow the copies of what they point into",
    "[integration][paste][anchors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    auto& st = score::test::some_state(*doc);

    // Many copied states, and their whole scenario listed last
    constexpr int copies = 2000;
    Scenario::CopiedPaths paths;
    for(int i = 0; i < copies; i++)
      paths.moved.emplace_back(
          path({{"Scenario", 1}, {"State", i}}),
          path({{"Scenario", 2}, {"State", copies + i}}));
    paths.moved.emplace_back(path({{"Scenario", 1}}), path({{"Scenario", 3}}));

    State::MessageList msgs;
    for(int i = 0; i < copies; i++)
      msgs.push_back(anchored(i, path({{"Scenario", 1}, {"State", i}, {"Port", 7}})));
    // Only in the whole scenario
    msgs.push_back(anchored(copies, path({{"Scenario", 1}, {"Interval", 9}})));
    // In nothing copied
    msgs.push_back(anchored(copies + 1, path({{"Other", 1}})));
    // A prefix of a copied path is not in it
    msgs.push_back(anchored(copies + 2, path({{"Scenario", 4}})));
    const int count = copies + 3;

    auto tree = st.messages().rootNode();
    Scenario::updateTreeWithMessageList(tree, msgs);
    st.messages() = std::move(tree);

    SECTION("in the same document")
    {
      paths.sameDocument = true;
      Scenario::remapCopiedAnchors(st, paths);
      const auto t = targets(st, count);
      for(int i = 0; i < copies; i++)
      {
        REQUIRE(t[i]);
        CHECK(*t[i] == path({{"Scenario", 2}, {"State", copies + i}, {"Port", 7}}));
      }
      REQUIRE(t[copies]);
      CHECK(*t[copies] == path({{"Scenario", 3}, {"Interval", 9}}));
      REQUIRE(t[copies + 1]);
      CHECK(*t[copies + 1] == path({{"Other", 1}}));
      REQUIRE(t[copies + 2]);
      CHECK(*t[copies + 2] == path({{"Scenario", 4}}));
    }

    SECTION("in another document")
    {
      paths.sameDocument = false;
      Scenario::remapCopiedAnchors(st, paths);
      const auto t = targets(st, count);
      REQUIRE(t[0]);
      CHECK(*t[0] == path({{"Scenario", 2}, {"State", copies}, {"Port", 7}}));
      REQUIRE(t[copies]);
      CHECK(*t[copies] == path({{"Scenario", 3}, {"Interval", 9}}));
      CHECK(!t[copies + 1]);
      CHECK(!t[copies + 2]);
    }
  });
}

TEST_CASE(
    "a state without anchors is left as it is by a paste",
    "[integration][paste][anchors]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    auto& st = score::test::some_state(*doc);

    State::MessageList msgs;
    State::Message m;
    m.address.address = State::Address{"dev", {"x"}};
    m.value = 1.f;
    msgs.push_back(m);
    auto tree = st.messages().rootNode();
    Scenario::updateTreeWithMessageList(tree, msgs);
    st.messages() = std::move(tree);

    int resets = 0;
    QObject::connect(
        &st.messages(), &QAbstractItemModel::modelReset, &st, [&] { resets++; });
    Scenario::CopiedPaths paths;
    paths.moved.emplace_back(path({{"Scenario", 1}}), path({{"Scenario", 2}}));
    Scenario::remapCopiedAnchors(st, paths);
    CHECK(resets == 0);
    CHECK(Process::flatten(st.messages().rootNode()).size() == 1);
  });
}
