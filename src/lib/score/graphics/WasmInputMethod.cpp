#if defined(__EMSCRIPTEN__)
// Input-method handling specific to the WebAssembly build. The non-wasm
// implementations of these functions are in GraphicsItem.cpp.
#include <score/graphics/GraphicsItem.hpp>
#include <score/widgets/ItemViewDrag.hpp>

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGuiApplication>
#include <QInputMethod>
#include <QWidget>
#include <QWindow>

#include <qpa/qwindowsysteminterface.h>

namespace score
{
void retargetInputMethod(QObject* target) noexcept
{
  auto* im = QGuiApplication::inputMethod();
  if(!im)
    return;

  // QGuiApplication::focusObject() is the focus object of the focus *window*.
  // On wasm every top-level is its own canvas with its own hidden <input>, and
  // a panel that took the platform focus -- the Inspector in particular --
  // makes focusObject() point into that unrelated window, which then answers
  // Qt::ImEnabled = false. Querying it would veto the retarget exactly when it
  // is needed, so ask the object we actually intend to type into.
  QObject* obj = target ? target : QGuiApplication::focusObject();
  if(!obj)
    return;

  QInputMethodQueryEvent query{Qt::ImEnabled};
  QCoreApplication::sendEvent(obj, &query);
  if(!query.value(Qt::ImEnabled).toBool())
    return;

  // Make the window that owns the target the focus window, otherwise
  // QWasmInputContext::updateInputElement() keeps pointing at another canvas'
  // <input> and every keystroke is delivered to that window instead.
  if(auto* w = qobject_cast<QWidget*>(obj))
  {
    if(!w->hasFocus())
      w->setFocus(Qt::OtherFocusReason);

    // Always normalise to the *top level*. Qt for wasm gives every native
    // widget its own container element, nested in its ancestors' containers,
    // and installs a "keydown"/"input" listener pair on each. The keydown
    // handler calls stopImmediatePropagation(), but QWasmWindow::handleInputEvent
    // does not, so an "input" event on a nested window's hidden <input> bubbles
    // up and is handled once per ancestor QWasmWindow -- i.e. every typed
    // character is committed once per level. Activating a mid-hierarchy native
    // window (the QSplitter, the Inspector) instead of the top level is what
    // put us in that situation and made scene editors insert everything twice.
    QWidget* top = w->window();
    QWindow* handle = top ? top->windowHandle() : nullptr;
    if(!handle)
      if(auto* native = w->nativeParentWidget())
        handle = native->windowHandle();
    if(!handle)
      handle = w->windowHandle();

    if(handle && QGuiApplication::focusWindow() != handle)
    {
      handle->requestActivate();
      QWindowSystemInterface::flushWindowSystemEvents();
    }
  }

  im->update(Qt::ImEnabled | Qt::ImQueryInput);
  im->show();
}

void watchSceneInputMethod(QGraphicsScene& scene)
{
  // Any drag started from anywhere in score would otherwise paint Qt's
  // temporary drag-image element in the corner of the page.
  installDragImageWorkaround();

  QObject::connect(
      &scene, &QGraphicsScene::focusItemChanged, &scene,
      [sc = &scene](QGraphicsItem* newItem, QGraphicsItem*, Qt::FocusReason) {
    if(!newItem || !(newItem->flags() & QGraphicsItem::ItemAcceptsInputMethod))
      return;

    // The view is what answers the input method queries for the scene.
    const auto views = sc->views();
    retargetInputMethod(views.empty() ? nullptr : views.first());
  });
}
}
#endif
