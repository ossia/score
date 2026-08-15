// Integration test: editing a parameter in the device explorer — both from the
// address panel (AddressItemModel, the table under the tree) and from the
// Value column of the tree itself (DeviceExplorerDelegate).

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

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
#include <QtTest/QTest>
#include <QLineEdit>
#include <QDoubleSpinBox>
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
    QTest::keyClicks(line, "typed");
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

TEST_CASE("an impulse parameter can be fired from the tree",
          "[integration][explorer]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    Fixture f{ctx};
    TreeEditor ed{*f.explorer, *f.impulseParam};
    REQUIRE(ed.editor != nullptr);

    auto* button = ed.editor->findChild<QPushButton*>();
    REQUIRE(button != nullptr);

    // The button commits as it is pressed rather than when the editor closes.
    int commits{};
    QObject::connect(
        &ed.delegate, &QAbstractItemDelegate::commitData, &ed.delegate,
        [&](QWidget*) { commits++; });

    button->click();
    CHECK(commits == 1);

    // Nothing was pressed the second time round, so nothing is written.
    TreeEditor untouched{*f.explorer, *f.impulseParam};
    REQUIRE(untouched.editor != nullptr);
    int untouchedCommits{};
    QObject::connect(
        &untouched.delegate, &QAbstractItemDelegate::commitData, &untouched.delegate,
        [&](QWidget*) { untouchedCommits++; });
    untouched.commit(*f.explorer);
    CHECK(untouchedCommits == 0);
  });
}
