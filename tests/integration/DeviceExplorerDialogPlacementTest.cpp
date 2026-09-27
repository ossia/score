// Editing a node of the device explorer opens its dialog over the window it
// was asked from, not at the top-left of the screen.
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Explorer/DeviceExplorerView.hpp>
#include <Explorer/Explorer/DeviceExplorerWidget.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/document/Document.hpp>

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QItemSelectionModel>
#include <QTimer>

#include <catch2/catch_test_macros.hpp>

namespace
{
void settle(int n = 8)
{
  for(int i = 0; i < n; i++)
  {
    QCoreApplication::sendPostedEvents();
    QApplication::processEvents(QEventLoop::AllEvents, 10);
  }
}

QDialog* openDialog()
{
  for(auto* w : QApplication::topLevelWidgets())
    if(auto* d = qobject_cast<QDialog*>(w); d && d->isVisible())
      return d;
  return nullptr;
}
}

TEST_CASE(
    "the explorer's edit dialogs open over the explorer's window",
    "[integration][explorer][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& devices = doc->context().plugin<Explorer::DeviceDocumentPlugin>();

    Device::ProtocolFactory* osc{};
    for(auto& f : ctx.interfaces<Device::ProtocolFactoryList>())
      if(f.prettyName() == "OSC")
        osc = &f;
    REQUIRE(osc);
    auto settings = osc->defaultSettings();
    settings.name = "dev";
    Device::Node dev{settings, nullptr};
    Device::AddressSettings level;
    level.name = "level";
    level.value = 0.5f;
    level.ioType = ossia::access_mode::BI;
    dev.push_back(Device::Node{level, &dev});
    CommandDispatcher<>{doc->context().commandStack}.submit(
        new Explorer::Command::LoadDevice{devices, std::move(dev)});
    settle();

    auto* widget = Explorer::findDeviceExplorerWidgetInstance(ctx);
    REQUIRE(widget);
    auto* win = widget->window();
    win->setGeometry(400, 250, 1000, 700);
    win->show();
    settle(20);
    const QRect winRect = win->frameGeometry();

    QAction* edit{};
    for(auto* act : widget->findChildren<QAction*>())
      if(act->text() == QObject::tr("Edit"))
        edit = act;
    REQUIRE(edit);

    auto& explorer = devices.explorer();
    Device::Node* devNode{};
    for(auto& n : devices.rootNode())
      if(n.get<Device::DeviceSettings>().name == "dev")
        devNode = &n;
    REQUIRE(devNode);

    auto check = [&](Device::Node& node, const char* what) {
      INFO(what);
      auto sel = widget->view()->selectionModel();
      sel->setCurrentIndex(
          widget->proxyIndex(explorer.modelIndexFromNode(node, 0)),
          QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
      settle();

      // The device's dialog is modal (exec()): look at it from inside.
      QRect seen;
      bool found = false;
      QTimer::singleShot(0, [&] {
        settle(20);
        if(auto* d = openDialog())
        {
          found = true;
          seen = d->frameGeometry();
          d->reject();
        }
      });
      edit->trigger();
      if(!found)
      {
        settle(20);
        if(auto* d = openDialog())
        {
          found = true;
          seen = d->frameGeometry();
          d->reject();
        }
      }
      settle();
      REQUIRE(found);
      INFO("window " << winRect.x() << "," << winRect.y() << " " << winRect.width() << "x"
                     << winRect.height() << "; dialog " << seen.x() << "," << seen.y()
                     << " " << seen.width() << "x" << seen.height());
      CHECK(winRect.contains(seen.center()));
      CHECK(std::abs(seen.center().x() - winRect.center().x()) <= 40);
      CHECK(std::abs(seen.center().y() - winRect.center().y()) <= 40);
    };

    check(devNode->childAt(0), "a node");
    check(*devNode, "the device");
  });
}
