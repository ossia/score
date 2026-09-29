// SelectionStack::Batch: objects destroyed while a batch is open are swept
// from the stack once, when the outermost batch ends, with one
// currentSelectionChanged. The resulting stack is the one the per-object
// prune gives.

#include <score/model/IdentifiedObject.hpp>
#include <score/selection/SelectionStack.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

namespace
{
struct Obj final : IdentifiedObject<Obj>
{
  explicit Obj(int id)
      : IdentifiedObject<Obj>{Id<Obj>{id}, QStringLiteral("Obj"), nullptr}
  {
  }
};

struct History
{
  std::vector<Selection> unselectable;
  std::vector<Selection> reselectable;
  Selection current;
  bool operator==(const History&) const = default;
};

// Walks the whole stack, then puts it back where it was.
History history(score::SelectionStack& s)
{
  History h;
  h.current = s.currentSelection();
  int down = 0;
  while(s.canUnselect())
  {
    s.unselect();
    h.unselectable.push_back(s.currentSelection());
    down++;
  }
  for(int i = 0; i < down; i++)
    s.reselect();
  int up = 0;
  while(s.canReselect())
  {
    s.reselect();
    h.reselectable.push_back(s.currentSelection());
    up++;
  }
  for(int i = 0; i < up; i++)
    s.unselect();
  REQUIRE(s.currentSelection() == h.current);
  return h;
}

struct Counter
{
  int count{};
  explicit Counter(score::SelectionStack& s)
  {
    QObject::connect(
        &s, &score::SelectionStack::currentSelectionChanged, &s, [this] { count++; });
  }
};

// Two stacks with the same history over the same objects, some of which
// appear in several selections, in the undo and in the redo part.
struct Fixture
{
  std::vector<std::unique_ptr<Obj>> objs;
  score::SelectionStack plain;
  score::SelectionStack batched;

  Fixture()
  {
    for(int i = 0; i < 10; i++)
      objs.push_back(std::make_unique<Obj>(i));
    auto sel = [&](std::initializer_list<int> ids) {
      Selection s;
      for(int i : ids)
        s.append(objs[i].get());
      return s;
    };
    for(auto* stack : {&plain, &batched})
    {
      stack->pushNewSelection(sel({0, 1}));
      stack->pushNewSelection(sel({2}));
      stack->pushNewSelection(sel({3, 4, 5}));
      stack->pushNewSelection(sel({1, 6}));
      stack->pushNewSelection(sel({7, 8}));
      stack->pushNewSelection(sel({4, 9}));
      stack->unselect();
      stack->unselect();
    }
  }
};
}

TEST_CASE(
    "selection stack batch: same stack as pruning each object, one change signal",
    "[selection]")
{
  Fixture f;
  Counter plainChanges{f.plain};
  Counter batchedChanges{f.batched};

  // {1, 6} is current, {7, 8} and {4, 9} can be reselected.
  // {2}, {1, 6} and {7, 8} lose all their objects.
  const std::vector<int> destroyed{1, 2, 6, 7, 8, 4};
  {
    score::SelectionStack::Batch b{f.batched};
    for(int i : destroyed)
      f.objs[i].reset();
    CHECK(batchedChanges.count == 0);
  }

  CHECK(plainChanges.count == int(destroyed.size()));
  CHECK(batchedChanges.count == 1);

  const auto expected = history(f.plain);
  CHECK(history(f.batched) == expected);

  // {3, 5}: the topmost selection that keeps an object.
  CHECK(expected.current == Selection{f.objs[3].get(), f.objs[5].get()});
  CHECK(expected.reselectable == std::vector<Selection>{Selection{f.objs[9].get()}});
}

TEST_CASE("selection stack batch: nested batches sweep once, at the end", "[selection]")
{
  Fixture f;
  Counter changes{f.batched};
  {
    score::SelectionStack::Batch outer{f.batched};
    {
      score::SelectionStack::Batch inner{f.batched};
      f.objs[6].reset();
    }
    CHECK(changes.count == 0);
    f.objs[1].reset();
    CHECK(changes.count == 0);
  }
  CHECK(changes.count == 1);
  CHECK(f.batched.currentSelection() == Selection{f.objs[3].get(), f.objs[4].get(), f.objs[5].get()});

  // A batch in which nothing selected is destroyed changes nothing.
  {
    score::SelectionStack::Batch b{f.batched};
  }
  CHECK(changes.count == 1);
}

TEST_CASE(
    "selection stack batch: inside a batch, the stack reads and changes as if swept",
    "[selection]")
{
  Fixture f;
  Counter plainChanges{f.plain};
  Counter batchedChanges{f.batched};

  score::SelectionStack::Batch b{f.batched};
  f.objs[1].reset();
  f.objs[6].reset();

  // The destroyed objects are not in the current selection, and a selection
  // they emptied is not current any more.
  CHECK(f.plain.currentSelection() == Selection{f.objs[3].get(), f.objs[4].get(), f.objs[5].get()});
  CHECK(f.batched.currentSelection() == f.plain.currentSelection());

  // A change of selection sweeps first, as the per-object prune had done:
  // pushing the current selection again is not a new entry.
  f.plain.pushNewSelection(f.plain.currentSelection());
  f.batched.pushNewSelection(f.batched.currentSelection());
  CHECK(batchedChanges.count == 1);
  CHECK(plainChanges.count == 2);

  f.plain.pushNewSelection(Selection{f.objs[0].get()});
  f.batched.pushNewSelection(Selection{f.objs[0].get()});
  f.objs[0].reset();
  CHECK(f.batched.currentSelection() == f.plain.currentSelection());
  f.plain.unselect();
  f.batched.unselect();
  CHECK(history(f.batched) == history(f.plain));
}

TEST_CASE(
    "selection stack batch: outside a batch, each destroyed object is pruned at once",
    "[selection]")
{
  Fixture f;
  Counter changes{f.batched};
  f.objs[6].reset();
  CHECK(changes.count == 1);
  CHECK(f.batched.currentSelection() == Selection{f.objs[1].get()});
  f.objs[1].reset();
  CHECK(changes.count == 2);
  CHECK(f.batched.currentSelection() == Selection{f.objs[3].get(), f.objs[4].get(), f.objs[5].get()});
  CHECK(history(f.batched) == history(f.plain));
}

TEST_CASE(
    "selection stack batch: an object reusing a destroyed one's address stays selected",
    "[selection]")
{
  score::SelectionStack s;
  auto a = std::make_unique<Obj>(0);
  s.pushNewSelection(Selection{a.get()});

  std::vector<std::unique_ptr<Obj>> others;
  std::unique_ptr<Obj> b;
  {
    score::SelectionStack::Batch batch{s};
    const void* addr = a.get();
    a.reset();
    for(int i = 1; i < 1000 && !b; i++)
    {
      auto o = std::make_unique<Obj>(i);
      if(o.get() == addr)
        b = std::move(o);
      else
        others.push_back(std::move(o));
    }
    if(!b)
      SKIP("the allocator did not reuse the address");

    s.pushNewSelection(Selection{b.get()});
    CHECK(s.currentSelection() == Selection{b.get()});
  }
  CHECK(s.currentSelection() == Selection{b.get()});

  // The new object's destruction is tracked, even though the old one's
  // connection was keyed by the same address.
  Counter changes{s};
  b.reset();
  CHECK(changes.count == 1);
  CHECK(s.currentSelection().empty());
  CHECK(!s.canUnselect());
}
