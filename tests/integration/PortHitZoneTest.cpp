// Integration test: what a click near a port hits.
//
// A port is a small circle drawn next to its control (slider, knob...). Its
// clickable zone must stay a circle around what is drawn: with a 20x20 square
// hit zone, a press on the left end of a slider grabbed the port and started a
// cable drag instead of moving the slider. Likewise the cable, the drag line
// and the magnetic snapping must all anchor on the center of that circle.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/Process.hpp>

#include <Control/DefaultEffectItem.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/graphics/widgets/QGraphicsKnob.hpp>
#include <score/graphics/widgets/QGraphicsSlider.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QElapsedTimer>
#include <QGraphicsRectItem>
#include <QGraphicsScene>

#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace
{
class SliderAndKnob final : public Process::ProcessModel
{
public:
  using Process::ProcessModel::m_inlets;
  using Process::ProcessModel::m_outlets;

  SliderAndKnob(const Id<Process::ProcessModel>& id, QObject* parent)
      : Process::ProcessModel{TimeVal::fromMsecs(1000), id, "SliderAndKnob", parent}
  {
    m_inlets.push_back(new Process::FloatSlider{
        0.f, 1.f, 0.5f, QStringLiteral("Freq."), Id<Process::Port>{0}, this});
    m_inlets.push_back(new Process::FloatKnob{
        0.f, 1.f, 0.5f, QStringLiteral("Gain"), Id<Process::Port>{1}, this});
    m_outlets.push_back(new Process::ValueOutlet{QStringLiteral("Out"), Id<Process::Port>{0}, this});
  }

  static UuidKey<Process::ProcessModel> static_concreteKey() noexcept
  {
    return UuidKey<Process::ProcessModel>{"5b2d3e1a-2c4f-4c7e-9b6a-1f0e2d3c4b5a"};
  }
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return static_concreteKey();
  }
  void serialize_impl(const VisitorVariant&) const override { }
  QString prettyShortName() const noexcept override { return "SliderAndKnob"; }
  QString category() const noexcept override { return "Test"; }
  QStringList tags() const noexcept override { return {}; }
  Process::ProcessFlags flags() const noexcept override
  {
    return Process::ProcessFlags::SupportsAll;
  }
};

void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  do
  {
    QApplication::processEvents(QEventLoop::AllEvents, 5);
  } while(t.elapsed() < ms);
}

template <typename T>
void collect(QGraphicsItem* item, std::vector<T*>& out)
{
  for(auto child : item->childItems())
  {
    if(auto t = dynamic_cast<T*>(child))
      out.push_back(t);
    collect(child, out);
  }
}

Dataflow::PortItem* portFor(QGraphicsItem& root, const Process::Port& port)
{
  std::vector<Dataflow::PortItem*> ports;
  collect(&root, ports);
  for(auto p : ports)
    if(&p->port() == &port)
      return p;
  return nullptr;
}

template <typename F>
void withProcess(F&& fn)
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

    SliderAndKnob proc{Id<Process::ProcessModel>{998}, &itv};

    QGraphicsScene scene;
    auto root = new QGraphicsRectItem;
    scene.addItem(root);

    auto item = new Process::DefaultEffectItem{false, proc, presenter->context(), root};
    spin(80);

    fn(proc, scene, *item);

    delete item;
    spin(20);
  });
}

//! The topmost item at a scene position, the way a mouse press finds it.
QGraphicsItem* hit(QGraphicsScene& scene, QPointF scenePos)
{
  return scene.itemAt(scenePos, QTransform{});
}
}

TEST_CASE("The port hit zone is a circle around the drawn port", "[integration][dataflow][gui]")
{
  withProcess([](SliderAndKnob& proc, QGraphicsScene& scene, QGraphicsItem& item) {
    auto port = portFor(item, *proc.m_inlets[0]);
    REQUIRE(port);

    const QPointF c = Dataflow::PortItem::portCenter();
    const double r = Dataflow::PortItem::hitRadius;

    // Inside: the center and the four cardinal points just within the radius
    CHECK(port->contains(c));
    CHECK(port->contains(c + QPointF{r - 0.5, 0}));
    CHECK(port->contains(c - QPointF{r - 0.5, 0}));
    CHECK(port->contains(c + QPointF{0, r - 0.5}));
    CHECK(port->contains(c - QPointF{0, r - 0.5}));

    // Outside: just past the radius, and the corners of the old 20x20 square
    CHECK(!port->contains(c + QPointF{r + 0.5, 0}));
    CHECK(!port->contains(c - QPointF{r + 0.5, 0}));
    CHECK(!port->contains(c + QPointF{r + 0.5, r + 0.5}));
    CHECK(!port->contains(QPointF{-3, -3}));
    CHECK(!port->contains(QPointF{16, 16}));
    CHECK(!port->contains(QPointF{16, 6}));

    // shape() agrees with contains(), and is what the scene uses
    CHECK(port->shape().contains(c));
    CHECK(!port->shape().contains(QPointF{16, 6}));
    CHECK(port->shape().boundingRect().width() <= 2 * r + 0.01);

    // The hit zone is small but still a comfortable target
    CHECK(r >= 5.);
    CHECK(r <= 6.5);
  });
}

TEST_CASE("Clicking a slider next to its port moves the slider, not the port", "[integration][dataflow][gui]")
{
  withProcess([](SliderAndKnob& proc, QGraphicsScene& scene, QGraphicsItem& item) {
    auto port = portFor(item, *proc.m_inlets[0]);
    REQUIRE(port);

    std::vector<score::QGraphicsSlider*> sliders;
    collect(&item, sliders);
    REQUIRE(sliders.size() == 1);
    auto slider = sliders.front();

    const QRectF sliderRect = slider->mapRectToScene(slider->boundingRect());
    const QPointF portCenter = port->sceneCenter();

    // The layout puts the port at the left of the slider, vertically centered
    REQUIRE(portCenter.x() < sliderRect.left());
    REQUIRE(sliderRect.top() < portCenter.y());
    REQUIRE(portCenter.y() < sliderRect.bottom());

    // A press at the center of the port hits the port
    CHECK(hit(scene, portCenter) == port);

    // A press in the first pixels of the slider, at the port's height, hits the
    // slider: this is exactly where the 20x20 square hit zone used to win.
    for(double dx : {0.5, 2., 4., 8.})
    {
      const QPointF p{sliderRect.left() + dx, portCenter.y()};
      INFO("dx = " << dx);
      CHECK(hit(scene, p) == slider);
      CHECK(!port->contains(port->mapFromScene(p)));
    }

    // Below the port, inside the slider's vertical extent but left of it:
    // no control there, and the port must not claim it either.
    const QPointF below{portCenter.x(), sliderRect.bottom() - 1.};
    CHECK(hit(scene, below) != port);
  });
}

TEST_CASE("Clicking a knob next to its port hits the knob", "[integration][dataflow][gui]")
{
  withProcess([](SliderAndKnob& proc, QGraphicsScene& scene, QGraphicsItem& item) {
    auto port = portFor(item, *proc.m_inlets[1]);
    REQUIRE(port);

    std::vector<score::QGraphicsKnob*> knobs;
    collect(&item, knobs);
    REQUIRE(knobs.size() == 1);
    auto knob = knobs.front();

    const QRectF knobRect = knob->mapRectToScene(knob->boundingRect());
    const QPointF portCenter = port->sceneCenter();
    REQUIRE(portCenter.x() < knobRect.center().x());

    CHECK(hit(scene, portCenter) == port);
    // 4px into the knob at the port's height: the knob
    const QPointF p{knobRect.left() + 4., portCenter.y()};
    CHECK(hit(scene, p) != port);
  });
}

TEST_CASE("Cables anchor on the center of the port circle", "[integration][dataflow][gui]")
{
  withProcess([](SliderAndKnob& proc, QGraphicsScene& scene, QGraphicsItem& item) {
    auto port = portFor(item, *proc.m_inlets[0]);
    REQUIRE(port);

    // Unscaled: the center of the drawn circle (6, 6) in item coordinates
    CHECK(port->sceneCenter() == port->mapToScene(Dataflow::PortItem::portCenter()));
    CHECK(port->sceneCenter() == port->scenePos() + QPointF{6., 6.});

    // In a zoomed nodal view the item is scaled: the anchor follows the
    // transform rather than adding a fixed 6px offset.
    auto top = item.topLevelItem();
    top->setScale(2.);
    CHECK(port->sceneCenter() == port->mapToScene(Dataflow::PortItem::portCenter()));
    CHECK(std::abs((port->sceneCenter() - port->scenePos()).x() - 12.) < 1e-6);
    CHECK(std::abs((port->sceneCenter() - port->scenePos()).y() - 12.) < 1e-6);
    top->setScale(1.);
  });
}
