// Integration test: editing a parameter in the device explorer — both from the
// address panel (AddressItemModel, the table under the tree) and from the
// Value column of the tree itself (DeviceExplorerDelegate).

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Keyboard.hpp>

#include <State/ValueConversion.hpp>

#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/AddressItemModel.hpp>
#include <Explorer/Explorer/Column.hpp>
#include <Explorer/Explorer/DeviceExplorerDelegate.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/domain/domain.hpp>

#include <catch2/catch_test_macros.hpp>

#include <QComboBox>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QStyleOptionViewItem>

#include <memory>

namespace
{
// An OSC device only ever sends: nothing has to answer for the tree to exist.
Device::ProtocolFactory* oscFactory(const score::GUIApplicationContext& ctx)
{
  for(auto& f : ctx.interfaces<Device::ProtocolFactoryList>())
    if(f.prettyName() == "OSC")
      return &f;
  return nullptr;
}

Device::AddressSettings enumParameter()
{
  Device::AddressSettings as;
  as.name = "mode";
  as.value = std::string{"a"};
  as.ioType = ossia::access_mode::BI;
  as.domain = ossia::make_domain(
      std::vector<ossia::value>{std::string{"a"}, std::string{"b"}});
  ossia::net::set_tags(as.extendedAttributes, ossia::net::tags{"initial"});
  return as;
}

Device::AddressSettings plainParameter(const QString& name, ossia::value v)
{
  Device::AddressSettings as;
  as.name = name;
  as.value = std::move(v);
  as.ioType = ossia::access_mode::BI;
  return as;
}

Device::FullAddressSettings full(const Device::Node& node)
{
  Device::FullAddressSettings as;
  static_cast<Device::AddressSettingsCommon&>(as) = node.get<Device::AddressSettings>();
  as.address = Device::address(node).address;
  return as;
}

struct Fixture
{
  score::Document* doc{};
  Explorer::DeviceExplorerModel* explorer{};
  Device::Node* param{};
  Device::Node* vecParam{};
  Device::Node* impulseParam{};

  explicit Fixture(const score::GUIApplicationContext& ctx)
  {
    doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* fact = oscFactory(ctx);
    REQUIRE(fact != nullptr);

    auto& devplug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    explorer = &devplug.explorer();

    auto settings = fact->defaultSettings();
    settings.name = "testdev";
    Device::Node dev{settings, nullptr};
    dev.push_back(Device::Node{enumParameter(), &dev});
    dev.push_back(
        Device::Node{plainParameter("pos", ossia::vec3f{{1.f, 2.f, 3.f}}), &dev});
    dev.push_back(
        Device::Node{plainParameter("bang", ossia::impulse{}), &dev});

    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit(new Explorer::Command::LoadDevice{devplug, std::move(dev)});

    REQUIRE(explorer->rootNode().childCount() >= 1);
    auto& devNode = explorer->rootNode().childAt(explorer->rootNode().childCount() - 1);
    REQUIRE(devNode.childCount() == 3);
    param = &devNode.childAt(0);
    vecParam = &devNode.childAt(1);
    impulseParam = &devNode.childAt(2);
  }

  std::unique_ptr<Explorer::AddressItemModel> makeAddressModel()
  {
    auto m = std::make_unique<Explorer::AddressItemModel>(nullptr);
    m->setState(explorer, Device::NodePath{*param}, full(*param));
    return m;
  }
};

QModelIndex row(const Explorer::AddressItemModel& m, int r)
{
  return m.index(r, 1, {});
}
}

TEST_CASE("the accepted values of a parameter can be edited", "[integration][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto mp = f.makeAddressModel();
    auto& m = *mp;

    // What the "Values" row displays before the edit.
    const auto shown = ossia::get_values(f.param->get<Device::AddressSettings>()
                                             .domain.get());
    REQUIRE(shown.size() == 2);

    // ... and what the line edit hands back: a parsed list.
    const bool ok = m.setData(
        row(m, Explorer::AddressItemModel::Rows::Values),
        QVariant::fromValue(ossia::value{std::vector<ossia::value>{
            std::string{"x"}, std::string{"y"}, std::string{"z"}}}),
        Qt::EditRole);
    CHECK(ok);

    const auto after
        = ossia::get_values(f.param->get<Device::AddressSettings>().domain.get());
    REQUIRE(after.size() == 3);
    CHECK(after[0] == ossia::value{std::string{"x"}});
    CHECK(after[2] == ossia::value{std::string{"z"}});
  });
}

TEST_CASE("a row that changes nothing does not land on the undo stack",
          "[integration][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto mp = f.makeAddressModel();
    auto& m = *mp;

    const auto before = f.doc->commandStack().size();

    // The address row is not editable; setting it used to push an empty command.
    CHECK_FALSE(m.setData(
        row(m, Explorer::AddressItemModel::Rows::Address), QString{"whatever"},
        Qt::EditRole));

    // Same value as the current one.
    CHECK_FALSE(m.setData(
        row(m, Explorer::AddressItemModel::Rows::Access),
        (int)ossia::access_mode::BI, Qt::EditRole));

    CHECK(f.doc->commandStack().size() == before);
  });
}

TEST_CASE("a value edited from the address panel shows up right away",
          "[integration][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto mp = f.makeAddressModel();
    auto& m = *mp;

    const auto valueRow = row(m, Explorer::AddressItemModel::Rows::Value);
    REQUIRE(m.setData(
        valueRow, QVariant::fromValue(ossia::value{std::string{"b"}}), Qt::EditRole));

    // The panel reflects it without waiting for the device to answer.
    CHECK(m.settings().value == ossia::value{std::string{"b"}});
    CHECK(m.data(valueRow, Qt::DisplayRole).toString() == "b");

    // ... and so does the tree, once the deferred update ran.
    QApplication::processEvents();
    CHECK(f.param->get<Device::AddressSettings>().value
          == ossia::value{std::string{"b"}});
  });
}

// Regression: editData built the index around the node's parent, so the
// dataChanged() it emitted matched no cell in the tree and "Refresh value"
// never repainted anything.
TEST_CASE("a refreshed value points at the cell that changed",
          "[integration][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};

    QModelIndex changed;
    QObject::connect(
        f.explorer, &Explorer::DeviceExplorerModel::dataChanged, f.explorer,
        [&](const QModelIndex& tl, const QModelIndex&, const QVector<int>&) {
      changed = tl;
        });

    f.explorer->editData(
        *f.param, Explorer::Column::Value, ossia::value{std::string{"b"}},
        Qt::EditRole);

    CHECK(f.param->get<Device::AddressSettings>().value
          == ossia::value{std::string{"b"}});

    REQUIRE(changed.isValid());
    CHECK(changed.column() == (int)Explorer::Column::Value);
    CHECK(&f.explorer->nodeFromModelIndex(changed) == f.param);
  });
}

namespace
{
// The delegate is driven exactly as the view drives it: through the
// QStyledItemDelegate interface.
struct TreeEditor
{
  Explorer::DeviceExplorerDelegate delegate;
  QStyledItemDelegate& iface{delegate};
  QModelIndex index;
  QWidget* editor{};

  TreeEditor(Explorer::DeviceExplorerModel& model, Device::Node& node)
      : index{model.modelIndexFromNode(node, (int)Explorer::Column::Value)}
  {
    QStyleOptionViewItem opt;
    editor = iface.createEditor(nullptr, opt, index);
    if(editor)
      iface.setEditorData(editor, index);
  }

  ~TreeEditor() { delete editor; }

  QComboBox* combo() const
  {
    return editor ? editor->findChild<QComboBox*>() : nullptr;
  }

  void commit(Explorer::DeviceExplorerModel& model)
  {
    iface.setModelData(editor, &model, index);
  }
};
}

TEST_CASE("a parameter with a value list is edited with a combo box",
          "[integration][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    TreeEditor ed{*f.explorer, *f.param};

    REQUIRE(ed.editor != nullptr);
    auto* cb = ed.combo();
    REQUIRE(cb != nullptr);

    // The values the device advertises, with the current one selected.
    REQUIRE(cb->count() == 2);
    CHECK(cb->itemText(0) == "a");
    CHECK(cb->itemText(1) == "b");
    CHECK(cb->currentText() == "a");

    // ... and it takes a value that is not in the list.
    CHECK(cb->isEditable());
  });
}

TEST_CASE("picking a listed value in the tree sets the parameter",
          "[integration][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    TreeEditor ed{*f.explorer, *f.param};
    REQUIRE(ed.combo() != nullptr);

    ed.combo()->setCurrentIndex(1);
    ed.commit(*f.explorer);

    CHECK(f.param->get<Device::AddressSettings>().value
          == ossia::value{std::string{"b"}});
  });
}

TEST_CASE("a value typed into the tree combo box is kept as typed",
          "[integration][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    TreeEditor ed{*f.explorer, *f.param};
    REQUIRE(ed.combo() != nullptr);

    ed.combo()->setCurrentText("something else");
    ed.commit(*f.explorer);

    CHECK(f.param->get<Device::AddressSettings>().value
          == ossia::value{std::string{"something else"}});
  });
}

TEST_CASE("a parameter without a value list gets a plain editor",
          "[integration][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};

    // Drop the enumeration: nothing left to offer a list of.
    f.param->get<Device::AddressSettings>().domain = ossia::domain{};

    TreeEditor ed{*f.explorer, *f.param};
    REQUIRE(ed.editor != nullptr);
    CHECK(ed.combo() == nullptr);

    // Still a string, still editable, just without a list to pick from.
    auto* line = ed.editor->findChild<QLineEdit*>();
    REQUIRE(line != nullptr);
    line->selectAll();
    score::test::keyClicks(*line, "typed");
    ed.commit(*f.explorer);
    CHECK(f.param->get<Device::AddressSettings>().value
          == ossia::value{std::string{"typed"}});
  });
}

// Regression: the tree had no editor for a vec, so Qt opened an empty line edit
// and committing it wrote [0, 0, 0] over the parameter.
TEST_CASE("a vec parameter is not zeroed by opening its editor",
          "[integration][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    TreeEditor ed{*f.explorer, *f.vecParam};
    REQUIRE(ed.editor != nullptr);

    // Committing without touching anything leaves the value as it was.
    ed.commit(*f.explorer);
    CHECK(f.vecParam->get<Device::AddressSettings>().value
          == ossia::value{ossia::vec3f{{1.f, 2.f, 3.f}}});

    // ... and each component can be edited.
    auto boxes = ed.editor->findChildren<QDoubleSpinBox*>();
    REQUIRE(boxes.size() == 3);
    boxes[1]->setValue(9.);
    ed.commit(*f.explorer);
    CHECK(f.vecParam->get<Device::AddressSettings>().value
          == ossia::value{ossia::vec3f{{1.f, 9.f, 3.f}}});
  });
}

// The bang is painted in the cell and answers the press there: it needs no
// editor, and an editor would only stack a second identical button on top of
// the painted one.
TEST_CASE("an impulse parameter is a button in the row, not an editor",
          "[integration][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    TreeEditor ed{*f.explorer, *f.impulseParam};
    CHECK(ed.editor == nullptr);

    // And the row does not offer to open one, so a double-click on the bang
    // does nothing but press it. (What the press writes is covered where a
    // real view can deliver the mouse events: DeviceExplorerEditorLookTest.)
    const auto idx
        = f.explorer->modelIndexFromNode(*f.impulseParam, (int)Explorer::Column::Value);
    CHECK_FALSE(f.explorer->flags(idx).testFlag(Qt::ItemIsEditable));
  });
}

// ---------------------------------------------------------------------------
// Renaming and metadata go through commands: they change the tree, so they must
// be undoable. Values do not: they are pushed straight to the device.
// ---------------------------------------------------------------------------

TEST_CASE("a parameter can be renamed from the tree, and it undoes",
          "[integration][explorer][undo]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};

    const auto nameIdx
        = f.explorer->modelIndexFromNode(*f.param, (int)Explorer::Column::Name);
    REQUIRE(f.explorer->flags(nameIdx) & Qt::ItemIsEditable);

    const auto before = f.doc->commandStack().size();
    REQUIRE(f.explorer->setData(nameIdx, QString{"renamed"}, Qt::EditRole));
    CHECK(f.param->get<Device::AddressSettings>().name == "renamed");
    CHECK(f.doc->commandStack().size() == before + 1);

    f.doc->commandStack().undo();
    CHECK(f.param->get<Device::AddressSettings>().name == "mode");

    f.doc->commandStack().redo();
    CHECK(f.param->get<Device::AddressSettings>().name == "renamed");
  });
}

TEST_CASE("a rename that would collide, or empty, is refused",
          "[integration][explorer][undo]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    const auto nameIdx
        = f.explorer->modelIndexFromNode(*f.param, (int)Explorer::Column::Name);
    const auto before = f.doc->commandStack().size();

    // "pos" is a sibling.
    CHECK_FALSE(f.explorer->setData(nameIdx, QString{"pos"}, Qt::EditRole));
    CHECK_FALSE(f.explorer->setData(nameIdx, QString{}, Qt::EditRole));

    CHECK(f.param->get<Device::AddressSettings>().name == "mode");
    CHECK(f.doc->commandStack().size() == before);
  });
}

TEST_CASE("a parameter can be renamed from the address panel",
          "[integration][explorer][undo]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto mp = f.makeAddressModel();
    auto& m = *mp;

    const auto nameRow = row(m, Explorer::AddressItemModel::Rows::Name);
    REQUIRE(m.flags(nameRow) & Qt::ItemIsEditable);

    // The editor opens on the current name rather than on an empty field.
    CHECK(m.data(nameRow, Qt::EditRole).toString() == "mode");

    const auto before = f.doc->commandStack().size();
    REQUIRE(m.setData(nameRow, QString{"renamed"}, Qt::EditRole));
    CHECK(f.param->get<Device::AddressSettings>().name == "renamed");
    CHECK(f.doc->commandStack().size() == before + 1);

    f.doc->commandStack().undo();
    CHECK(f.param->get<Device::AddressSettings>().name == "mode");
  });
}

TEST_CASE("changing the type drops a domain that no longer applies",
          "[integration][explorer][undo]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto mp = f.makeAddressModel();
    auto& m = *mp;

    // The enumeration is a list of strings; as an int it means nothing.
    REQUIRE(m.setData(
        row(m, Explorer::AddressItemModel::Rows::Type),
        QVariant::fromValue(ossia::val_type::INT), Qt::EditRole));

    const auto& after = f.param->get<Device::AddressSettings>();
    CHECK(after.value.get_type() == ossia::val_type::INT);
    CHECK(ossia::get_values(after.domain.get()).empty());

    f.doc->commandStack().undo();
    CHECK(f.param->get<Device::AddressSettings>().value.get_type()
          == ossia::val_type::STRING);
    CHECK(ossia::get_values(f.param->get<Device::AddressSettings>().domain.get()).size()
          == 2);
  });
}

TEST_CASE("tags can be edited from the address panel", "[integration][explorer][undo]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto mp = f.makeAddressModel();
    auto& m = *mp;

    // Extended attributes come after the fixed rows; find the tags one.
    int tagRow = -1;
    for(int r = Explorer::AddressItemModel::Rows::Count; r < m.rowCount({}); r++)
    {
      if(m.data(m.index(r, 0, {}), Qt::DisplayRole).toString() == "Tags")
        tagRow = r;
    }
    REQUIRE(tagRow != -1);

    const auto before = f.doc->commandStack().size();
    REQUIRE(m.setData(m.index(tagRow, 1, {}), QString{"one, two"}, Qt::EditRole));
    CHECK(f.doc->commandStack().size() == before + 1);

    const auto tags = ossia::net::get_tags(
        f.param->get<Device::AddressSettings>().extendedAttributes);
    REQUIRE(tags.has_value());
    REQUIRE(tags->size() == 2);
    CHECK((*tags)[0] == "one");
    CHECK((*tags)[1] == "two");

    // Writing the same tags again is not a change.
    CHECK_FALSE(m.setData(m.index(tagRow, 1, {}), QString{"one,two"}, Qt::EditRole));
    CHECK(f.doc->commandStack().size() == before + 1);

    f.doc->commandStack().undo();
    const auto restored = ossia::net::get_tags(
        f.param->get<Device::AddressSettings>().extendedAttributes);
    REQUIRE(restored.has_value());
    REQUIRE(restored->size() == 1);
    CHECK((*restored)[0] == "initial");
  });
}

// Values are state, not structure: pushing one to a device is not something the
// user undoes, and it must stay off the command stack.
TEST_CASE("editing a value does not touch the undo stack",
          "[integration][explorer][undo]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto mp = f.makeAddressModel();
    auto& m = *mp;

    const auto before = f.doc->commandStack().size();

    REQUIRE(m.setData(
        row(m, Explorer::AddressItemModel::Rows::Value),
        QVariant::fromValue(ossia::value{std::string{"b"}}), Qt::EditRole));
    REQUIRE(f.explorer->setData(
        f.explorer->modelIndexFromNode(*f.param, (int)Explorer::Column::Value),
        QVariant::fromValue(ossia::value{std::string{"a"}}), Qt::EditRole));

    CHECK(f.doc->commandStack().size() == before);
  });
}

TEST_CASE("a role that is not an edit does not change anything",
          "[integration][explorer][undo]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto mp = f.makeAddressModel();
    auto& m = *mp;

    const auto before = f.doc->commandStack().size();
    CHECK_FALSE(m.setData(
        row(m, Explorer::AddressItemModel::Rows::Name), QString{"renamed"},
        Qt::ToolTipRole));
    CHECK(f.param->get<Device::AddressSettings>().name == "mode");
    CHECK(f.doc->commandStack().size() == before);
  });
}

TEST_CASE("a boolean is edited through its check state", "[integration][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    f.param->get<Device::AddressSettings>().value = false;
    f.param->get<Device::AddressSettings>().domain = ossia::domain{};

    auto mp = f.makeAddressModel();
    auto& m = *mp;

    const auto valueRow = row(m, Explorer::AddressItemModel::Rows::Value);
    REQUIRE(m.flags(valueRow) & Qt::ItemIsUserCheckable);

    REQUIRE(m.setData(valueRow, (int)Qt::Checked, Qt::CheckStateRole));
    CHECK(m.settings().value == ossia::value{true});

    REQUIRE(m.setData(valueRow, (int)Qt::Unchecked, Qt::CheckStateRole));
    CHECK(m.settings().value == ossia::value{false});
  });
}

// Opening an editor and closing it without touching anything must not change
// what it was showing.
TEST_CASE("the values editor opens on the current list", "[integration][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    auto mp = f.makeAddressModel();
    auto& m = *mp;

    Explorer::AddressItemDelegate delegate;
    QStyledItemDelegate& iface = delegate;
    const auto idx = row(m, Explorer::AddressItemModel::Rows::Values);

    QStyleOptionViewItem opt;
    auto* editor = iface.createEditor(nullptr, opt, idx);
    REQUIRE(editor != nullptr);
    iface.setEditorData(editor, idx);

    auto* line = editor->findChild<QLineEdit*>();
    REQUIRE(line != nullptr);
    CHECK(line->text() == "[\"a\", \"b\"]");

    iface.setModelData(editor, &m, idx);
    delete editor;

    const auto after
        = ossia::get_values(f.param->get<Device::AddressSettings>().domain.get());
    REQUIRE(after.size() == 2);
    CHECK(after[0] == ossia::value{std::string{"a"}});
  });
}
