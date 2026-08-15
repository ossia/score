// Integration test: how a node with a lot of controls is displayed.
//
// Plug-ins such as VSTs routinely expose hundreds or thousands of parameters.
// Laying all of them out - as a control grid unfolded, as a column of labels
// folded - is both unreadable and very slow, so:
//  * folded, the node shows its routing only: the ports that are not controls,
//    plus the controls that are cabled or exposed to an address;
//  * unfolded, the controls are paginated, with arrows to walk the pages.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <State/Address.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Dataflow/PortVisibility.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/Process.hpp>

#include <Control/DefaultEffectItem.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/graphics/widgets/QGraphicsPixmapButton.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QElapsedTimer>
#include <QPointer>
#include <QGraphicsRectItem>
#include <QGraphicsScene>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

namespace
{
constexpr int ControlCount = 60;

//! A process with far more controls than fits on screen, plus the audio ports
//! that make up the patch around it.
class ManyControls final : public Process::ProcessModel
{
public:
  using Process::ProcessModel::m_inlets;
  using Process::ProcessModel::m_outlets;

  ManyControls(int controls, const Id<Process::ProcessModel>& id, QObject* parent)
      : Process::ProcessModel{TimeVal::fromMsecs(1000), id, "ManyControls", parent}
  {
    m_inlets.push_back(new Process::AudioInlet{"In", Id<Process::Port>{0}, this});
    for(int i = 0; i < controls; i++)
    {
      m_inlets.push_back(new Process::FloatSlider{
          0.f, 1.f, 0.5f, QStringLiteral("Param %1").arg(i), Id<Process::Port>{i + 1},
          this});
    }
    m_outlets.push_back(new Process::AudioOutlet{"Out", Id<Process::Port>{0}, this});
  }

  Process::ControlInlet& control(int i) const noexcept
  {
    return *static_cast<Process::ControlInlet*>(m_inlets[i + 1]);
  }

  static UuidKey<Process::ProcessModel> static_concreteKey() noexcept
  {
    return UuidKey<Process::ProcessModel>{"7c2f0f5e-6b1a-4b17-9d1c-5f2d0a9c4e31"};
  }
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return static_concreteKey();
  }
  void serialize_impl(const VisitorVariant&) const override { }
  QString prettyShortName() const noexcept override { return "Many"; }
  QString category() const noexcept override { return "Test"; }
  QStringList tags() const noexcept override { return {}; }
  Process::ProcessFlags flags() const noexcept override
  {
    return Process::ProcessFlags::SupportsAll;
  }
};

//! DefaultEffectItem rebuilds itself asynchronously: let the loop run.
void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  do
  {
    QApplication::processEvents(QEventLoop::AllEvents, 5);
  } while(t.elapsed() < ms);
}

void collectPorts(QGraphicsItem* item, std::vector<Dataflow::PortItem*>& out)
{
  for(auto child : item->childItems())
  {
    if(child->type() == Dataflow::PortItem::Type)
      out.push_back(static_cast<Dataflow::PortItem*>(child));
    collectPorts(child, out);
  }
}

std::vector<Dataflow::PortItem*> displayedPorts(QGraphicsItem& item)
{
  std::vector<Dataflow::PortItem*> out;
  collectPorts(&item, out);
  return out;
}

int displayedControls(QGraphicsItem& item)
{
  int n = 0;
  for(auto p : displayedPorts(item))
    n += Process::isControlPort(p->port());
  return n;
}

bool shows(QGraphicsItem& item, const Process::Port& port)
{
  for(auto p : displayedPorts(item))
    if(&p->port() == &port)
      return true;
  return false;
}

//! Previous / next page, left to right.
std::vector<score::QGraphicsPixmapButton*> pagerButtons(QGraphicsItem* item)
{
  std::vector<score::QGraphicsPixmapButton*> out;
  for(auto child : item->childItems())
  {
    if(auto b = dynamic_cast<score::QGraphicsPixmapButton*>(child))
      out.push_back(b);
    for(auto sub : pagerButtons(child))
      out.push_back(sub);
  }
  std::sort(out.begin(), out.end(), [](auto* a, auto* b) {
    return a->scenePos().x() < b->scenePos().x();
  });
  return out;
}

//! ManyControls declares one leading audio inlet, and it sits in the control
//! grid: the pages are shortened by one so they end on a full column.
constexpr int GridOffset = 1;

Process::ControlPage pageOf(int controlCount, int page)
{
  return Process::nodeControlPage(controlCount, page, GridOffset);
}

//! Runs fn with a live document, a scene, and a process with `controls` controls.
template <typename F>
void withControls(int controls, F&& fn)
{
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& itv
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();

    auto presenter
        = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(
            *doc);
    REQUIRE(presenter != nullptr);

    ManyControls proc{controls, Id<Process::ProcessModel>{999}, &itv};

    QGraphicsScene scene;
    auto root = new QGraphicsRectItem;
    scene.addItem(root);

    fn(proc, presenter->context(), root);

    spin(20);
  });
}
}

TEST_CASE("An unfolded node paginates its controls", "[integration][nodal][gui]")
{
  withControls(
      ControlCount,
      [](ManyControls& proc, const Process::Context& pctx, QGraphicsItem* root) {
    auto item = new Process::DefaultEffectItem{false, proc, pctx, root};
    spin(80);

    const auto page0 = pageOf(ControlCount, 0);

    // One page of controls, not the 60 of them
    REQUIRE(ControlCount > Process::MaxUnpaginatedControls);
    CHECK(displayedControls(*item) == page0.count());

    // ... and that page fills its columns
    CHECK((GridOffset + displayedControls(*item)) % Process::ControlsPerColumn == 0);

    // The audio ports are still there: they are what the patch is made of
    CHECK(shows(*item, *proc.m_inlets.front()));
    CHECK(shows(*item, *proc.m_outlets.front()));

    // First page: the head of the list
    CHECK(shows(*item, proc.control(0)));
    CHECK(!shows(*item, proc.control(page0.last)));

    auto buttons = pagerButtons(item);
    REQUIRE(buttons.size() == 2);

    SECTION("the arrows walk the pages")
    {
      buttons.back()->clicked();
      spin(80);
      const auto page1 = pageOf(ControlCount, 1);
      CHECK(displayedControls(*item) == page1.count());
      CHECK(!shows(*item, proc.control(0)));
      CHECK(shows(*item, proc.control(page1.first)));

      // Last page holds the tail of the list, and the node does not go past it
      for(int i = 0; i < page0.pageCount; i++)
      {
        pagerButtons(item).back()->clicked();
        spin(80);
      }
      CHECK(shows(*item, proc.control(ControlCount - 1)));

      // Back to the first page, and no further
      for(int i = 0; i < page0.pageCount + 1; i++)
      {
        pagerButtons(item).front()->clicked();
        spin(80);
      }
      CHECK(shows(*item, proc.control(0)));
    }

    delete item;
      });
}

TEST_CASE(
    "Paging follows the declaration order, not the wiring",
    "[integration][nodal][gui]")
{
  // Controls keep the order the process declares them in, whether or not they
  // are patched: a cable landing on a control that is not on the current page
  // is simply not drawn, as with the tabbed layouts.
  withControls(
      ControlCount,
      [](ManyControls& proc, const Process::Context& pctx, QGraphicsItem* root) {
    const auto addr = State::parseAddressAccessor("dev:/param");
    REQUIRE(addr.has_value());
    proc.control(50).setAddress(*addr);

    auto item = new Process::DefaultEffectItem{false, proc, pctx, root};
    spin(80);

    // Page 1 is the head of the list, and nothing else
    const auto page0 = pageOf(ControlCount, 0);
    CHECK(displayedControls(*item) == page0.count());
    for(int i = page0.first; i < page0.last; i++)
      CHECK(shows(*item, proc.control(i)));
    CHECK(!shows(*item, proc.control(50)));

    // The patched one shows up on its own page, in place
    pagerButtons(item).back()->clicked();
    spin(80);
    pagerButtons(item).back()->clicked();
    spin(80);

    const auto page2 = pageOf(ControlCount, 2);
    REQUIRE(page2.first <= 50);
    REQUIRE(page2.last > 50);
    CHECK(shows(*item, proc.control(50)));
    CHECK(shows(*item, proc.control(page2.first)));
    CHECK(!shows(*item, proc.control(0)));

    delete item;
      });
}

TEST_CASE("A folded node shows its routing only", "[integration][nodal][gui]")
{
  withControls(
      ControlCount,
      [](ManyControls& proc, const Process::Context& pctx, QGraphicsItem* root) {
    auto item = new Process::DefaultEffectItem{true, proc, pctx, root};
    spin(80);

    // The audio ports, and none of the controls
    CHECK(displayedControls(*item) == 0);
    CHECK(shows(*item, *proc.m_inlets.front()));
    CHECK(shows(*item, *proc.m_outlets.front()));

    // Exposing a control to an address brings it back: the patch refers to it,
    // so hiding it would hide part of the routing.
    const auto addr = State::parseAddressAccessor("dev:/param");
    REQUIRE(addr.has_value());
    proc.control(7).setAddress(*addr);
    spin(80);

    CHECK(displayedControls(*item) == 1);
    CHECK(shows(*item, proc.control(7)));

    // Clearing it hides the control again
    proc.control(7).setAddress(State::AddressAccessor{});
    spin(80);
    CHECK(displayedControls(*item) == 0);

    delete item;
      });
}

TEST_CASE(
    "Wiring a control does not delete the port items under the caller",
    "[integration][nodal][gui]")
{
  // Port::cablesChanged / addressChanged fire from inside the drop that creates
  // the cable, while the event is still being delivered to the port item. A
  // node that rebuilt itself right there deleted that item, and the drop
  // handler (Dataflow::DragMoveFilter) then walked into freed memory.
  withControls(
      ControlCount,
      [](ManyControls& proc, const Process::Context& pctx, QGraphicsItem* root) {
    auto item = new Process::DefaultEffectItem{true, proc, pctx, root};
    spin(80);

    const auto addr = State::parseAddressAccessor("dev:/param");
    REQUIRE(addr.has_value());
    proc.control(7).setAddress(*addr);
    spin(80);
    REQUIRE(shows(*item, proc.control(7)));

    Dataflow::PortItem* shownPort{};
    for(auto p : displayedPorts(*item))
      if(&p->port() == &proc.control(7))
        shownPort = p;
    REQUIRE(shownPort != nullptr);
    QPointer<Dataflow::PortItem> alive{shownPort};

    // Hiding the control again changes what is displayed, but not before the
    // signal has finished being delivered.
    proc.control(7).setAddress(State::AddressAccessor{});
    CHECK(alive);

    spin(80);
    CHECK(!alive);
    CHECK(!shows(*item, proc.control(7)));

    delete item;
      });
}

TEST_CASE("A node with few controls shows all of them", "[integration][nodal][gui]")
{
  withControls(5, [](ManyControls& proc, const Process::Context& pctx, QGraphicsItem* root) {
    auto item = new Process::DefaultEffectItem{false, proc, pctx, root};
    spin(80);

    CHECK(displayedControls(*item) == 5);
    for(int i = 0; i < 5; i++)
      CHECK(shows(*item, proc.control(i)));

    // No pager on a node that does not need one
    CHECK(pagerButtons(item).empty());

    delete item;
  });
}
