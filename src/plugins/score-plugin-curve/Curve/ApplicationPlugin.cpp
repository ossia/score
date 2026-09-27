#include <Curve/ApplicationPlugin.hpp>

#include <QApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QWindow>

namespace Curve
{

ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& ctx)
    : score::GUIApplicationPlugin{ctx}
{
  if(!ctx.mainWindow)
    return;

  // The tool is a function of the modifiers held, never a toggle: every key
  // event in the application is looked at, whichever widget has the focus,
  // and the state is read again when the application comes back, so that a
  // release that went elsewhere (another window, a menu, a focus change
  // mid-gesture) cannot leave a tool stuck.
  qApp->installEventFilter(this);
  connect(
      qGuiApp, &QGuiApplication::applicationStateChanged, this,
      [this](Qt::ApplicationState) {
    followModifiers(QGuiApplication::queryKeyboardModifiers());
  });
  connect(qGuiApp, &QGuiApplication::focusWindowChanged, this, [this] {
    followModifiers(QGuiApplication::queryKeyboardModifiers());
  });
}

Tool ApplicationPlugin::toolForModifiers(Qt::KeyboardModifiers mods) noexcept
{
  if(mods & Qt::ShiftModifier)
    return Tool::SetSegment;
  if(mods & Qt::ControlModifier)
    return Tool::Create;
  if(mods & Qt::AltModifier)
    return Tool::CreatePen;
  return Tool::Select;
}

void ApplicationPlugin::followModifiers(Qt::KeyboardModifiers mods)
{
  // Only the tools the modifiers choose between: Disabled and Playing are set
  // by the process for their own reasons.
  switch(m_editionSettings.tool())
  {
    case Tool::Select:
    case Tool::Create:
    case Tool::SetSegment:
    case Tool::CreatePen:
      m_editionSettings.setTool(toolForModifiers(mods));
      break;
    default:
      break;
  }
}

void ApplicationPlugin::onKey(const QKeyEvent& event, bool pressed)
{
  // A modifier's own key event may or may not count it in modifiers(),
  // depending on the platform: the key says which one changed.
  Qt::KeyboardModifiers mods = event.modifiers();
  Qt::KeyboardModifier changed = Qt::NoModifier;
  switch(event.key())
  {
    case Qt::Key_Shift:
      changed = Qt::ShiftModifier;
      break;
    case Qt::Key_Control:
      changed = Qt::ControlModifier;
      break;
    case Qt::Key_Alt:
      changed = Qt::AltModifier;
      break;
    case Qt::Key_Meta:
      changed = Qt::MetaModifier;
      break;
    default:
      break;
  }
  if(changed != Qt::NoModifier)
    mods.setFlag(changed, pressed);
  followModifiers(mods);
}

bool ApplicationPlugin::eventFilter(QObject* watched, QEvent* event)
{
  switch(event->type())
  {
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
      // Filters see the event once per receiver up the parent chain; the
      // result is the same each time.
      onKey(static_cast<QKeyEvent&>(*event), event->type() == QEvent::KeyPress);
      break;
    default:
      break;
  }
  return QObject::eventFilter(watched, event);
}

void ApplicationPlugin::on_keyPressEvent(QKeyEvent& event)
{
  onKey(event, true);
}

void ApplicationPlugin::on_keyReleaseEvent(QKeyEvent& event)
{
  onKey(event, false);
}

ApplicationPlugin::~ApplicationPlugin() { }

}
