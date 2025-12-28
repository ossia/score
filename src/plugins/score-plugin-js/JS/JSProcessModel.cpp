// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "JSProcessModel.hpp"

#include "JS/JSProcessMetadata.hpp"

#include <State/Expression.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/PresetHelpers.hpp>

#include <JS/ApplicationPlugin.hpp>
#include <JS/Executor/ExecutionHelpers.hpp>
#include <JS/Qml/QmlObjects.hpp>
#include <Library/LibrarySettings.hpp>

#include <JS/Commands/EditScript.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/MultiOngoingCommandDispatcher.hpp>
#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/Identifier.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/DeleteAll.hpp>
#include <score/tools/File.hpp>

#include <core/document/Document.hpp>

#include <QStandardPaths>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>

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
      auto path = data;
      QFile f{path};
      QString exec_data;
      if(f.open(QIODevice::ReadOnly))
        exec_data = f.readAll();

      path.resize(data.size() - 3);
      path.append("ui.qml");
      QString ui_data;
      if(QFile ui_f{path}; ui_f.open(QIODevice::ReadOnly)) {
        ui_data = ui_f.readAll();
      }
      (void)setProgram({exec_data, ui_data});
    }
  }

  metadata().setInstanceName(*this);
}

Process::ProcessFlags ProcessModel::flags() const noexcept
{
  auto flags = Metadata<Process::ProcessFlags_k, JS::ProcessModel>::get();
  if(m_ui_component)
    flags |= Process::ExternalUIAvailable; // FIXME set in every relevant process
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


QString ProcessModel::effect() const noexcept
{
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

  connect(script, &ScriptUI::executionSend,
          this, [self] (const QJSValue& v) {
    self->uiToExecution(v.toVariant());
  });

  /*
  struct StateUpdateDispatcher : public SingleOngoingCommandDispatcher<UpdateState>
  {
    void submitUpdate(JS::ProcessModel& proc, const QString& k, const QJSValue& v)
    {
      if(!m_cmd)
      {
        stack().disableActions();
        m_cmd = std::make_unique<UpdateState>(proc, k, v);
      }

      {
        m_cmd->update(proc, k, v);
      }
      m_cmd->redo(stack().context());
    }
    void submitClear(JS::ProcessModel& proc, const QString& k, const QJSValue& v)
    {
      if(!m_cmd)
      {
        stack().disableActions();
        m_cmd = std::make_unique<UpdateState>(proc, k, v);
        m_cmd->redo(stack().context());
      }
      else
      {
        m_cmd->update(proc, k, v);
        m_cmd->redo(stack().context());
      }
    }

  };
*/
  struct StateUpdater {
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
    connect(this, &JS::ProcessModel::stateElementChanged,
            script, [this, on_stateUpdated, &dummyEngine] (const QString& k, const ossia::value& v) {
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

  auto win = new QQuickWindow{};
  win->setWidth(640);
  win->setHeight(640);

  m_ui_object->setParentItem(win->contentItem());

  auto widg = QWidget::createWindowContainer(win, parent);
  if(!widg) {
    delete m_ui_object;
    delete win;
    return nullptr;
  }

  connect(win, &QQuickWindow::closing, this, [this] { if(m_ui_object) delete m_ui_object; m_ui_object = nullptr; });
  connect(win, &QQuickWindow::destroyed, this, [this] { if(m_ui_object) delete m_ui_object; m_ui_object = nullptr; });
  connect(this, &JS::ProcessModel::executionScriptOk,
          win, [this,  win, &ctx] () mutable {
    delete m_ui_object;
    m_ui_object = nullptr;
    if(!m_ui_component) {
      win->close();
      win->deleteLater();
      return;
    }

    m_ui_object = createItemForUI(ctx);
    if(!m_ui_object)
      return;
    m_ui_object->setParentItem(win->contentItem());
    m_ui_object->setParent(win->contentItem());
  });
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

  if(QFileInfo::exists(path))
  {
    if(res = setQmlData(path.toUtf8(), true); !res.valid)
      return res;
  }
  else
  {
    if(res = setQmlData(data, false); !res.valid)
      return res;
  }

  m_program = script;
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

const ComponentCache::Cache* ComponentCache::tryGet(const QByteArray& str, bool isFile) const noexcept
{
  QByteArray content;
  QFile f;
  if(isFile)
  {
    f.setFileName(str);
    if(f.open(QIODevice::ReadOnly))
      content = score::mapAsByteArray(f);
    else
      return nullptr;
  }
  else
  {
    content = str;
  }

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
    loadJSObjectFromString(str, *comp, false);
  }
  else
  {
    comp = std::make_unique<QQmlComponent>(&dummyEngine, QUrl::fromLocalFile(str));
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
        Cache{str, std::move(comp), std::unique_ptr<JS::Script>(script)});
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
    loadJSObjectFromString(str, *comp, true);
  }
  else
  {
    comp = std::make_unique<QQmlComponent>(&dummyEngine, QUrl::fromLocalFile(str));
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
        Cache{str, std::move(comp), {}});
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

void ProcessModel::loadPreset(const Process::Preset& preset)
{
  Process::loadScriptProcessPreset<ProcessModel::p_program>(*this, preset);
}

Process::Preset ProcessModel::savePreset() const noexcept
{
  // FIXME this should save p_program
  return Process::saveScriptProcessPreset(*this, this->m_qmlData);
}

}
