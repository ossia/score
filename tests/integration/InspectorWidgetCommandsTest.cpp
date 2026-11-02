// A control's widget in the inspector shows a value set elsewhere (a preset,
// an undo, the execution) without turning it into a command: one user action,
// one undo step. A change made in the widget is one.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Process/Dataflow/ControlWidgets.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QSpinBox>

#include <score/widgets/ControlWidgets.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
//! The first control of type T among the processes this build has, each made
//! in the document until one has it
template <typename T>
T* findControl(score::Document& doc)
{
  auto& ctx = doc.context();
  for(const Process::ProcessModelFactory& f :
      ctx.app.interfaces<Process::ProcessFactoryList>())
  {
    if(f.flags() & Process::ProcessFlags::RequiresCustomData)
      continue;
    auto* proc = score::test::add_process(
        doc, QString::fromLatin1(score::uuids::toByteArray(f.concreteKey().impl())), {});
    if(!proc)
      continue;
    for(auto* in : proc->inlets())
      if(auto c = qobject_cast<T*>(in))
        return c;
  }
  return nullptr;
}
}

TEST_CASE(
    "inspector widgets make no command for a value they are shown",
    "[integration][presets][undo]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dc = doc->context();
    auto steps = [&] { return doc->commandStack().size(); };
    QWidget parent;
    QObject context;

    SECTION("an integer spin box")
    {
      auto* c = findControl<Process::IntSpinBox>(*doc);
      REQUIRE(c);
      auto* w = WidgetFactory::IntSpinBox::make_widget(*c, dc, &parent, &context);
      const int n = steps();
      c->setValue(c->getMin() == w->value() ? c->getMax() : c->getMin());
      CHECK(steps() == n);
      w->setValue(w->value() == w->minimum() ? w->maximum() : w->minimum());
      CHECK(steps() == n + 1);
    }

    SECTION("a decimal spin box")
    {
      auto* c = findControl<Process::FloatSpinBox>(*doc);
      if(!c)
        SKIP("no process with a decimal spin box");
      auto* w = WidgetFactory::FloatSpinBox::make_widget(*c, dc, &parent, &context);
      const int n = steps();
      c->setValue(float(w->value() == w->minimum() ? w->maximum() : w->minimum()));
      CHECK(steps() == n);
      w->setValue(w->value() == w->minimum() ? w->maximum() : w->minimum());
      CHECK(steps() == n + 1);
    }

    SECTION("a check box")
    {
      auto* c = findControl<Process::Toggle>(*doc);
      REQUIRE(c);
      auto* w = WidgetFactory::Toggle::make_widget(*c, dc, &parent, &context);
      const int n = steps();
      c->setValue(!w->isChecked());
      CHECK(steps() == n);
      w->setChecked(!w->isChecked());
      CHECK(steps() == n + 1);
    }

    SECTION("a toggle with two labels")
    {
      // A control of a process in the document, so that its command finds it
      auto* proc = findControl<Process::IntSpinBox>(*doc)->parent();
      REQUIRE(proc);
      auto* c = new Process::ChooserToggle{
          {"Off", "On"}, false, "Mode", Id<Process::Port>{4242}, proc};
      auto* w = static_cast<score::ToggleButton*>(
          WidgetFactory::ChooserToggle::make_widget(*c, dc, &parent, &context));
      const int n = steps();
      const bool next = !w->isChecked();
      c->setValue(next);
      CHECK(steps() == n);
      CHECK(w->isChecked() == next);
      // Its label follows
      CHECK(w->text() == w->alternatives[next ? 1 : 0]);
      w->toggle();
      CHECK(steps() == n + 1);
      CHECK(w->text() == w->alternatives[next ? 0 : 1]);
    }

    SECTION("a text field")
    {
      auto* c = findControl<Process::LineEdit>(*doc);
      if(!c)
        SKIP("no process with a text field");
      auto* w = WidgetFactory::LineEdit::make_widget(*c, dc, &parent, &context);
      const int n = steps();
      c->setValue(std::string{"from elsewhere"});
      CHECK(steps() == n);
      // Leaving the field unchanged edits nothing
      Q_EMIT w->editingFinished();
      CHECK(steps() == n);
      w->setText("typed");
      Q_EMIT w->editingFinished();
      CHECK(steps() == n + 1);
    }
  });
}
