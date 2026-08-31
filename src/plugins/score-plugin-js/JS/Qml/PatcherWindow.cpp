#include <JS/Qml/PatcherContext.hpp>
#include <JS/Qml/PatcherWindow.hpp>

#include <Process/Process.hpp>

#include <score/document/DocumentContext.hpp>

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QWidget>

namespace JS
{

QWidget* createPatcherWindow(
    Process::ProcessModel& proc, const score::DocumentContext& ctx, QWidget* parent)
{
  // Create QML engine and window
  auto win = new QQuickWindow{};
  win->setTitle(QStringLiteral("Patcher - %1").arg(proc.prettyName()));
  win->setWidth(1024);
  win->setHeight(768);
  win->setColor(QColor("#1a1a2e"));

  // Create patcher context
  auto patcher = new PatcherContext{&proc, ctx, win};

  // Create QML engine
  auto engine = new QQmlEngine{win};
  engine->rootContext()->setContextProperty("patcher", patcher);
  engine->rootContext()->setContextProperty("Score", patcher->editContext());

  // Load the QML patcher view
  auto component = new QQmlComponent{
      engine, QUrl{QStringLiteral("qrc:/JS/Qml/qml/patcher/PatcherView.qml")}, win};

  if(component->isError())
  {
    qWarning() << "PatcherWindow QML errors:" << component->errors();
    delete win;
    return nullptr;
  }

  auto item = qobject_cast<QQuickItem*>(component->create());
  if(!item)
  {
    qWarning() << "PatcherWindow: failed to create QML root item";
    delete win;
    return nullptr;
  }

  item->setParentItem(win->contentItem());
  item->setParent(win->contentItem());

  // Make the QML item fill the window
  item->setWidth(win->width());
  item->setHeight(win->height());
  QObject::connect(win, &QQuickWindow::widthChanged, item, [item](int w) {
    item->setWidth(w);
  });
  QObject::connect(win, &QQuickWindow::heightChanged, item, [item](int h) {
    item->setHeight(h);
  });

  // Cleanup on close
  const auto cleanup = [&proc] {
    const_cast<QWidget*&>(proc.externalUI) = nullptr;
    proc.externalUIVisible(false);
  };

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 2)
  QObject::connect(win, &QQuickWindow::closing, &proc, cleanup);
#endif
  QObject::connect(win, &QQuickWindow::destroyed, &proc, cleanup);

  // Wrap in QWidget container
  auto widg = QWidget::createWindowContainer(win, parent);
  if(!widg)
  {
    cleanup();
    delete win;
    return nullptr;
  }
  widg->setAttribute(Qt::WA_DeleteOnClose);

  return widg;
}

}
