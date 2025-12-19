// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "JSProcessModel.hpp"

#include "JS/JSProcessMetadata.hpp"

#include <State/Expression.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/PresetHelpers.hpp>

#include <JS/ApplicationPlugin.hpp>
#include <JS/Qml/QmlObjects.hpp>
#include <Library/LibrarySettings.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/Identifier.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/DeleteAll.hpp>
#include <score/tools/File.hpp>
#if __has_include(<boost/hash2/xxh3.hpp>)
#include <boost/hash2/xxh3.hpp>
#include <boost/algorithm/hex.hpp>
#endif

#include <core/document/Document.hpp>

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
Script {
  ValueInlet { id: in1; objectName: "Value In" }
  ValueOutlet { id: out1; objectName: "Value Out" }
  FloatSlider { id: sl; min: 10; max: 100; objectName: "Control" }

  // Called on every tick
  tick: function(token, state) {
    if (typeof in1.value !== 'undefined') {
      console.log(in1.value);
      out1.value = in1.value + sl.value * Math.random();
      messageToUi(123);
    }
  }

  // Use these to handle specific execution events if necessary:
  // start: function() { }
  // stop: function() { }
  // pause: function() { }
  // resume: function() { }

  // Handling UI events
  uiEvent: function(message) {
    console.log(message);
  }
})_";
static constexpr const char* default_js_ui =
    R"_(import QtQuick
import QtQuick3D

View3D {
  id: view
  anchors.fill: parent
  MouseArea {
    anchors.fill: view
    onClicked: { console.log("oki"); messageToExecution(mouseX); }
  }

  property var model;

  signal messageToExecution(var message);
  function executionEvent(message) {
    console.log("in ui: " + message);
  }

  //! [environment]
  environment: SceneEnvironment {
      clearColor: "skyblue"
      backgroundMode: SceneEnvironment.Color
  }
  //! [environment]

  //! [camera]
  PerspectiveCamera {
      position: Qt.vector3d(0, 200, 300)
      eulerRotation.x: -30
  }
  //! [camera]

  //! [light]
  DirectionalLight {
      eulerRotation.x: -30
      eulerRotation.y: -70
  }
  //! [light]

  //! [objects]
  Model {
      position: Qt.vector3d(0, -200, 0)
      source: "#Cylinder"
      scale: Qt.vector3d(2, 0.2, 1)
      materials: [ DefaultMaterial {
              diffuseColor: "red"
          }
      ]
  }

  Model {
      position: Qt.vector3d(0, 150, 0)
      source: "#Sphere"

      materials: [ DefaultMaterial {
              diffuseColor: "blue"
          }
      ]

      //! [animation]
      SequentialAnimation on y {
          loops: Animation.Infinite
          NumberAnimation {
              duration: 3000
              to: -150
              from: 150
              easing.type:Easing.InQuad
          }
          NumberAnimation {
              duration: 3000
              to: 150
              from: -150
              easing.type:Easing.OutQuad
          }
      }
      //! [animation]
  }
  //! [objects]
})_";

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
    (void)setProgram({data, {}}); // FIXME .ui.qml ?
  }

  metadata().setInstanceName(*this);
}

ProcessModel::~ProcessModel() { }

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

void listSignals(QObject* obj) {
  const QMetaObject* metaObj = obj->metaObject();

  qDebug() << "Signals for" << metaObj->className() << ":";

  // Iterate through all methods
  for (int i = metaObj->methodOffset(); i < metaObj->methodCount(); ++i) {
    QMetaMethod method = metaObj->method(i);

    // Filter for signals only
    if (method.methodType() == QMetaMethod::Signal) {
      qDebug() << "  " << method.methodSignature()
      << "- Parameters:" << method.parameterNames()
      << "- Types:" << method.parameterTypes();
    }
  }
}
QWidget* ProcessModel::createWindowForUI(const score::DocumentContext& ctx,
                                         QWidget* parent) const noexcept
{
  if(!m_ui_component)
    return nullptr;

  auto obj = m_ui_component->create();
  if(!obj)
    return nullptr;

  auto script = qobject_cast<QQuickItem*>(obj);
  if(!script) {
    delete obj;
    return nullptr;
  }

  connect(this, &JS::ProcessModel::executionToUi,
          script, [script] (const QVariant& v) {
    QMetaObject::invokeMethod(script, "executionEvent", v);
  });

  {
    const QMetaObject* uiMetaObj = obj->metaObject();
    for (int i = uiMetaObj->methodOffset(); i < uiMetaObj->methodCount(); ++i) {
      QMetaMethod method = uiMetaObj->method(i);

      if (method.methodType() == QMetaMethod::Signal) {
        if(method.name() == "messageToExecution") {
          connect(script, method,
                  this, QMetaMethod::fromSignal(&ProcessModel::uiToExecution));
          break;
        }
      }
    }
  }


  auto win = new QQuickWindow{};
  win->setWidth(640);
  win->setHeight(640);

  script->setParentItem(win->contentItem());

  auto widg = QWidget::createWindowContainer(win, parent);
  if(!widg) {
    delete script;
    delete win;
    return nullptr;
  }
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
  programChanged();
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

static QString hashFileData(const QByteArray& str)
{
  QString hexName;
#if __has_include(<boost/hash2/xxh3.hpp>)
  boost::hash2::xxh3_128 hasher;
  hasher.update(str.constData(), str.size());
  const auto result = hasher.result();
  std::string hexString;
  boost::algorithm::hex(result.begin(), result.end(), std::back_inserter(hexString));

  hexName.reserve(32);
  hexName.push_back("-");
  hexName.append(hexString.data());
#endif
  return hexName;
}

Script* ComponentCache::getExecution(
    const ProcessModel& process, const QByteArray& str, bool isFile) noexcept
{
  if(auto cache = tryGet(str, isFile))
    return cache->object.get();

  auto& dummyEngine = score::GUIAppContext()
                          .guiApplicationPlugin<JS::ApplicationPlugin>()
                          .m_dummyEngine;
  auto comp = std::make_unique<QQmlComponent>(&dummyEngine);
  if(!isFile)
  {
    // FIXME QTBUG-107204
    auto& lib = score::AppContext().settings<Library::Settings::Model>();

    QString path = lib.getDefaultLibraryPath() + QDir::separator() + "Scripts"
                   + QDir::separator() + "include" + QDir::separator() + "Script" + hashFileData(str) + ".qml";
    comp->setData(str, QUrl::fromLocalFile(path));
  }
  else
  {
    comp->loadUrl(QUrl::fromLocalFile(str));
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
                          .m_dummyEngine;
  auto comp = std::make_unique<QQmlComponent>(&dummyEngine);
  if(!isFile)
  {
    // FIXME QTBUG-107204
    auto& lib = score::AppContext().settings<Library::Settings::Model>();

    QString path = lib.getDefaultLibraryPath() + QDir::separator() + "Scripts"
                   + QDir::separator() + "include" + QDir::separator() + "Script" + hashFileData(str) + ".ui.qml";
    comp->setData(str, QUrl::fromLocalFile(path));
  }
  else
  {
    comp->loadUrl(QUrl::fromLocalFile(str));
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
  auto script = qobject_cast<QQuickItem*>(obj);
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
    {
      delete obj;
    }
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
