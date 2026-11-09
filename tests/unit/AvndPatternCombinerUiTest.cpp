// Pattern combiner in a document: a process saved by score 3.9.0, whose only
// controls were Pattern and Mode, and a preset of it, load with the mode and
// the pattern they were saved with; its controls are greyed out while the
// selected mode does not read them, through mode changes and their undo; and
// its node shows its outlet once, with its name, at its top right.

#include <Process/Commands/SetControlValue.hpp>
#include <Process/Dataflow/NodeItem.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/Preset.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ProcessList.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/graphics/RectItem.hpp>
#include <score/graphics/TextItem.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QGraphicsRectItem>
#include <QCoreApplication>
#include <QGraphicsScene>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <cmath>
#include <map>
#include <string>

namespace
{
const QString combiner_uuid = QStringLiteral("18efe965-9acc-4703-9af3-3cef658b301a");

//! The process as score 3.9.0 saved it, from the address-tools example of the
//! documentation, its mode set to `mode`.
QByteArray saved_by_3_9_0(const char* mode)
{
  return QByteArray(R"_({"uuid": "18efe965-9acc-4703-9af3-3cef658b301a",
  "ObjectName": "avnd_pattern_combine", "id": 2,
  "Metadata": {"ScriptingName": "Pattern combiner", "Comment": "",
    "Color": "Transparent1", "Label": "", "Touched": true},
  "Duration": 11289600000, "Height": 300.0, "StartOffset": 0,
  "LoopDuration": 11289600000, "Pos": [43.98, -246.82], "Size": [238.0, 95.0],
  "Loops": false, "FoldMode": 0,
  "Inlets": [
    {"uuid": "9ae797ea-d94c-4792-acec-9ec1932bae5d", "ObjectName": "Inlet",
     "id": 0, "Hidden": true, "Custom": "Pattern", "Exposed": "pattern",
     "Value": {"String": "OSC:/pattern_combiner/*"}, "Init": {"String": ""},
     "Domain": {}},
    {"uuid": "8b1d76c4-3838-4ac0-9b9c-c12bc5db8e8a", "ObjectName": "Inlet",
     "id": 1, "Hidden": true, "Custom": "Mode", "Exposed": "mode",
     "Value": {"String": ")_")
         + mode
         + R"_("}, "Init": {"String": "List"},
     "Domain": {"String": {"Values": ["List", "Average", "Sum", "Min", "Max"]}},
     "Values": ["List", "Average", "Sum", "Min", "Max"], "Pixmaps": []}],
  "Outlets": [
    {"uuid": "cff96158-cc72-46d7-99dc-b6038171375b", "ObjectName": "Outlet",
     "id": 0, "Hidden": false, "Custom": "Output", "Exposed": "output"}]})_";
}

Process::ControlInlet* control(const Process::ProcessModel& proc, const QString& name)
{
  for(auto* in : proc.inlets())
    if(in->name() == name)
      return qobject_cast<Process::ControlInlet*>(in);
  return nullptr;
}

std::string text(const Process::ControlInlet& c)
{
  const auto* s = c.value().target<std::string>();
  return s ? *s : std::string{"<not a string>"};
}

const char* const old_modes[]{"List", "Average", "Sum", "Min", "Max"};
}

TEST_CASE(
    "a Pattern combiner saved by score 3.9.0 keeps its mode and its pattern",
    "[avnd][pattern_combiner][upgrade]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
    if(!score::test::add_process(*doc, combiner_uuid, {}))
      SKIP("the Pattern combiner is not built");

    for(int index = 0; index < 5; index++)
    {
      const char* mode = old_modes[index];
      CAPTURE(mode);
      const auto json = readJson(saved_by_3_9_0(mode));
      JSONObject::Deserializer des{json};
      std::unique_ptr<Process::ProcessModel> proc{
          deserialize_interface(pl, des, dctx, &doc->model())};
      REQUIRE(proc);

      auto* pattern = control(*proc, "Pattern");
      auto* m = qobject_cast<Process::Enum*>(control(*proc, "Mode"));
      REQUIRE(pattern);
      REQUIRE(m);
      CHECK(pattern->id().val() == 0);
      CHECK(m->id().val() == 1);
      CHECK(text(*pattern) == "OSC:/pattern_combiner/*");
      CHECK(text(*m) == mode);

      // Its five modes are the first five, in the same order: a mode saved by
      // its name or by its index is the same one
      const auto& values = m->getValues();
      REQUIRE(values.size() >= 5);
      for(int i = 0; i < 5; i++)
        CHECK(values[i].toStdString() == old_modes[i]);
      CHECK(m->indexOfValue(m->value()) == index);

      // The controls added since come at their defaults, which none of these
      // modes reads
      auto* n = control(*proc, "N");
      auto* test = qobject_cast<Process::Enum*>(control(*proc, "Test"));
      REQUIRE(n);
      REQUIRE(test);
      CHECK(ossia::convert<int>(n->value()) == 3);
      CHECK(text(*test) == "Above");
      REQUIRE(control(*proc, "Min"));
      REQUIRE(control(*proc, "Max"));
      CHECK(proc->outlets().size() == 1);
    }
  });
}

TEST_CASE(
    "a Pattern combiner preset saved by score 3.9.0 sets the mode and the pattern",
    "[avnd][pattern_combiner][upgrade][preset]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = score::test::add_process(*doc, combiner_uuid, {});
    if(!proc)
      SKIP("the Pattern combiner is not built");

    // 3.9.0 saved [id, value] for each control, without their names
    Process::Preset preset;
    preset.key.key = proc->concreteKey();
    preset.data = R"_([[0, {"String": "dev:/sensors/*"}], [1, {"String": "Max"}]])_";
    proc->loadPreset(preset);

    CHECK(text(*control(*proc, "Pattern")) == "dev:/sensors/*");
    CHECK(text(*control(*proc, "Mode")) == "Max");
    CHECK(ossia::convert<int>(control(*proc, "N")->value()) == 3);
    CHECK(text(*control(*proc, "Test")) == "Above");
  });
}

TEST_CASE(
    "the Pattern combiner greys out the controls its mode does not read",
    "[avnd][pattern_combiner][ui]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    auto* proc = score::test::add_process(*doc, combiner_uuid, {});
    if(!proc)
      SKIP("the Pattern combiner is not built");
    auto* mode = qobject_cast<Process::Enum*>(control(*proc, "Mode"));
    REQUIRE(mode);

    Process::DataflowManager dfm;
    FocusDispatcher fd;
    Process::Context pctx{dctx, dfm, fd};
    QGraphicsScene scene;
    auto* root = new QGraphicsRectItem{QRectF{0., 0., 1000., 1000.}};
    scene.addItem(root);
    auto* factory = ctx.interfaces<Process::LayerFactoryList>().findDefaultFactory(*proc);
    REQUIRE(factory);
    auto* item = factory->makeItem(*proc, pctx, root);
    REQUIRE(item);

    //! Whether the port of each control is shown enabled, by control name.
    auto enabled = [&] {
      std::map<std::string, bool> res;
      QList<QGraphicsItem*> items = item->childItems();
      while(!items.empty())
      {
        auto* i = items.takeLast();
        items.append(i->childItems());
        if(auto* port = dynamic_cast<Dataflow::PortItem*>(i))
          if(qobject_cast<const Process::Inlet*>(&port->port()))
            res[port->port().name().toStdString()] = port->isEnabled();
      }
      return res;
    };

    auto expected = [](const std::string& m) {
      const bool counting = m == "Count" || m == "Any" || m == "All";
      return std::map<std::string, bool>{
          {"Pattern", true}, {"Mode", true},     {"N", m == "TopN"},
          {"Test", counting}, {"Min", counting}, {"Max", counting}};
    };

    // At creation, List
    CHECK(enabled() == expected("List"));

    auto& stack = doc->commandStack();
    CommandDispatcher<> disp{dctx.commandStack};
    const auto values = mode->getValues();
    REQUIRE(values.size() == 20);
    REQUIRE(values[0] == "List");
    const int commands = stack.size();
    for(std::size_t i = 1; i < values.size(); i++)
    {
      const std::string m = values[i].toStdString();
      CAPTURE(m);
      disp.submit<Process::SetControlValue>(*mode, m);
      CHECK(text(*mode) == m);
      CHECK(enabled() == expected(m));
    }
    // One command a mode change: greying out is the view's, not the document's
    CHECK(stack.size() - commands == int(values.size()) - 1);

    // Undone and redone, the mode changes grey them out as they do
    for(int i = int(values.size()) - 1; i > 0; i--)
    {
      stack.undo();
      const std::string m = values[i - 1].toStdString();
      CAPTURE(m);
      CHECK(text(*mode) == m);
      CHECK(enabled() == expected(m));
    }
    for(int i = 1; i < int(values.size()); i++)
    {
      stack.redo();
      const std::string m = values[i].toStdString();
      CAPTURE(m);
      CHECK(text(*mode) == m);
      CHECK(enabled() == expected(m));
    }

    scene.removeItem(root);
    delete root;
  });
}

TEST_CASE(
    "the Pattern combiner's node shows its outlet once, named, at its top right",
    "[avnd][pattern_combiner][ui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = score::test::add_process(*doc, combiner_uuid, {});
    if(!proc)
      SKIP("the Pattern combiner is not built");
    REQUIRE(proc->outlets().size() == 1);
    const Process::Port& output = *proc->outlets()[0];

    Process::DataflowManager dfm;
    FocusDispatcher fd;
    Process::Context pctx{doc->context(), dfm, fd};
    QGraphicsScene scene;
    auto* root = new QGraphicsRectItem{QRectF{0., 0., 1000., 1000.}};
    scene.addItem(root);
    auto* node = new Process::NodeItem{*proc, pctx, TimeVal::fromMsecs(1000.), root};
    QCoreApplication::processEvents();

    //! The items of the node showing a port.
    auto itemsOf = [&](const Process::Port& port) {
      std::vector<Dataflow::PortItem*> res;
      QList<QGraphicsItem*> items = node->childItems();
      while(!items.empty())
      {
        auto* i = items.takeLast();
        items.append(i->childItems());
        if(auto* p = dynamic_cast<Dataflow::PortItem*>(i); p && &p->port() == &port)
          res.push_back(p);
      }
      return res;
    };

    // The layout shows it: the node does not add a bare one on its side
    const auto outs = itemsOf(output);
    REQUIRE(outs.size() == 1);
    auto* out = outs[0];
    CHECK(out->isVisible());

    // Its name next to it
    bool named = false;
    for(auto* sibling : out->parentItem()->childItems())
      if(auto* t = dynamic_cast<score::SimpleTextItem*>(sibling))
        named |= t->isVisible() && t->text() == "Output";
    CHECK(named);

    // At the top right: by the pattern's port, above the mode
    const auto pattern = itemsOf(*control(*proc, "Pattern"));
    const auto mode = itemsOf(*control(*proc, "Mode"));
    REQUIRE(pattern.size() == 1);
    REQUIRE(mode.size() == 1);
    const QPointF o = out->sceneBoundingRect().center();
    const QPointF p = pattern[0]->sceneBoundingRect().center();
    CHECK(o.x() > p.x() + 100.);
    CHECK(std::abs(o.y() - p.y()) < 8.);
    CHECK(o.y() < mode[0]->sceneBoundingRect().top());

    delete node;
    scene.removeItem(root);
    delete root;
  });
}
