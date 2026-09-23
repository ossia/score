// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "JSProcessModel.hpp"

#include <State/Expression.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/ExternalFiles.hpp>
#include <Process/PresetHelpers.hpp>

#include <JS/ApplicationPlugin.hpp>
#include <JS/Commands/EditScript.hpp>
#include <JS/Executor/ExecutionHelpers.hpp>
#include <JS/JSProcessMetadata.hpp>
#include <JS/Qml/QmlObjects.hpp>
#include <Library/LibrarySettings.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/MultiOngoingCommandDispatcher.hpp>
#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/Identifier.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/DeleteAll.hpp>
#include <score/tools/File.hpp>

#include <optional>

#include <core/document/Document.hpp>

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QPointer>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>

#include <wobjectimpl.h>

#include <vector>
W_OBJECT_IMPL(JS::ProcessModel)
namespace JS
{
static constexpr const char* default_js_program =
    R"_(import Score
import QtQuick
// This is a minimal example script that showcases the available API.
// View the complete documentation at
// https://ossia.io/score-docs/processes/javascript.html
Script {
  ValueInlet { id: in1; objectName: "Value In" }
  ValueOutlet { id: out1; objectName: "Value Out" }
  FloatSlider { id: sl; min: 10; max: 100; objectName: "Control" }

  // Called on every tick
  tick: function(token, state)
  {
    if (typeof in1.value !== 'undefined')
    {
      console.log(in1.value);
      out1.value = in1.value * mx + sl.value * my;
    }
  }
})_";
static constexpr const char* default_js_ui =
    R"_()_";

ProcessModel::ProcessModel(
    const TimeVal& duration, const QString& data, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{
        duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
{
  if(data.isEmpty())
  {
    (void)setProgram({default_js_program, default_js_ui});
  }
  else
  {
    if(!data.endsWith(".qml")) {
      (void)setProgram({data, {}});
    }
    else {
      m_root = data;
      (void)setProgram(readProgramFromFile(data));
    }
  }

  metadata().setInstanceName(*this);
}

Process::ProcessFlags ProcessModel::flags() const noexcept
{
  auto flags = Metadata<Process::ProcessFlags_k, JS::ProcessModel>::get();
  if(m_ui_component)
    // The UI is a Qt Quick scene in a window container: it can be docked.
    flags |= Process::ExternalUIAvailable | Process::ExternalUIEmbeddable;
  return flags;
}

ProcessModel::~ProcessModel()
{
  if(this->externalUI)
  {
    this->externalUI->close();
    this->externalUI = nullptr;
  }
}

void ProcessModel::mapExternalFiles(Process::ExternalFileMap& map)
{
  Process::ProcessModel::mapExternalFiles(map);

  auto& ctx = score::IDocument::documentContext(*this);

  // The QML import root. Relocating it means collecting a whole include tree,
  // which score does not attempt: report it so the user knows the other
  // machine needs it.
  if(!m_root.isEmpty())
    map.readOnly(m_root, score::FileKind::Script);

  // A script is stored either inline or as a path to a .qml file; only the
  // latter is an external dependency.
  QmlSource next = m_program;
  bool changed = false;
  const auto relocate = [&](QString& script) {
    if(!Process::looksLikeExistingFile(script, ctx))
      return;

    const QString relocated = map.map(
        {.path = script,
         .kind = score::FileKind::Script,
         .usage = Process::FileUsage::Input,
         .directory = false,
         .rewritable = true,
         .owner = map.owner});
    if(relocated.isEmpty())
      return;

    script = relocated;
    changed = true;
  };
  relocate(next.execution);
  relocate(next.ui);

  if(changed)
    map.addCommand(new JS::EditScript{*this, next, ctx});
}

QString ProcessModel::uiPathFor(const QString& qmlPath) noexcept
{
  if(!qmlPath.endsWith(".qml", Qt::CaseInsensitive))
    return {};
  return qmlPath.chopped(3) + QStringLiteral("ui.qml");
}

QmlSource ProcessModel::readProgramFromFile(const QString& qmlPath) noexcept
{
  const auto read = [](const QString& path) -> QString {
    if(path.isEmpty())
      return {};
    QFile f{path};
    if(!f.open(QIODevice::ReadOnly))
      return {};
    return QString::fromUtf8(f.readAll());
  };
  return {read(qmlPath), read(uiPathFor(qmlPath))};
}

void ProcessModel::updateFileLink() noexcept
{
  m_modified = m_root.isEmpty() || m_program != readProgramFromFile(m_root);
}

QString ProcessModel::rootPath() const noexcept
{
  if(!m_root.isEmpty())
  {
    return m_root;
  }
  else
  {
    // Not cached: tests run several applications in one process
    const auto& lib = score::AppContext().settings<Library::Settings::Model>();

    return lib.getDefaultLibraryPath() + QDir::separator() + "Scripts"
           + QDir::separator() + "include" + QDir::separator() + "Script/Script.qml";
  }
}

bool ProcessModel::validate(const std::vector<QString>& script) const noexcept
{
  if(script.empty())
    return false;
  if(script[0].isEmpty())
    return false;

  const auto trimmed = script[0].trimmed();
  const QByteArray data = trimmed.toUtf8();

  auto path = score::locateFilePath(trimmed, score::IDocument::documentContext(*this));

  if(QFileInfo::exists(path))
  {
    return (bool)m_cache.getExecution(*this, path.toUtf8(), true);
  }
  else
  {
    if(!data.startsWith("import"))
      return false;
    return (bool)m_cache.getExecution(*this, data, false);
  }
}


// A script loaded from a .qml file is identified by that file path;
// otherwise by its text
QString ProcessModel::effect() const noexcept
{
  if(!m_root.isEmpty())
    return m_root;
  return m_qmlData;
}

QQuickItem* ProcessModel::createItemForUI(const score::DocumentContext& ctx) const noexcept
{
  if(!m_ui_component)
    return nullptr;
  auto& dummyEngine =  ctx.app
                          .guiApplicationPlugin<JS::ApplicationPlugin>()
                          .m_scriptProcessUIEngine;

  auto obj = m_ui_component->beginCreate(dummyEngine.rootContext());

  if(!obj)
    return nullptr;
  auto script = qobject_cast<ScriptUI*>(obj);
  auto self = const_cast<JS::ProcessModel*>(this);
  if(script) {
    script->setProcess(self);
  }
  m_ui_component->completeCreate();

  if(!script) {
    delete obj;
    return nullptr;
  }

  if(const auto& on_exec = script->executionEvent(); on_exec.isCallable())
  {
    connect(this, &JS::ProcessModel::executionToUi,
            script, [&dummyEngine, on_exec] (const QVariant& v) {
      on_exec.call({dummyEngine.toScriptValue(v)});
    });
  }

  connect(script, &ScriptUI::executionSend, this, [self](const QJSValue& v) {
    self->uiToExecution(v.toVariant());
  });

  struct StateUpdater
  {
    const score::DocumentContext& ctx;
    JS::ProcessModel& self;
    std::unique_ptr<MultiOngoingCommandDispatcher> disp;
    int count = 0;

    void beginUpdateState(const QString& name)
    {
      if(count > 0) {
        count++;
      }
      else {
        disp = std::make_unique<MultiOngoingCommandDispatcher>(ctx.commandStack);
        count = 1;
      }
    }

    void endUpdateState()
    {
      if(!disp)
        return;
      count--;
      if(count > 0)
        return;

      disp->commit<JS::UpdateStateMacro>();
      disp.reset();
      count = 0;
    }

    void updateState(const QString& k, const QJSValue& v)
    {
      beginUpdateState("Update");

      disp->submit<JS::UpdateStateElement>(self, k, ossia::qt::value_from_js(v));

      endUpdateState();
    }

    void cancelUpdateState()
    {
      if(!disp)
        return;

      disp->rollback();
      disp.reset();
      count = 0;
    }

    void clearState()
    {
      beginUpdateState("Clear");

      disp->submit<JS::ReplaceState>(self, JS::JSState{});

      endUpdateState();
    }

    void replaceState(const QJSValue& v)
    {
      beginUpdateState("Replace");

      JS::JSState cur;
      auto var = v.toVariant().toMap();
      for(auto it = var.constBegin(); it != var.constEnd(); ++it) {
        if(it.value().isValid()) {
          cur.insert_or_assign(it.key(), ossia::qt::qt_to_ossia{}(it.value()));
        }
      }
      disp->submit<JS::ReplaceState>(self, std::move(cur));

      endUpdateState();
    }
  };
  auto updater = std::make_shared<StateUpdater>(StateUpdater{ctx, *self});

  connect(script, &ScriptUI::beginUpdateState,
          this, [updater] (const QString& name) {
    updater->beginUpdateState(name);
  });
  connect(script, &ScriptUI::updateState,
          this, [updater] (const QString& name, const QJSValue& v) {
    updater->updateState(name, v);
  });
  connect(script, &ScriptUI::endUpdateState,
          this, [updater] () {
    updater->endUpdateState();
  });
  connect(script, &ScriptUI::cancelUpdateState,
          this, [updater] () {
    updater->cancelUpdateState();
  });
  connect(script, &ScriptUI::clearState,
          this, [updater] () {
    updater->clearState();
  });
  connect(script, &ScriptUI::replaceState,
          this, [updater] (const QJSValue& v) {
    updater->replaceState(v);
  });

  if(const auto& on_stateUpdated = script->stateUpdated(); on_stateUpdated.isCallable())
  {
    connect(
        this, &JS::ProcessModel::stateElementChanged, script,
        [on_stateUpdated, &dummyEngine](const QString& k, const ossia::value& v) {
      if(v.valid())
      {
        if(auto res = v.apply(ossia::qt::ossia_to_qvariant{}); res.isValid())
          on_stateUpdated.call({k, dummyEngine.toScriptValue(res)});
        else
          on_stateUpdated.call({k, QJSValue{}});
      }
      else
        on_stateUpdated.call({k, QJSValue{}});
    }, Qt::QueuedConnection);
  }

  if(const auto& on_load = script->loadState(); on_load.isCallable())
  {
    QVariantMap vm;
    for(auto& [k, v]: this->m_state) {
      if(auto res = v.apply(ossia::qt::ossia_to_qvariant{}); res.isValid())
        vm[k] = std::move(res);
    }
    on_load.call({dummyEngine.toScriptValue(vm)});
  }

  return script;
}

QWidget* ProcessModel::createWindowForUI(const score::DocumentContext& ctx,
                                         QWidget* parent) const noexcept
{
  m_ui_object = createItemForUI(ctx);
  if(!m_ui_object)
    return nullptr;

  // A close is deferred: by the time this runs the process may show a new
  // UI, which must be left alone. The container pointer is only compared,
  // the widget may be half-destroyed.
  const auto cleanup_ui = [this](QQuickWindow* win, QWidget* container) {
    if(m_ui_object && m_ui_object->window() == win)
    {
      delete m_ui_object;
      m_ui_object = nullptr;
    }

    if(externalUI == container)
    {
      const_cast<QWidget*&>(externalUI) = nullptr;
      externalUIVisible(false);
    }
  };

  // Let the UI take the whole window: needed when the window is docked in
  // the main window and follows the size of its pane.
  const auto fitToWindow = [](QQuickWindow* win, QQuickItem* item) {
    if(win && item)
      item->setSize(QSizeF(win->width(), win->height()));
  };

  // On a new compile the UI object is recreated; put it back in the window
  const auto reloadUI = [this, &ctx, fitToWindow](QQuickWindow* win) -> bool {
    // A window closed and waiting for its deletion while a new one holds
    // the UI: nothing to do in it
    if(m_ui_object && m_ui_object->window() != win)
      return true;

    delete m_ui_object;
    m_ui_object = nullptr;
    if(!m_ui_component)
      return false;

    m_ui_object = createItemForUI(ctx);
    if(!m_ui_object)
      return false;
    m_ui_object->setParentItem(win->contentItem());
    m_ui_object->setParent(win->contentItem());
    fitToWindow(win, m_ui_object);
    return true;
  };

  // The requested size: the ScriptUI root asks for one through its implicit
  // width / height (e.g. `implicitWidth: 1280`), defaulting to 640x640.
  const QSize requested = [this] {
    int w = 640, h = 640;
    if(m_ui_object->implicitWidth() >= 100.)
      w = static_cast<int>(m_ui_object->implicitWidth());
    if(m_ui_object->implicitHeight() >= 100.)
      h = static_cast<int>(m_ui_object->implicitHeight());
    return QSize{w, h};
  }();

  auto win = new QQuickWindow{};
  // QWidget gets these from QWidgetPrivate::adjustFlags; a bare QQuickWindow
  // does not, and on platforms where Qt draws the chrome itself (wasm) that
  // leaves the window with no title bar, close or minimise button.
  win->setFlags(
      win->flags() | Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint
      | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint
      | Qt::WindowMaximizeButtonHint);
#if defined(__EMSCRIPTEN__)
  // Qt for wasm reports ShowIsFullScreen unconditionally, so QWindow::show()
  // turns into showFullScreen() for every top level: the requested size is
  // discarded and the full-screen state suppresses the frame. Only Qt::Dialog
  // and Qt::Popup opt out (QWasmIntegration::defaultWindowState).
  win->setFlags(win->flags() | Qt::Dialog);
#endif
  win->setWidth(640);
  win->setHeight(640);
  win->setColor(qApp->palette().color(QPalette::Window));

  m_ui_object->setParentItem(win->contentItem());
  connect(win, &QQuickWindow::widthChanged, this, [this, win, fitToWindow] {
    fitToWindow(win, m_ui_object);
  });
  connect(win, &QQuickWindow::heightChanged, this, [this, win, fitToWindow] {
    fitToWindow(win, m_ui_object);
  });

  auto widg = QWidget::createWindowContainer(win, parent);
  if(!widg) {
    delete m_ui_object;
    m_ui_object = nullptr;
    delete win;
    return nullptr;
  }
  widg->setAttribute(Qt::WA_DeleteOnClose);
  // The container widget does not follow the QQuickWindow's size
  widg->resize(requested);

#if QT_VERSION >= QT_VERSION_CHECK(6,8,2)
  // Bug in older Qt 6 versions:
  // QtCore/qmetatype.h:842:23: error: invalid application of 'sizeof' to an incomplete type 'QQuickCloseEvent'
  // static_assert(sizeof(T), "Type argument of Q_PROPERTY or Q_DECLARE_METATYPE(T*) must be fully defined");
  connect(win, &QQuickWindow::closing, this, [cleanup_ui, win, widg] {
    cleanup_ui(win, widg);
  });
#endif
  connect(win, &QQuickWindow::destroyed, this, [cleanup_ui, win, widg] {
    cleanup_ui(win, widg);
  });
  connect(
      this, &JS::ProcessModel::uiScriptOk, win,
      [reloadUI, win, container = QPointer<QWidget>{widg}] {
    if(!reloadUI(win))
    {
      // The container is what the main window holds: closing it takes the
      // window with it and runs the cleanup from destroyed()
      if(container)
        container->close();
    }
  });

  // No externalUIVisible(true) here: setupExternalUI emits it for every
  // process type once the UI is placed, and only when one is actually created
  // -- asking again for an open UI raises it instead. Emitting a second time
  // from this path would make a re-request report {true, true, false}.
  return widg;
}

void ProcessModel::setExecutionScript(const QString& f)
{
  if(f == m_program.execution)
    return;
  m_program.execution = std::move(f);

  executionScriptChanged(m_program.execution);
}

void ProcessModel::setUiScript(const QString& f)
{
  if(f == m_program.ui)
    return;
  m_program.ui = std::move(f);

  uiScriptChanged(m_program.ui);
}

void ProcessModel::setState(const JSState &s)
{
  if(s == m_state)
    return;

  {
    const auto prev = std::move(m_state);
    for(auto& [prev_k, prev_v] : prev) {
      stateElementChanged(prev_k, prev_v);
    }
  }

  m_state = std::move(s);
  for(auto& [k, v] : m_state) {
    stateElementChanged(k, v);
  }

  stateChanged();
}

void ProcessModel::updateState(const QString &k, const ossia::value& res)
{
  if(auto it = m_state.find(k); it != m_state.end())
  {
    if(res.valid())
    {
      if(res != it->second)
      {
        // Updating a new element
        m_state[k] = res;
        stateElementChanged(k, res);
        stateChanged();
      }
    }
    else
    {
      // Removing an element
      m_state.erase(k);
      stateElementChanged(k, res);
      stateChanged();
    }
  }
  else
  {
    if(res.valid())
    {
      // Adding a new element
      m_state[k] = res;
      stateElementChanged(k, res);
      stateChanged();
    }
    else
    {
      // Already not there, nothing to do
    }
  }
}

[[nodiscard]] Process::ScriptChangeResult ProcessModel::setProgram(const JS::QmlSource& script)
{
  setExecutionScript(script.execution);
  setUiScript(script.ui);

  Process::ScriptChangeResult res;
  const auto trimmed = script.execution.trimmed();
  const QByteArray data = trimmed.toUtf8();

  auto path = score::locateFilePath(trimmed, score::IDocument::documentContext(*this));

  // setQmlData builds the UI component from m_program.ui: it must be the new
  // one, otherwise every compile shows the UI of the previous compile.
  const auto previous = m_program;
  m_program = script;

  if(QFileInfo::exists(path))
    res = setQmlData(path.toUtf8(), true);
  else
    res = setQmlData(data, false);

  if(!res.valid)
    m_program = previous;
  updateFileLink();
  return res;
}

Process::ScriptChangeResult ProcessModel::setQmlData(const QByteArray& data, bool isFile)
{
  Process::ScriptChangeResult res;
  if(!isFile && !data.contains("import "))
    return res;

  auto script = m_cache.getExecution(*this, data, isFile);
  if(!script)
    return res;

  m_isFile = isFile;
  m_qmlData = data;

  res.inlets = score::clearAndDeleteLater(m_inlets);
  res.outlets = score::clearAndDeleteLater(m_outlets);
  const bool had_ui = m_ui_component;
  m_ui_component = nullptr;
  delete m_ui_object;
  m_ui_object = nullptr;

  SCORE_ASSERT(m_inlets.size() == 0);
  SCORE_ASSERT(m_outlets.size() == 0);

  // Check inlets / outlets
  {
    auto cld_inlet = script->findChildren<Inlet*>();
    int i = 0;
    for(auto n : cld_inlet)
    {
      auto port = n->make(Id<Process::Port>(i++), this);
      if(const auto& name = n->objectName(); !name.isEmpty())
        port->setName(name);
      if(auto addr = State::parseAddressAccessor(n->address()))
        port->setAddress(std::move(*addr));
      m_inlets.push_back(port);
    }
  }

  {
    auto cld_outlet = script->findChildren<Outlet*>();
    int i = 0;
    for(auto n : cld_outlet)
    {
      auto port = n->make(Id<Process::Port>(i++), this);
      if(const auto& name = n->objectName(); !name.isEmpty())
        port->setName(name);
      if(auto addr = State::parseAddressAccessor(n->address()))
        port->setAddress(std::move(*addr));
      m_outlets.push_back(port);
    }
  }

  // Create ui if any
  if(!this->m_program.ui.isEmpty()) {
    m_ui_component = m_cache.getUi(*this, this->m_program.ui.toUtf8(), isFile);
  }

  if(m_isFile)
  {
    const auto name = QFileInfo{data}.baseName();
    metadata().setName(name);
    metadata().setLabel(name);
  }
  else if(metadata().getName().isEmpty())
  {
    metadata().setName(QStringLiteral("Script"));
  }

  executionScriptOk();
  res.valid = true;

  if(bool(m_ui_component) != had_ui)
    flagsChanged();

  if(m_ui_component)
  {
    uiScriptOk();
  }
  else if(externalUI)
  {
    externalUI->close();
    externalUI->deleteLater();
    externalUI = nullptr;
    externalUIVisible(false);
  }

  // inlets / outletsChanged : in ScriptEditCommand
  return res;
}

Script* ProcessModel::currentExecutionObject() const noexcept
{
  if(auto cache = m_cache.tryGet(m_qmlData, m_isFile))
    return cache->object.get();
  return nullptr;
}

bool ProcessModel::isGpu() const noexcept
{
#if defined(SCORE_HAS_GPU_JS)
  if(auto script = currentExecutionObject())
  {
    return
        script->findChild<JS::TextureInlet*>() != nullptr
           || script->findChild<JS::TextureOutlet*>() != nullptr
           // || script->findChild<JS::BufferInlet*>() != nullptr
           // || script->findChild<JS::BufferOutlet*>() != nullptr
        ;
  }
#endif
  return false;
}

ComponentCache::ComponentCache() { }
ComponentCache::~ComponentCache() { }

QByteArray ComponentCache::key(const QByteArray& str, bool isFile) noexcept
{
  if(!isFile)
    return str;

  // What is cached is a compiled script, so what identifies it is the text it
  // was compiled from -- not the name of the file that happened to hold it,
  // which says nothing about whether that file still says the same thing.
  QFile f{QString::fromUtf8(str)};
  if(!f.open(QIODevice::ReadOnly))
    return {};
  return f.readAll();
}

const ComponentCache::Cache* ComponentCache::tryGet(const QByteArray& str, bool isFile) const noexcept
{
  const QByteArray content = key(str, isFile);
  if(content.isEmpty())
    return nullptr;

  if(auto it = ossia::find_if(m_map, [&](const auto& k) { return k.key == content; });
     it != m_map.end())
  {
    return &*it;
  }
  return nullptr;
}

Script* ComponentCache::getExecution(
    const ProcessModel& process, const QByteArray& str, bool isFile) noexcept
{
  if(auto cache = tryGet(str, isFile))
    return cache->object.get();

  auto& dummyEngine = score::GUIAppContext()
                          .guiApplicationPlugin<JS::ApplicationPlugin>()
                          .m_scriptProcessUIEngine;
  std::unique_ptr<QQmlComponent> comp;
  if(!isFile)
  {
    comp = std::make_unique<QQmlComponent>(&dummyEngine);
    loadJSObjectFromString(process.rootPath(), str, *comp, false);
  }
  else
  {
    comp = std::make_unique<QQmlComponent>(&dummyEngine);
    loadJSObjectFromFile(QString::fromUtf8(str), *comp);
  }

  const auto& errs = comp->errors();
  if(!errs.empty())
  {
    const auto& err = errs.first();
    qDebug() << err.line() << err.toString();
    auto str = err.toString();
    str.remove("<Unknown File>:");
    process.errorMessage(/* err.line(), */str);
    return nullptr;
  }

  auto obj = comp->create();
  auto script = qobject_cast<JS::Script*>(obj);
  if(script)
  {
    if(m_map.size() > 5)
      m_map.erase(m_map.begin());

    m_map.emplace_back(
        Cache{key(str, isFile), std::move(comp), std::unique_ptr<JS::Script>(script)});
    return script;
  }
  else
  {
    process.errorMessage(/* 0, */"The component must be of type Script");
    if(obj)
    {
      delete obj;
    }
    return nullptr;
  }
}

QQmlComponent* ComponentCache::getUi(
    const ProcessModel& process, const QByteArray& str, bool isFile) noexcept
{
  if(auto cache = tryGet(str, isFile))
    return cache->component.get();

  auto& dummyEngine = score::GUIAppContext()
                          .guiApplicationPlugin<JS::ApplicationPlugin>()
                          .m_scriptProcessUIEngine;

  std::unique_ptr<QQmlComponent> comp;
  if(!isFile)
  {
    comp = std::make_unique<QQmlComponent>(&dummyEngine);
    loadJSObjectFromString(process.rootPath(), str, *comp, true);
  }
  else
  {
    comp = std::make_unique<QQmlComponent>(&dummyEngine);
    loadJSObjectFromFile(QString::fromUtf8(str), *comp);
  }

  const auto& errs = comp->errors();
  if(!errs.empty())
  {
    const auto& err = errs.first();
    qDebug() << err.line() << err.toString();
    auto str = err.toString();
    str.remove("<Unknown File>:");
    process.errorMessage(/* err.line(), */str);
    return nullptr;
  }

  auto obj = comp->beginCreate(dummyEngine.rootContext());
  if(!obj) {
    process.errorMessage(/* 0, */"Cannot create UI object");
    return nullptr;
  }
  auto script = qobject_cast<ScriptUI*>(obj);
  if(script) {
    script->setProcess((Process::ProcessModel*)&process);
  }
  comp->completeCreate();
  if(script)
  {
    if(m_map.size() > 5)
      m_map.erase(m_map.begin());

    m_map.emplace_back(
        Cache{key(str, isFile), std::move(comp), {}});
    delete script;
    return m_map.back().component.get();
  }
  else
  {
    process.errorMessage(/* 0, */"The component must be of type Script");
    if(obj)
      delete obj;
    return nullptr;
  }
}

// Preset data: {"Controls": [...], "State": [...]} where "State" is the
// script state set through Script.replaceState, plus the program: "Root"
// (source .qml file, also used for includes) and "Script" / "Ui" when they
// differ from that file. A plain array of controls is also accepted; its
// script is then the effect of the preset key.
void ProcessModel::loadPreset(const Process::Preset& preset)
{
  const rapidjson::Document doc = readJson(preset.data);
  const bool object = doc.IsObject();
  if(!object && !doc.IsArray())
    return;

  std::optional<QmlSource> program;
  if(object)
  {
    QString root;
    if(auto it = doc.FindMember("Root"); it != doc.MemberEnd() && it->value.IsString())
      root = score::locateFilePath(
          QString::fromUtf8(it->value.GetString(), it->value.GetStringLength()),
          score::IDocument::documentContext(*this));
    m_root = root;

    if(auto it = doc.FindMember("Script"); it != doc.MemberEnd() && it->value.IsString())
    {
      QmlSource p{
          QString::fromUtf8(it->value.GetString(), it->value.GetStringLength()), {}};
      if(auto ui = doc.FindMember("Ui"); ui != doc.MemberEnd() && ui->value.IsString())
        p.ui = QString::fromUtf8(ui->value.GetString(), ui->value.GetStringLength());
      program = std::move(p);
    }
    else if(!root.isEmpty())
    {
      program = readProgramFromFile(root);
    }
  }
  if(!program && !preset.key.effect.isEmpty())
    program = QmlSource{preset.key.effect, m_program.ui};

  if(program && *program != m_program)
    (void)setProgram(*program);
  else
    updateFileLink();

  if(!object)
  {
    Process::loadFixedControls(doc.GetArray(), *this);
    return;
  }

  if(auto it = doc.FindMember("Controls"); it != doc.MemberEnd() && it->value.IsArray())
    Process::loadFixedControls(it->value.GetArray(), *this);

  JSState st;
  if(auto it = doc.FindMember("State"); it != doc.MemberEnd())
    st <<= JsonValue{it->value};
  setState(st);
}

Process::Preset ProcessModel::savePreset() const noexcept
{
  auto p = Process::saveScriptProcessPreset(*this, effect());

  JSONReader r;
  r.stream.StartObject();
  if(!m_root.isEmpty())
    r.obj["Root"] = score::relativizeFilePath(m_root);
  if(!followsRootFile())
  {
    r.obj["Script"] = m_program.execution;
    if(!m_program.ui.isEmpty())
      r.obj["Ui"] = m_program.ui;
  }
  r.stream.Key("Controls");
  Process::saveFixedControls(r, *this);
  r.obj["State"] = m_state;
  r.stream.EndObject();
  p.data = r.toByteArray();
  return p;
}

// Also matches presets that identify a file-based script by its text
bool ProcessModel::presetMatches(const Process::Preset& preset) const noexcept
{
  return preset.key.key == concreteKey()
         && (preset.key.effect == effect() || preset.key.effect == QString{m_qmlData});
}

}
