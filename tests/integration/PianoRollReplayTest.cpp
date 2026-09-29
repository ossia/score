// Piano roll on many notes: selecting them all makes one document selection;
// a transpose that grows the range and a resize clamped at the minimum
// duration undo exactly, and replay exactly from their saved form as a crash
// restore does.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Process/DocumentPlugin.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ProcessList.hpp>

#include <Midi/MidiProcess.hpp>

#include <score/command/Command.hpp>
#include <score/selection/SelectionStack.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <QApplication>
#include <QCoreApplication>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QKeyEvent>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

namespace
{
struct PianoRoll
{
  score::Document& doc;
  Midi::ProcessModel& proc;
  Process::DataflowManager dfm;
  FocusDispatcher fd;
  Process::Context pctx{doc.context(), dfm, fd};

  QGraphicsScene scene;
  QGraphicsView gv{&scene};
  QGraphicsRectItem root{QRectF{0., 0., 1000., 1000.}};
  QGraphicsRectItem focusThief{QRectF{0., 0., 10., 10.}};
  Process::LayerView* view{};
  Process::LayerPresenter* presenter{};

  PianoRoll(score::Document& d, Midi::ProcessModel& p)
      : doc{d}
      , proc{p}
  {
    scene.addItem(&root);
    focusThief.setFlag(QGraphicsItem::ItemIsFocusable);
    scene.addItem(&focusThief);
    auto& layers = doc.context().app.interfaces<Process::LayerFactoryList>();
    auto* fact = layers.findDefaultFactory(proc);
    REQUIRE(fact);
    view = fact->makeLayerView(proc, pctx, &root);
    REQUIRE(view);
    presenter = fact->makeLayerPresenter(proc, view, pctx, nullptr);
    REQUIRE(presenter);
    presenter->setHeight(600.);
    presenter->setWidth(1000., 1000.);
    presenter->setFocus(true);
  }

  ~PianoRoll()
  {
    delete presenter;
    delete view;
    scene.removeItem(&root);
    scene.removeItem(&focusThief);
  }

  std::vector<QGraphicsItem*> notes() const
  {
    std::vector<QGraphicsItem*> res;
    for(auto* it : view->childItems())
      if(it->flags() & QGraphicsItem::ItemIsSelectable)
        res.push_back(it);
    return res;
  }

  void mouse(QGraphicsItem* item, QEvent::Type t, QPointF pos, QPointF down)
  {
    QGraphicsSceneMouseEvent ev{t};
    ev.setButton(Qt::LeftButton);
    ev.setButtons(t == QEvent::GraphicsSceneMouseRelease ? Qt::NoButton : Qt::LeftButton);
    ev.setPos(pos);
    ev.setScenePos(item->mapToScene(pos));
    ev.setButtonDownPos(Qt::LeftButton, down);
    ev.setButtonDownScenePos(Qt::LeftButton, item->mapToScene(down));
    scene.sendEvent(item, &ev);
  }

  void resize(QGraphicsItem* note, double dx)
  {
    const QPointF edge{note->boundingRect().width() - 1., 1.};
    mouse(note, QEvent::GraphicsSceneMousePress, edge, edge);
    mouse(note, QEvent::GraphicsSceneMouseMove, edge + QPointF{dx, 0.}, edge);
    mouse(note, QEvent::GraphicsSceneMouseRelease, edge + QPointF{dx, 0.}, edge);
  }

  void key(int k, Qt::KeyboardModifiers mods = Qt::NoModifier)
  {
    scene.setFocusItem(&focusThief);
    QKeyEvent ev{QEvent::KeyPress, k, mods};
    QApplication::sendEvent(&gv, &ev);
  }
};

Midi::ProcessModel* newPianoRoll(score::Document& doc)
{
  auto* p = dynamic_cast<Midi::ProcessModel*>(score::test::add_process(
      doc, QStringLiteral("c189e507-3afa-4a53-9369-e38860e6bb2d"), {}));
  REQUIRE(p);
  return p;
}

struct NoteState
{
  int pitch;
  double start;
  double duration;
  bool operator==(const NoteState&) const = default;
};

std::vector<NoteState> snapshot(const Midi::ProcessModel& p)
{
  std::vector<NoteState> res;
  for(auto& n : p.notes)
    res.push_back({n.pitch(), n.start(), n.duration()});
  return res;
}

//! Replaces the last command by a copy made from its saved form, as a crash
//! restore does.
void replayLast(score::Document& doc)
{
  auto& stack = doc.commandStack();
  REQUIRE(stack.canUndo());
  const score::CommandData data{*stack.command(stack.currentIndex() - 1)};
  stack.undo();
  std::unique_ptr<score::Command> copy{
      doc.context().app.components.instantiateUndoCommand(data)};
  REQUIRE(copy);
  copy->redo(doc.context());
  stack.push(copy.release());
}
}

TEST_CASE(
    "piano roll: a transpose of many notes growing the range replays exactly",
    "[midi][pianoroll][replay]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* p = newPianoRoll(*doc);
    p->setRange(40, 80);

    constexpr int count = 2000;
    for(int i = 0; i < count; i++)
    {
      Midi::NoteData n;
      n.m_start = double(i) / count;
      n.m_duration = 0.5 / count;
      n.m_pitch = 48 + i % 25;
      n.m_velocity = 100;
      p->notes.add(new Midi::Note{Id<Midi::Note>{i}, n, p});
    }
    const auto before = snapshot(*p);

    PianoRoll roll{*doc, *p};
    auto notes = roll.notes();
    REQUIRE(notes.size() == std::size_t(count));
    // Selecting every note makes one document selection holding them all
    auto& selection = doc->context().selectionStack;
    int pushes = 0;
    QObject::connect(
        &selection, &score::SelectionStack::currentSelectionChanged, &selection,
        [&] { pushes++; });
    for(auto* n : notes)
      n->setSelected(true);
    QCoreApplication::processEvents();
    CHECK(pushes == 1);
    CHECK(selection.currentSelection().size() == count);

    // 48..72 an octave up: past 80, the range grows with the notes
    roll.key(Qt::Key_Up, Qt::ShiftModifier);
    auto after = before;
    for(auto& n : after)
      n.pitch += 12;
    CHECK(snapshot(*p) == after);
    CHECK(p->range() == std::pair{40, 84});

    replayLast(*doc);
    CHECK(snapshot(*p) == after);
    CHECK(p->range() == std::pair{40, 84});

    doc->commandStack().undo();
    CHECK(snapshot(*p) == before);
    CHECK(p->range() == std::pair{40, 80});

    doc->commandStack().redo();
    CHECK(snapshot(*p) == after);
    CHECK(p->range() == std::pair{40, 84});
  });
}

TEST_CASE(
    "piano roll: a resize clamped at the minimum duration replays and undoes exactly",
    "[midi][pianoroll][replay]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* p = newPianoRoll(*doc);
    p->setRange(48, 72);
    for(int i = 0; i < 3; i++)
    {
      Midi::NoteData n;
      n.m_start = 0.1 + 0.2 * i;
      n.m_duration = 0.05 * (i + 1);
      n.m_pitch = 60 + i;
      n.m_velocity = 100;
      p->notes.add(new Midi::Note{Id<Midi::Note>{i}, n, p});
    }
    const auto before = snapshot(*p);

    PianoRoll roll{*doc, *p};
    auto notes = roll.notes();
    REQUIRE(notes.size() == 3);
    for(auto* n : notes)
      n->setSelected(true);

    // The longest shortened to almost nothing: by as much, the others would
    // go below zero and stop at the minimum duration.
    auto* longest = *std::ranges::max_element(notes, {}, [](auto* n) {
      return n->boundingRect().width();
    });
    roll.resize(longest, -1000.);
    const auto clamped = snapshot(*p);
    for(auto& n : clamped)
      CHECK(n.duration > 0.);
    CHECK(clamped != before);

    replayLast(*doc);
    CHECK(snapshot(*p) == clamped);

    doc->commandStack().undo();
    CHECK(snapshot(*p) == before);
    doc->commandStack().redo();
    CHECK(snapshot(*p) == clamped);
  });
}
