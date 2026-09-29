// Piano roll at black-MIDI scale: selecting, reselecting, deleting and
// replacing among 100k notes, and closing the document, has to stay linear in
// the number of notes, through the document presenter, the command stack and
// the command dispatchers as in the application.

#include <Process/DocumentPlugin.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Commands/Interval/RemoveProcessFromInterval.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Midi/Commands/AddNote.hpp>
#include <Midi/MidiProcess.hpp>

#include <score/command/AggregateCommand.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/command/Dispatchers/MultiOngoingCommandDispatcher.hpp>
#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <score/model/ObjectEditor.hpp>
#include <score/selection/SelectionStack.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGraphicsRectItem>
#include <QGraphicsScene>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <algorithm>

namespace
{
// Linear work on 100k notes takes well under a second even in a Debug build;
// a per-note linear lookup takes minutes.
constexpr qint64 budget_ms = 20'000;

constexpr int count = 100'000;

Midi::ProcessModel* newPianoRoll(score::Document& doc, int count)
{
  auto* p = dynamic_cast<Midi::ProcessModel*>(score::test::add_process(
      doc, QStringLiteral("c189e507-3afa-4a53-9369-e38860e6bb2d"), {}));
  REQUIRE(p);
  for(int i = 0; i < count; i++)
  {
    Midi::NoteData n;
    n.m_start = double(i) / count;
    n.m_duration = 0.5 / count;
    n.m_pitch = 24 + i % 80;
    n.m_velocity = 100;
    p->notes.add(new Midi::Note{Id<Midi::Note>{i}, n, p});
  }
  return p;
}

struct PianoRoll
{
  score::Document& doc;
  Midi::ProcessModel& proc;
  Process::DataflowManager dfm;
  FocusDispatcher fd;
  Process::Context pctx{doc.context(), dfm, fd};
  QGraphicsScene scene;
  QGraphicsRectItem root{QRectF{0., 0., 1000., 1000.}};
  Process::LayerView* view{};
  Process::LayerPresenter* presenter{};

  PianoRoll(score::Document& d, Midi::ProcessModel& p)
      : doc{d}
      , proc{p}
  {
    scene.addItem(&root);
    auto& layers = doc.context().app.interfaces<Process::LayerFactoryList>();
    auto* fact = layers.findDefaultFactory(proc);
    REQUIRE(fact);
    view = fact->makeLayerView(proc, pctx, &root);
    REQUIRE(view);
    presenter = fact->makeLayerPresenter(proc, view, pctx, nullptr);
    REQUIRE(presenter);
    presenter->setHeight(600.);
    presenter->setWidth(1000., 1000.);
  }

  ~PianoRoll()
  {
    delete presenter;
    delete view;
    scene.removeItem(&root);
  }

  std::vector<QGraphicsItem*> notes() const
  {
    std::vector<QGraphicsItem*> res;
    for(auto* it : view->childItems())
      if(it->flags() & QGraphicsItem::ItemIsSelectable)
        res.push_back(it);
    return res;
  }
};

const CommandGroupKey& testCommandGroup()
{
  static const CommandGroupKey k{"PianoRollSelectionScaleTest"};
  return k;
}

// Declared by hand rather than with SCORE_COMMAND_DECL, which the build system
// scans for serializable commands. ReplaceNotes, with the update() that the
// ongoing dispatchers call on every mouse move: a drag whose every step
// destroys and rebuilds all the notes.
class DragReplaceNotes final : public score::Command
{
public:
  DragReplaceNotes(const Midi::ProcessModel& p, const std::vector<Midi::NoteData>& n)
      : m_cmd{p, n, 0, 127, p.duration()}
  {
  }

  void update(const Midi::ProcessModel&, const std::vector<Midi::NoteData>&) { }

  void undo(const score::DocumentContext& ctx) const override { m_cmd.undo(ctx); }
  void redo(const score::DocumentContext& ctx) const override { m_cmd.redo(ctx); }

  static const CommandKey& static_key() noexcept
  {
    static const CommandKey k{"DragReplaceNotes"};
    return k;
  }
  const CommandGroupKey& parentKey() const noexcept override
  {
    return testCommandGroup();
  }
  const CommandKey& key() const noexcept override { return static_key(); }
  QString description() const override { return QStringLiteral("Drag"); }
  void serializeImpl(DataStreamInput&) const override { }
  void deserializeImpl(DataStreamOutput&) override { }

private:
  Midi::ReplaceNotes m_cmd;
};

class TestMacro final : public score::AggregateCommand
{
public:
  const CommandGroupKey& parentKey() const noexcept override
  {
    return testCommandGroup();
  }
  const CommandKey& key() const noexcept override
  {
    static const CommandKey k{"TestMacro"};
    return k;
  }
  QString description() const override { return QStringLiteral("Macro"); }
};

qint64 selectAndFlush(const std::vector<QGraphicsItem*>& notes, bool selected)
{
  QElapsedTimer t;
  t.start();
  for(auto* n : notes)
    n->setSelected(selected);
  QCoreApplication::processEvents();
  return t.elapsed();
}

std::size_t selectedModelNotes(const Midi::ProcessModel& p)
{
  return std::count_if(
      p.notes.begin(), p.notes.end(), [](auto& n) { return n.selection.get(); });
}
}

TEST_CASE(
    "piano roll: selecting all of 100k notes, then all but one, stays linear",
    "[midi][pianoroll][scale]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* p = newPianoRoll(*doc, count);
    PianoRoll roll{*doc, *p};
    auto notes = roll.notes();
    REQUIRE(notes.size() == std::size_t(count));

    auto& selection = doc->context().selectionStack;
    int pushes = 0;
    QObject::connect(
        &selection, &score::SelectionStack::currentSelectionChanged, &selection,
        [&] { pushes++; });

    // The document presenter forwards the selection to the process, which
    // marks each selected note.
    const auto all = selectAndFlush(notes, true);
    CHECK(pushes == 1);
    CHECK(selection.currentSelection().size() == count);
    CHECK(selectedModelNotes(*p) == std::size_t(count));
    CHECK(all < budget_ms);

    // One large selection replacing another.
    const auto allButOne = selectAndFlush({notes.front()}, false);
    CHECK(pushes == 2);
    CHECK(selection.currentSelection().size() == count - 1);
    CHECK(selectedModelNotes(*p) == std::size_t(count - 1));
    CHECK(allButOne < budget_ms);
  });
}

TEST_CASE(
    "piano roll: a process marks the selected notes among 100k in linear time",
    "[midi][pianoroll][scale]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* p = newPianoRoll(*doc, count);

    std::vector<IdentifiedObjectAbstract*> ptrs;
    for(auto& n : p->notes)
      ptrs.push_back(&n);
    const Selection all(ptrs.begin(), ptrs.end());

    QElapsedTimer t;
    t.start();
    p->setSelection(all);
    const auto elapsed = t.elapsed();
    CHECK(selectedModelNotes(*p) == std::size_t(count));
    CHECK(elapsed < budget_ms);

    p->setSelection(Selection{});
    CHECK(selectedModelNotes(*p) == 0);
  });
}

TEST_CASE(
    "piano roll: deleting the selected half of 100k notes stays linear",
    "[midi][pianoroll][scale]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* p = newPianoRoll(*doc, count);
    PianoRoll roll{*doc, *p};
    auto notes = roll.notes();
    REQUIRE(notes.size() == std::size_t(count));

    std::vector<QGraphicsItem*> half;
    for(std::size_t i = 0; i < notes.size(); i += 2)
      half.push_back(notes[i]);
    selectAndFlush(half, true);
    const auto sel = doc->context().selectionStack.currentSelection();
    REQUIRE(sel.size() == count / 2);

    // What the Delete action does with the document's selection, the piano
    // roll having the focus as after a click in it.
    doc->focusManager().set(QPointer<Midi::ProcessModel>{p});
    QElapsedTimer t;
    t.start();
    bool removed = false;
    for(auto& iface : ctx.interfaces<score::ObjectEditorList>())
      if((removed = iface.remove(sel, doc->context())))
        break;
    QCoreApplication::processEvents();
    const auto elapsed = t.elapsed();
    CHECK(removed);
    CHECK(p->notes.size() == std::size_t(count - count / 2));
    CHECK(roll.notes().size() == std::size_t(count - count / 2));
    CHECK(doc->context().selectionStack.currentSelection().empty());
    CHECK(elapsed < budget_ms);

    doc->commandStack().undo();
    CHECK(p->notes.size() == std::size_t(count));
    CHECK(roll.notes().size() == std::size_t(count));
  });
}

TEST_CASE(
    "piano roll: commands destroying 100k selected notes update the selection once",
    "[midi][pianoroll][scale]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* p = newPianoRoll(*doc, count);
    PianoRoll roll{*doc, *p};
    auto& selection = doc->context().selectionStack;
    int changes = 0;
    QObject::connect(
        &selection, &score::SelectionStack::currentSelectionChanged, &selection,
        [&] { changes++; });

    // Each command replaces every note while all of them are selected.
    auto replaceAllSelected = [&](auto&& command) {
      const auto before = selection.currentSelection();
      selectAndFlush(roll.notes(), true);
      REQUIRE(selection.currentSelection().size() == count);
      changes = 0;

      QElapsedTimer t;
      t.start();
      command();
      QCoreApplication::processEvents();
      const auto elapsed = t.elapsed();

      // The notes' selection goes away from the history: the one before is
      // current again.
      CHECK(changes == 1);
      CHECK(selection.currentSelection() == before);
      CHECK(p->notes.size() == std::size_t(count));
      CHECK(roll.notes().size() == std::size_t(count));
      CHECK(selectedModelNotes(*p) == 0);
      CHECK(elapsed < budget_ms);
    };

    std::vector<Midi::NoteData> replacement;
    for(auto& n : p->notes)
      replacement.push_back(n.noteData());

    // What dropping a MIDI file on the piano roll does.
    replaceAllSelected([&] {
      doc->commandStack().redoAndPush(
          new Midi::ReplaceNotes{*p, replacement, 0, 127, p->duration()});
    });
    replaceAllSelected([&] { doc->commandStack().undo(); });
    replaceAllSelected([&] { doc->commandStack().redo(); });
  });
}

TEST_CASE(
    "piano roll: each dispatcher step destroying 20k selected notes updates the "
    "selection once",
    "[midi][pianoroll][scale]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    constexpr int dragCount = 20'000;
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* p = newPianoRoll(*doc, dragCount);
    PianoRoll roll{*doc, *p};
    auto& selection = doc->context().selectionStack;
    int changes = 0;
    QObject::connect(
        &selection, &score::SelectionStack::currentSelectionChanged, &selection,
        [&] { changes++; });

    std::vector<Midi::NoteData> replacement;
    for(auto& n : p->notes)
      replacement.push_back(n.noteData());

    const auto before = selection.currentSelection();
    const bool couldUnselect = selection.canUnselect();
    const bool couldReselect = selection.canReselect();

    // Each step runs with every note selected; it destroys all of them, so
    // their selection leaves the history and the one before is current again.
    auto step = [&](auto&& action) {
      selectAndFlush(roll.notes(), true);
      REQUIRE(selection.currentSelection().size() == dragCount);
      changes = 0;

      QElapsedTimer t;
      t.start();
      action();
      QCoreApplication::processEvents();
      const auto elapsed = t.elapsed();

      CHECK(changes == 1);
      CHECK(selection.currentSelection() == before);
      CHECK(selection.canUnselect() == couldUnselect);
      CHECK(selection.canReselect() == couldReselect);
      CHECK(p->notes.size() == std::size_t(dragCount));
      CHECK(roll.notes().size() == std::size_t(dragCount));
      CHECK(selectedModelNotes(*p) == 0);
      CHECK(elapsed < budget_ms);
    };
    auto replaceNotes
        = [&] { return new Midi::ReplaceNotes{*p, replacement, 0, 127, p->duration()}; };
    const auto& stack = doc->context().commandStack;

    {
      INFO("OngoingCommandDispatcher");
      auto& d = doc->context().dispatcher;
      for(int i = 0; i < 3; i++)
        step([&] { d.submit<DragReplaceNotes>(*p, replacement); });
      step([&] { d.rollback(); });
    }
    {
      INFO("SingleOngoingCommandDispatcher");
      SingleOngoingCommandDispatcher<DragReplaceNotes> d{stack};
      for(int i = 0; i < 3; i++)
        step([&] { d.submit(*p, replacement); });
      step([&] { d.rollback(); });
    }
    {
      INFO("MultiOngoingCommandDispatcher");
      MultiOngoingCommandDispatcher d{stack};
      for(int i = 0; i < 3; i++)
        step([&] { d.submit<DragReplaceNotes>(*p, replacement); });
      step([&] { d.submit(replaceNotes()); });
      step([&] { d.rollback(); });
    }
    {
      INFO("RedoMacroCommandDispatcher");
      RedoMacroCommandDispatcher<TestMacro> d{stack};
      for(int i = 0; i < 3; i++)
        step([&] { d.submit(replaceNotes()); });
      step([&] { d.rollback(); });
    }
    {
      INFO("SendStrategy::UndoRedo");
      GenericMacroCommandDispatcher<
          TestMacro, RedoStrategy::Redo, SendStrategy::UndoRedo>
          d{stack};
      step([&] { d.submit(replaceNotes()); });
      // The undo destroys the selected notes; the redo only unselected ones.
      step([&] { d.commit(); });
    }
  });
}

// A scenario clears the selection when it is destroyed. Without one in the
// document, nothing else stops each note from pruning the selection stack as
// it dies.
template <typename Close>
void closeWithSelectedNotes(Close&& close)
{
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto& root = score::test::base_interval(*doc);
    std::vector<Id<Process::ProcessModel>> scenarios;
    for(auto& proc : root.processes)
      if(qobject_cast<Scenario::ProcessModel*>(&proc))
        scenarios.push_back(proc.id());
    for(auto& id : scenarios)
      doc->commandStack().redoAndPush(
          new Scenario::Command::RemoveProcessFromInterval{root, id});

    auto* p = newPianoRoll(*doc, count);

    std::vector<IdentifiedObjectAbstract*> ptrs;
    for(auto& note : p->notes)
      ptrs.push_back(&note);
    auto& selection = doc->context().selectionStack;
    selection.pushNewSelection(Selection(ptrs.begin(), ptrs.end()));
    REQUIRE(selection.currentSelection().size() == count);

    QElapsedTimer t;
    t.start();
    close(ctx, *doc);
    const auto elapsed = t.elapsed();
    CHECK(ctx.docManager.documents().empty());
    CHECK(elapsed < budget_ms);
  });
}

TEST_CASE(
    "piano roll: closing a document with 100k selected notes stays linear",
    "[midi][pianoroll][scale]")
{
  closeWithSelectedNotes(
      [](const score::GUIApplicationContext& ctx, score::Document& doc) {
    ctx.docManager.forceCloseDocument(ctx, doc);
  });
}

TEST_CASE(
    "piano roll: a document with 100k selected notes still open at exit closes "
    "in linear time",
    "[midi][pianoroll][scale]")
{
  closeWithSelectedNotes([](const score::GUIApplicationContext& ctx, score::Document&) {
    ctx.docManager.closeRemainingDocuments(&ctx);
  });
}
