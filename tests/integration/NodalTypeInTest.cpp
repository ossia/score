// The right-click type-in box on a control of a real process node, end to end:
// the model's XYSlider inlet, the item DefaultEffectItem builds for it, the box
// the right-click raises over it, and the command the typed value leaves on the
// document's undo stack.
//
// GraphicsTypeInTest covers the box itself against a bare scene. This is the
// chain around it: a value typed into the second field has to reach the inlet
// and cost exactly one undo step, which is what a drag costs.

#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/Process.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>

#include <Control/DefaultEffectItem.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/graphics/RightClickWidget.hpp>
#include <score/graphics/widgets/QGraphicsXYChooser.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QGraphicsProxyWidget>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Keyboard.hpp>
#include <score_test/Mouse.hpp>

#include <algorithm>

namespace
{
//! A process whose only control is a pad, so the node has exactly one to find.
class PadProcess final : public Process::ProcessModel
{
public:
  using Process::ProcessModel::m_inlets;

  PadProcess(const Id<Process::ProcessModel>& id, QObject* parent)
      : Process::ProcessModel{TimeVal::fromMsecs(1000), id, "PadProcess", parent}
  {
    m_inlets.push_back(new Process::XYSlider{
        ossia::vec2f{0., 0.}, ossia::vec2f{1., 1.}, ossia::vec2f{0., 0.}, "Pos",
        Id<Process::Port>{0}, this});
  }

  Process::ControlInlet& pad() const noexcept
  {
    return *static_cast<Process::ControlInlet*>(m_inlets[0]);
  }

  static UuidKey<Process::ProcessModel> static_concreteKey() noexcept
  {
    return UuidKey<Process::ProcessModel>{"1a4f6d90-2f0a-4d0e-9a2b-6f7c1e5d3b84"};
  }
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return static_concreteKey();
  }
  void serialize_impl(const VisitorVariant&) const override { }
  QString prettyShortName() const noexcept override { return "Pad"; }
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

//! Where the PNGs go, as in DeviceExplorerEditorLookTest.
QString shotDir()
{
  auto d = qEnvironmentVariable("SCORE_TEST_SHOT_DIR");
  if(d.isEmpty())
    d = QDir::currentPath() + "/shots";
  QDir{}.mkpath(d);
  return d;
}

score::QGraphicsXYChooser* padItem(QGraphicsItem& root)
{
  for(auto* child : root.childItems())
  {
    if(auto* pad = dynamic_cast<score::QGraphicsXYChooser*>(child))
      return pad;
    if(auto* pad = padItem(*child))
      return pad;
  }
  return nullptr;
}

std::vector<QDoubleSpinBox*> typeInFields()
{
  std::vector<QDoubleSpinBox*> out;
  auto* proxy = score::currentRightClickWidget().data();
  if(!proxy || !proxy->widget())
    return out;

  for(auto* b : proxy->widget()->findChildren<QDoubleSpinBox*>())
    out.push_back(b);
  std::sort(out.begin(), out.end(), [](QDoubleSpinBox* a, QDoubleSpinBox* b) {
    return a->geometry().x() < b->geometry().x();
  });
  return out;
}
}

TEST_CASE(
    "a pad on a node commits one command per field typed into",
    "[integration][nodal][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& itv
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    auto presenter
        = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(
            *doc);
    REQUIRE(presenter != nullptr);

    PadProcess proc{Id<Process::ProcessModel>{999}, &itv};

    QGraphicsScene scene;
    scene.setSceneRect(0, 0, 500, 400);
    auto* root = new QGraphicsRectItem;
    scene.addItem(root);

    QGraphicsView view{&scene};
    view.resize(520, 420);

    auto* node = new Process::DefaultEffectItem{false, proc, presenter->context(), root};
    spin(80);

    auto* pad = padItem(*node);
    REQUIRE(pad != nullptr);

    // Qt sends no focus event outside an active window, and the type-in box
    // lives or dies by the focus.
    if(!score::test::showAndActivate(view))
      SKIP("this platform never activates the window");
    spin(20);

    auto inView
        = [&](QPointF padPos) { return view.mapFromScene(pad->mapToScene(padPos)); };
    auto value = [&] { return ossia::convert<ossia::vec2f>(proc.pad().value()); };

    const auto commandsBefore = doc->commandStack().size();

    score::test::mouseClick(*view.viewport(), inView({50, 50}), Qt::RightButton);
    spin(20);

    auto boxes = typeInFields();
    REQUIRE(boxes.size() == 2);

    // Both fields over the pad, for the eye as well as the assertions.
    view.grab().save(shotDir() + "/xy-pad-type-in.png");

    // The right-click itself is not an edit of the value.
    CHECK(doc->commandStack().size() == commandsBefore);

    auto typeInto = [&](QDoubleSpinBox& b, const QString& text) {
      score::test::keyClick(b, Qt::Key_A, Qt::ControlModifier);
      score::test::keyClicks(b, text);
      spin(20);
    };

    typeInto(*boxes[0], "0.25");

    // The y field is reachable: this used to take the whole box down.
    const QPointF yCentre = score::currentRightClickWidget()->mapToScene(
        QPointF{boxes[1]->geometry().center()});
    score::test::mouseClick(*view.viewport(), view.mapFromScene(yCentre));
    spin(20);
    REQUIRE(score::currentRightClickWidget() != nullptr);

    typeInto(*boxes[1], "0.75");
    score::test::keyClick(*boxes[1], Qt::Key_Return);
    spin(40);

    CHECK(score::currentRightClickWidget() == nullptr);
    CHECK(value()[0] == Catch::Approx(0.25f));
    CHECK(value()[1] == Catch::Approx(0.75f));

    // One command per field, not one per keystroke: the x edit and the y edit.
    CHECK(doc->commandStack().size() == commandsBefore + 2);

    // ... and each undoes on its own.
    doc->commandStack().undo();
    spin(20);
    CHECK(value()[1] == Catch::Approx(0.f));
    CHECK(value()[0] == Catch::Approx(0.25f));

    score::closeRightClickWidget();
  });
}
