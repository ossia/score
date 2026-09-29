// The selection stack keeps an empty selection at its bottom, whatever
// happens to the others: capped history, pruned objects. Losing it makes the
// oldest selection come back as the current one when an object is deleted.

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
}

TEST_CASE("selection stack: the capped history keeps the empty bottom", "[selection]")
{
  score::SelectionStack stack;
  std::vector<std::unique_ptr<Obj>> objs;
  for(int i = 0; i < 60; i++)
  {
    objs.push_back(std::make_unique<Obj>(i));
    stack.pushNewSelection(Selection{objs.back().get()});
  }
  CHECK(stack.currentSelection() == Selection{objs.back().get()});

  // Back to the bottom
  while(stack.canUnselect())
    stack.unselect();
  CHECK(stack.currentSelection().empty());

  // Deleting an object prunes the stack: nothing gets selected
  objs.back().reset();
  CHECK(stack.currentSelection().empty());
  CHECK(!stack.canUnselect());
}

TEST_CASE("selection stack: pruning an object keeps the empty bottom", "[selection]")
{
  score::SelectionStack stack;
  auto a = std::make_unique<Obj>(1);
  auto b = std::make_unique<Obj>(2);
  stack.pushNewSelection(Selection{a.get()});
  stack.pushNewSelection(Selection{b.get()});

  a.reset();
  CHECK(stack.currentSelection() == Selection{b.get()});
  REQUIRE(stack.canUnselect());
  stack.unselect();
  CHECK(stack.currentSelection().empty());

  b.reset();
  CHECK(stack.currentSelection().empty());
  CHECK(stack.canReselect() == false);
}
