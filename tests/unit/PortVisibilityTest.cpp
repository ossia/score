// Unit test: which ports a node shows, and how its controls are paginated.
//
// Nodes with thousands of controls (VST plug-ins in particular) used to lay all
// of them out at once, in both the folded and the unfolded display. These are
// the two rules that keep such a node readable:
//  * folded, a node shows its routing only - everything that is not a control,
//    plus the controls that are cabled or exposed to an address;
//  * unfolded, past MaxUnpaginatedControls the remaining controls are shown
//    ControlsPerPage at a time.

#include <State/Address.hpp>

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/CableData.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortVisibility.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <score/model/path/Path.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Control ports are the ones edited through a widget", "[process][port][nodal]")
{
  QObject owner;

  Process::ControlInlet ctl{"ctl", Id<Process::Port>{0}, &owner};
  Process::ControlOutlet ctlOut{"ctlOut", Id<Process::Port>{1}, &owner};
  Process::FloatSlider slider{0.f, 1.f, 0.5f, "slider", Id<Process::Port>{2}, &owner};
  Process::AudioInlet audio{"audio", Id<Process::Port>{3}, &owner};
  Process::MidiInlet midi{"midi", Id<Process::Port>{4}, &owner};
  Process::ValueInlet value{"value", Id<Process::Port>{5}, &owner};
  Process::AudioOutlet audioOut{"audioOut", Id<Process::Port>{6}, &owner};

  CHECK(Process::isControlPort(ctl));
  CHECK(Process::isControlPort(ctlOut));
  CHECK(Process::isControlPort(slider));
  CHECK(!Process::isControlPort(audio));
  CHECK(!Process::isControlPort(midi));
  CHECK(!Process::isControlPort(value));
  CHECK(!Process::isControlPort(audioOut));

  // Widget inlets are the avendish-style controls; plain ControlInlets, as VST
  // and LV2 use, are not flagged but are controls all the same.
  CHECK(slider.displayHandledExplicitly);
  CHECK(!ctl.displayHandledExplicitly);
}

namespace
{
// Cabling a port for real needs a live document to resolve the path against;
// isVisibleWhenFolded only looks at whether the cable list is empty.
struct CabledControl final : public Process::ControlInlet
{
  using Process::ControlInlet::ControlInlet;
  void attachCable()
  {
    m_cables.push_back(Path<Process::Cable>{
        ObjectPath{{"Cable", 0}}, Path<Process::Cable>::UnsafeDynamicCreation{}});
  }
};
}

TEST_CASE("A folded node keeps the ports the patch is made of", "[process][port][nodal]")
{
  QObject owner;

  Process::AudioInlet audio{"audio", Id<Process::Port>{0}, &owner};
  Process::MidiInlet midi{"midi", Id<Process::Port>{1}, &owner};
  Process::ControlInlet plain{"plain", Id<Process::Port>{2}, &owner};
  Process::ControlInlet addressed{"addressed", Id<Process::Port>{3}, &owner};
  CabledControl cabled{"cabled", Id<Process::Port>{4}, &owner};

  const auto addr = State::parseAddressAccessor("dev:/gain");
  REQUIRE(addr.has_value());
  addressed.setAddress(*addr);

  CHECK(!Process::isVisibleWhenFolded(cabled));
  cabled.attachCable();

  // Non-controls always show up
  CHECK(Process::isVisibleWhenFolded(audio));
  CHECK(Process::isVisibleWhenFolded(midi));

  // A control only shows up when it takes part in the patch
  CHECK(!Process::isVisibleWhenFolded(plain));
  CHECK(Process::isVisibleWhenFolded(addressed));
  CHECK(Process::isVisibleWhenFolded(cabled));

  // Clearing the address hides it again
  addressed.setAddress(State::AddressAccessor{});
  CHECK(!Process::isVisibleWhenFolded(addressed));
}

TEST_CASE("Controls are only paginated when there are too many", "[process][port][nodal]")
{
  using namespace Process;

  SECTION("nothing to show")
  {
    const auto p = nodeControlPage(0, 0);
    CHECK(p.pageCount == 1);
    CHECK(!p.paginated());
    CHECK(p.first == 0);
    CHECK(p.last == 0);
    CHECK(p.count() == 0);
  }

  SECTION("under the threshold: everything on one page")
  {
    for(int n : {1, 5, MaxUnpaginatedControls})
    {
      const auto p = nodeControlPage(n, 0);
      INFO("count: " << n);
      CHECK(p.pageCount == 1);
      CHECK(!p.paginated());
      CHECK(p.first == 0);
      CHECK(p.last == n);
    }
  }

  SECTION("just above the threshold")
  {
    const int n = MaxUnpaginatedControls + 1;
    const auto p0 = nodeControlPage(n, 0);
    CHECK(p0.paginated());
    CHECK(p0.pageCount == 1 + (n - 1) / ControlsPerPage);
    CHECK(p0.first == 0);
    CHECK(p0.last == ControlsPerPage);

    const auto p1 = nodeControlPage(n, 1);
    CHECK(p1.page == 1);
    CHECK(p1.first == ControlsPerPage);
    CHECK(p1.last == n);
  }

  SECTION("a VST-sized bank of controls")
  {
    const int n = 2000;
    const auto p = nodeControlPage(n, 3);
    CHECK(p.pageCount == n / ControlsPerPage);
    CHECK(p.page == 3);
    CHECK(p.first == 3 * ControlsPerPage);
    CHECK(p.last == 4 * ControlsPerPage);
    CHECK(p.count() == ControlsPerPage);
  }

  SECTION("every control is reachable, exactly once")
  {
    const int n = 137;
    int seen = 0;
    int expectedFirst = 0;
    const int pageCount = nodeControlPage(n, 0).pageCount;
    for(int i = 0; i < pageCount; i++)
    {
      const auto p = nodeControlPage(n, i);
      CHECK(p.page == i);
      CHECK(p.first == expectedFirst);
      CHECK(p.last > p.first);
      seen += p.count();
      expectedFirst = p.last;
    }
    CHECK(seen == n);
    CHECK(expectedFirst == n);
  }

  SECTION("a page ends on a full column")
  {
    // The control grid packs ControlsPerColumn items per column, and the
    // leading non-control inlets take the first slots: a page that would leave
    // one item alone in the last column gives it back to the next page.
    const int n = 500;
    for(int offset = 0; offset < 3 * ControlsPerColumn; offset++)
    {
      INFO("grid offset: " << offset);
      const auto p = nodeControlPage(n, 0, offset);
      CHECK(p.count() > 0);
      CHECK(p.count() <= ControlsPerPage);
      CHECK((offset + p.count()) % ControlsPerColumn == 0);
    }

    // The case that prompted this: one midi inlet ahead of the controls
    CHECK(nodeControlPage(n, 0, 1).count() == ControlsPerPage - 1);
    CHECK(nodeControlPage(n, 0, 0).count() == ControlsPerPage);

    // Pages still tile the whole range without gaps or repeats
    const int offset = 2;
    const auto pageCount = nodeControlPage(n, 0, offset).pageCount;
    int expectedFirst = 0;
    for(int i = 0; i < pageCount; i++)
    {
      const auto p = nodeControlPage(n, i, offset);
      CHECK(p.first == expectedFirst);
      expectedFirst = p.last;
    }
    CHECK(expectedFirst == n);
  }

  SECTION("out-of-range pages are clamped")
  {
    const int n = 137;
    const int pageCount = nodeControlPage(n, 0).pageCount;

    const auto before = nodeControlPage(n, -12);
    CHECK(before.page == 0);
    CHECK(before.first == 0);

    const auto after = nodeControlPage(n, 999);
    CHECK(after.page == pageCount - 1);
    CHECK(after.last == n);

    // Clamping a single-page node too
    CHECK(nodeControlPage(3, 7).page == 0);
    CHECK(nodeControlPage(3, 7).last == 3);
  }
}
