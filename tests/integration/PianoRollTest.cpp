// The piano roll: moving the selection with the arrow keys, undoing a
// resize of the notes, and a background that lets the interval's bar lines
// show through.

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
#include <Midi/MidiStyle.hpp>

#include <core/command/CommandStack.hpp>

#include <core/document/Document.hpp>

#include <QGraphicsRectItem>
#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>

#include <catch2/catch_test_macros.hpp>

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
  //! Something else in the scene that takes the keyboard focus, as a selected
  //! node does when selecting a note selects its process.
  QGraphicsRectItem focusThief{QRectF{0., 0., 10., 10.}};
  // The piano roll's own classes are internal to its plug-in: it is driven
  // through the base classes and the scene, as a user would.
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
    // What a click in the layer does, through the focus dispatcher.
    presenter->setFocus(true);
  }

  ~PianoRoll()
  {
    delete presenter;
    delete view;
    scene.removeItem(&root);
    scene.removeItem(&focusThief);
  }

  //! The note items, lowest pitch first.
  std::vector<QGraphicsItem*> notes() const
  {
    std::vector<QGraphicsItem*> res;
    for(auto* it : view->childItems())
      if(it->flags() & QGraphicsItem::ItemIsSelectable)
        res.push_back(it);
    std::sort(res.begin(), res.end(), [](auto* a, auto* b) {
      return a->pos().y() > b->pos().y();
    });
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

  //! Drags the right edge of a note by dx, as to lengthen or shorten it.
  void resize(QGraphicsItem* note, double dx)
  {
    const QPointF edge{note->boundingRect().width() - 1., 1.};
    mouse(note, QEvent::GraphicsSceneMousePress, edge, edge);
    mouse(note, QEvent::GraphicsSceneMouseMove, edge + QPointF{dx, 0.}, edge);
    mouse(note, QEvent::GraphicsSceneMouseRelease, edge + QPointF{dx, 0.}, edge);
  }

  //! A key press as the document's view receives it.
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

void addNote(Midi::ProcessModel& p, int pitch, double start)
{
  Midi::NoteData n;
  n.m_start = start;
  n.m_duration = 0.1;
  n.m_pitch = pitch;
  n.m_velocity = 100;
  p.notes.add(new Midi::Note{Id<Midi::Note>{int(p.notes.size())}, n, &p});
}

std::vector<int> pitches(const Midi::ProcessModel& p)
{
  std::vector<int> res;
  for(auto& n : p.notes)
    res.push_back(n.pitch());
  std::sort(res.begin(), res.end());
  return res;
}
}

TEST_CASE("piano roll: Up / Down move the selection by a semitone, with Shift an octave",
          "[midi][pianoroll]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* p = newPianoRoll(*doc);
    p->setRange(36, 84);
    addNote(*p, 60, 0.1);
    addNote(*p, 64, 0.2);
    addNote(*p, 67, 0.3);

    PianoRoll roll{*doc, *p};
    auto notes = roll.notes();
    REQUIRE(notes.size() == 3);

    // Nothing selected: nothing moves.
    roll.key(Qt::Key_Up);
    CHECK(pitches(*p) == std::vector{60, 64, 67});

    // C and E selected, G left alone.
    notes[0]->setSelected(true);
    notes[1]->setSelected(true);

    roll.key(Qt::Key_Up);
    CHECK(pitches(*p) == std::vector{61, 65, 67});
    roll.key(Qt::Key_Down);
    roll.key(Qt::Key_Down);
    CHECK(pitches(*p) == std::vector{59, 63, 67});
    roll.key(Qt::Key_Up, Qt::ShiftModifier);
    CHECK(pitches(*p) == std::vector{67, 71, 75});
    roll.key(Qt::Key_Down, Qt::ShiftModifier);
    CHECK(pitches(*p) == std::vector{59, 63, 67});

    // The views follow the model: B (59) is drawn below G (67).
    CHECK(notes[0]->pos().y() > notes[2]->pos().y());

    // One command per key press.
    doc->commandStack().undo();
    CHECK(pitches(*p) == std::vector{67, 71, 75});
    doc->commandStack().redo();

    // Past the range, the range grows, and one undo takes both back.
    roll.key(Qt::Key_Up, Qt::ShiftModifier);
    CHECK(pitches(*p) == std::vector{67, 71, 75});
    CHECK(p->range() == std::pair{36, 84});
    roll.key(Qt::Key_Up, Qt::ShiftModifier); // 87 > 84
    CHECK(pitches(*p) == std::vector{67, 83, 87});
    CHECK(p->range() == std::pair{36, 87});
    doc->commandStack().undo();
    CHECK(pitches(*p) == std::vector{67, 71, 75});
    CHECK(p->range() == std::pair{36, 84});
    doc->commandStack().redo();
    CHECK(p->range() == std::pair{36, 87});

    // Past MIDI's own range, the step is refused as a whole: the chord is not
    // squashed at the edge.
    for(int i = 0; i < 4; i++)
      roll.key(Qt::Key_Up, Qt::ShiftModifier);
    CHECK(pitches(*p) == std::vector{67, 119, 123});
    roll.key(Qt::Key_Up, Qt::ShiftModifier); // 131 > 127
    CHECK(pitches(*p) == std::vector{67, 119, 123});
    CHECK(p->range() == std::pair{36, 123});
  });
}

TEST_CASE("piano roll: a note dragged past the top of the range grows it", "[midi][pianoroll]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* p = newPianoRoll(*doc);
    p->setRange(48, 72);
    addNote(*p, 70, 0.1);

    PianoRoll roll{*doc, *p};
    auto notes = roll.notes();
    REQUIRE(notes.size() == 1);
    auto* v = notes.front();
    const double row = 600. / 25.;

    // Five rows up, which is past 72: the range follows the note.
    const QPointF grab{2., 2.};
    roll.mouse(v, QEvent::GraphicsSceneMousePress, grab, grab);
    const QPointF up = grab - QPointF{0., 5. * row};
    roll.mouse(v, QEvent::GraphicsSceneMouseMove, up, grab);
    roll.mouse(v, QEvent::GraphicsSceneMouseMove, up, grab); // again: no runaway
    roll.mouse(v, QEvent::GraphicsSceneMouseRelease, up, grab);
    CHECK(pitches(*p) == std::vector{75});
    CHECK(p->range() == std::pair{48, 75});

    doc->commandStack().undo();
    CHECK(pitches(*p) == std::vector{70});
    CHECK(p->range() == std::pair{48, 72});
  });
}

TEST_CASE("piano roll: undoing a resize of the notes resizes their views", "[midi][pianoroll]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* p = newPianoRoll(*doc);
    addNote(*p, 60, 0.1);

    PianoRoll roll{*doc, *p};
    auto notes = roll.notes();
    REQUIRE(notes.size() == 1);
    auto* v = notes.front();
    auto& note = *p->notes.begin();
    const double width = v->boundingRect().width();

    roll.resize(v, 50.);
    CHECK(note.duration() > 0.1);
    CHECK(v->boundingRect().width() > width);

    // The view follows the model back, not the width it was dragged to.
    doc->commandStack().undo();
    CHECK(note.duration() == 0.1);
    CHECK(v->boundingRect().width() == width);

    // Shortened, then undone.
    roll.resize(v, -width / 2.);
    CHECK(note.duration() > 0.);
    CHECK(note.duration() < 0.1);
    doc->commandStack().undo();
    CHECK(note.duration() == 0.1);
    CHECK(v->boundingRect().width() == width);
  });
}

TEST_CASE("piano roll: the rows are tints, black keys darker than white ones",
          "[midi][pianoroll]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto& s = Midi::MidiStyle::instance();
    // Translucent rows let the bar lines drawn behind the processes in musical
    // mode show through.
    CHECK(s.whiteKeyBrush.color().alpha() < 64);
    CHECK(s.blackKeyBrush.color().alpha() < 128);
    CHECK(s.semitonePen.color().alpha() < 64);
    CHECK(s.octavePen.color().alpha() < 128);
    CHECK(s.semitonePen.color().alpha() < s.octavePen.color().alpha());
    CHECK(s.whiteKeyBrush.color().lightness() > s.blackKeyBrush.color().lightness());
  });
}
