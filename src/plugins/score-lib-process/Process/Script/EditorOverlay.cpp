#include "EditorOverlay.hpp"

#include "ScriptTabBar.hpp"
#include "ScriptWidget.hpp"

#include <score/graphics/BackgroundRenderer.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateView.hpp>

#include <QAbstractScrollArea>
#include <QApplication>
#include <QCloseEvent>
#include <QCodeEditor>
#include <QDialogButtonBox>
#include <QHideEvent>
#include <QLayout>
#include <QPainter>
#include <QSettings>
#include <QShowEvent>
#include <QStackedLayout>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QVariant>

#include <wobjectimpl.h>

#include <algorithm>

W_OBJECT_IMPL(Process::EditorOverlayHost)
W_OBJECT_IMPL(Process::DocumentBackgroundPreview)

namespace Process
{
static constexpr const char* originalPaletteProperty = "overlayOriginalPalette";
static constexpr const char* originalAutoFillProperty = "overlayOriginalAutoFill";
static constexpr const char* originalViewportAutoFillProperty
    = "overlayOriginalViewportAutoFill";

void setEditorTransparent(QWidget* editor, bool transparent)
{
  if(!editor)
    return;

  editor->setAutoFillBackground(false);
  for(auto tabs : editor->findChildren<QTabWidget*>())
    tabs->setAutoFillBackground(false);
  for(auto stack : editor->findChildren<QStackedWidget*>())
    stack->setAutoFillBackground(false);

  // What to restore, taken before anything is touched
  for(auto area : editor->findChildren<QAbstractScrollArea*>())
  {
    if(!area->property(originalAutoFillProperty).isValid())
    {
      area->setProperty(originalPaletteProperty, QVariant::fromValue(area->palette()));
      area->setProperty(originalAutoFillProperty, area->autoFillBackground());
      area->setProperty(
          originalViewportAutoFillProperty, area->viewport()->autoFillBackground());
    }
  }

  // The theme paints the text background, the line numbers and the current
  // line: the overlay variant has no background. It sets the editor's palette.
  for(auto code : editor->findChildren<QCodeEditor*>())
    code->setSyntaxStyle(transparent ? overlayScriptStyle() : scriptStyle());

  // The other text areas (the log) get a transparent base
  for(auto area : editor->findChildren<QAbstractScrollArea*>())
  {
    if(transparent)
    {
      if(!qobject_cast<QCodeEditor*>(area))
      {
        QPalette pal = area->palette();
        pal.setColor(QPalette::Base, Qt::transparent);
        area->setPalette(pal);
      }
      area->setAutoFillBackground(false);
      area->viewport()->setAutoFillBackground(false);
    }
    else
    {
      if(!qobject_cast<QCodeEditor*>(area))
        area->setPalette(area->property(originalPaletteProperty).value<QPalette>());
      area->setAutoFillBackground(area->property(originalAutoFillProperty).toBool());
      area->viewport()->setAutoFillBackground(
          area->property(originalViewportAutoFillProperty).toBool());
    }
  }
}

static constexpr const char* savedFrameProperty = "overlaySavedFrame";
static constexpr const char* savedHScrollProperty = "overlaySavedHScroll";
static constexpr const char* savedVScrollProperty = "overlaySavedVScroll";
static constexpr const char* savedMarginsProperty = "overlaySavedMargins";
static constexpr const char* savedSpacingProperty = "overlaySavedSpacing";
static constexpr const char* savedDocumentModeProperty = "overlaySavedDocumentMode";

void setEditorChromeless(QWidget* editor, bool chromeless)
{
  if(!editor)
    return;
  // Margins of the editor's own layouts
  QList<QLayout*> layouts;
  if(auto lay = editor->layout())
  {
    layouts.push_back(lay);
    layouts.append(lay->findChildren<QLayout*>());
  }
  for(auto lay : layouts)
  {
    if(!lay->property(savedMarginsProperty).isValid())
    {
      lay->setProperty(
          savedMarginsProperty, QVariant::fromValue(lay->contentsMargins()));
      lay->setProperty(savedSpacingProperty, lay->spacing());
    }
    if(chromeless)
    {
      lay->setContentsMargins(0, 0, 0, 0);
      lay->setSpacing(0);
    }
    else
    {
      lay->setContentsMargins(lay->property(savedMarginsProperty).value<QMargins>());
      lay->setSpacing(lay->property(savedSpacingProperty).toInt());
    }
  }

  // Frames and scroll bars of the text areas
  for(auto area : editor->findChildren<QAbstractScrollArea*>())
  {
    if(!area->property(savedFrameProperty).isValid())
    {
      area->setProperty(savedFrameProperty, int(area->frameShape()));
      area->setProperty(savedHScrollProperty, int(area->horizontalScrollBarPolicy()));
      area->setProperty(savedVScrollProperty, int(area->verticalScrollBarPolicy()));
    }
    if(chromeless)
    {
      area->setFrameShape(QFrame::NoFrame);
      area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
      area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
    else
    {
      area->setFrameShape(QFrame::Shape(area->property(savedFrameProperty).toInt()));
      area->setHorizontalScrollBarPolicy(
          Qt::ScrollBarPolicy(area->property(savedHScrollProperty).toInt()));
      area->setVerticalScrollBarPolicy(
          Qt::ScrollBarPolicy(area->property(savedVScrollProperty).toInt()));
    }
  }

  // No button row: Ctrl+Return compiles, Escape closes
  for(auto bbox : editor->findChildren<QDialogButtonBox*>())
    bbox->setVisible(!chromeless);

  // Tabs without a pane frame, drawn as plain labels with an underline
  for(auto tabs : editor->findChildren<QTabWidget*>())
  {
    if(!tabs->property(savedDocumentModeProperty).isValid())
      tabs->setProperty(savedDocumentModeProperty, tabs->documentMode());
    tabs->setDocumentMode(
        chromeless ? true : tabs->property(savedDocumentModeProperty).toBool());
    if(auto bar = qobject_cast<ScriptTabBar*>(tabs->tabBar()))
      bar->setFlat(chromeless);
  }
}

EditorOverlayHost::EditorOverlayHost(QWidget* editor, QWidget* preview, QWidget* parent)
    : QWidget{parent}
    , m_editor{editor}
    , m_preview{preview}
{
  setObjectName("EditorOverlayHost");
  setAttribute(Qt::WA_DeleteOnClose);

  m_layout = new QStackedLayout{this};
  m_layout->setContentsMargins(0, 0, 0, 0);
  m_layout->setStackingMode(QStackedLayout::StackAll);
  m_layout->addWidget(preview);
  m_layout->addWidget(editor);
  // The current widget is the one on top
  m_layout->setCurrentWidget(editor);

  setEditorTransparent(editor, true);
  setEditorChromeless(editor, true);

  // The host goes with its editor. Not close(): that runs hide/layout code
  // over the children while the editor is half-destroyed.
  //
  // Neither handler touches the QPointer of the object being destroyed:
  // resetting it from inside destroyed() frees the object's shared reference
  // count while ~QObject is still using it. The QPointers null themselves
  // once the destruction is complete.
  connect(editor, &QObject::destroyed, this, [this] { deleteLater(); });
  // Deferred: when the host itself is going, its children die together and
  // the editor must not be touched from the preview's destruction. The queued
  // call is dropped with the host.
  connect(preview, &QObject::destroyed, this, [this] {
    QMetaObject::invokeMethod(this, [this] {
      if(m_editor)
      {
        setEditorTransparent(m_editor, false);
        setEditorChromeless(m_editor, false);
      }
    }, Qt::QueuedConnection);
  });
}

EditorOverlayHost::~EditorOverlayHost() { }

QWidget* EditorOverlayHost::releaseEditor()
{
  auto ed = m_editor.data();
  if(!ed)
    return nullptr;

  disconnect(ed, &QObject::destroyed, this, nullptr);
  m_editor = nullptr;
  m_layout->removeWidget(ed);
  ed->setParent(nullptr);
  setEditorTransparent(ed, false);
  setEditorChromeless(ed, false);

  close();
  return ed;
}

void EditorOverlayHost::closeEvent(QCloseEvent* e)
{
  // Closing the tab closes the editor, which resets the process's pointer;
  // the preview is deleted along with the host.
  if(auto ed = m_editor.data())
  {
    disconnect(ed, &QObject::destroyed, this, nullptr);
    m_editor = nullptr;
    ed->close();
  }
  e->accept();
}

DocumentBackgroundPreview::DocumentBackgroundPreview(
    score::DocumentDelegateView& view, QWidget* parent)
    : QWidget{parent}
    , m_view{&view}
{
  setAttribute(Qt::WA_OpaquePaintEvent);
}

// Only refreshes while shown: a central tab that is not current is hidden
void DocumentBackgroundPreview::showEvent(QShowEvent* ev)
{
  QWidget::showEvent(ev);
  if(m_timer != -1)
    return;

  // Every repaint also repaints the editor stacked over it: no faster than
  // the score view refreshes its own background, from the same setting.
  int interval = 16;
  bool ok{};
  const int rate
      = QSettings{}.value(QStringLiteral("Scenario/ExecutionRefreshRate")).toInt(&ok);
  if(ok && rate > 0)
    interval = std::max(interval, 1000 / rate);
  m_timer = startTimer(interval, Qt::CoarseTimer);
}

void DocumentBackgroundPreview::hideEvent(QHideEvent* ev)
{
  QWidget::hideEvent(ev);
  if(m_timer != -1)
  {
    killTimer(m_timer);
    m_timer = -1;
  }
}

DocumentBackgroundPreview::~DocumentBackgroundPreview()
{
  if(m_timer != -1)
    killTimer(m_timer);
}

void DocumentBackgroundPreview::timerEvent(QTimerEvent*)
{
  // Minimising does not hide the widget
  if(auto w = window(); w && w->isMinimized())
    return;
  update();
}

void DocumentBackgroundPreview::paintEvent(QPaintEvent*)
{
  QPainter p{this};
  p.fillRect(rect(), Qt::black);
  if(!m_view)
    return;
  if(auto renderer = m_view->activeBackgroundRenderer())
    renderer->render(&p, rect());
}
}
