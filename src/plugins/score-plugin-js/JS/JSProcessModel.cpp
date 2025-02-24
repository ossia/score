// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "JSProcessModel.hpp"

#include "JS/JSProcessMetadata.hpp"

#include <State/Expression.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/PresetHelpers.hpp>

#include <JS/Qml/QmlObjects.hpp>
#include <Library/LibrarySettings.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/Identifier.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/DeleteAll.hpp>
#include <score/tools/File.hpp>

#include <core/document/Document.hpp>

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QQmlComponent>
#include <QQuickWindow>

#include <wobjectimpl.h>

#include <mutex>
#include <vector>
W_OBJECT_IMPL(JS::ProcessModel)
namespace JS
{

void setupEngineImportPaths(QQmlEngine& eng) noexcept
{
  for(auto& p :
      score::AppContext().settings<Library::Settings::Model>().getIncludePaths())
  {
    eng.addImportPath(p);
  }
}

ProcessModel::ProcessModel(
    const TimeVal& duration, const QString& data, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{
        duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
{
  if(data.isEmpty())
  {
    setScript(
        R"_(import Score 1.0
Script {
  ValueInlet { id: in1 }
  ValueOutlet { id: out1 }
  FloatSlider { id: sl; min: 10; max: 100; }

  tick: function(token, state) {
    if (typeof in1.value !== 'undefined') {
      console.log(in1.value);
      out1.value = in1.value + sl.value * Math.random();
    }
  }
  start: function() { }
  stop: function() { }
  pause: function() { }
  resume: function() { }
})_");
  }
  else
  {
    setScript(data);
  }

  setUiScript(R"_(import QtQuick
import QtQuick3D

    View3D {
        id: view
        anchors.fill: parent

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
    }
)_");
  metadata().setInstanceName(*this);
}

ProcessModel::~ProcessModel() { }

bool ProcessModel::validate(const QString& script) const noexcept
{
  const auto trimmed = script.trimmed();
  const QByteArray data = trimmed.toUtf8();

  auto path = score::locateFilePath(trimmed, score::IDocument::documentContext(*this));

  if(QFileInfo::exists(path))
  {
    return (bool)m_cache.get(*this, path.toUtf8(), true);
  }
  else
  {
    if(!data.startsWith("import"))
      return false;
    return (bool)m_cache.get(*this, data, false);
  }
}

[[nodiscard]] Process::ScriptChangeResult
ProcessModel::setUiScript(const QString& script)
{
  Process::ScriptChangeResult res;
  const auto trimmed = script.trimmed();
  const QByteArray data = trimmed.toUtf8();

  if(res = setUiQmlData(data); !res.valid)
    return res;

  m_ui_script = script;
  uiScriptChanged(script);
  res.valid = true;
  return res;
}

QString ProcessModel::effect() const noexcept
{
  return m_qmlData;
}

Process::ScriptChangeResult ProcessModel::setUiQmlData(const QByteArray& data)
{
  Process::ScriptChangeResult res;
  if(!data.contains("import QtQuick"))
    return res;

  auto script = m_cache.getUi(*this, data);
  if(!script)
    return res;

  m_ui_qmlData = data;
  res.valid = true;
  // FIXME signal?

  auto win = new QQuickWindow;
  win->setWidth(640);
  win->setHeight(640);
  script->setParentItem(win->contentItem());
  win->show();
  return res;
}

[[nodiscard]] Process::ScriptChangeResult ProcessModel::setScript(const QString& script)
{
  Process::ScriptChangeResult res;
  const auto trimmed = script.trimmed();
  const QByteArray data = trimmed.toUtf8();

  auto path = score::locateFilePath(trimmed, score::IDocument::documentContext(*this));

  if(QFileInfo{path}.exists())
  {
    if(res = setQmlData(path.toUtf8(), true); !res.valid)
      return res;
  }
  else
  {
    if(res = setQmlData(data, false); !res.valid)
      return res;
  }

  m_script = script;
  scriptChanged(script);
  return res;
}

Process::ScriptChangeResult ProcessModel::setQmlData(const QByteArray& data, bool isFile)
{
  Process::ScriptChangeResult res;
  if(!isFile && !data.contains("import "))
    return res;

  auto script = m_cache.get(*this, data, isFile);
  if(!script)
    return res;

  m_isFile = isFile;
  m_qmlData = data;

  res.inlets = score::clearAndDeleteLater(m_inlets);
  res.outlets = score::clearAndDeleteLater(m_outlets);

  SCORE_ASSERT(m_inlets.size() == 0);
  SCORE_ASSERT(m_outlets.size() == 0);

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

  scriptOk();
  res.valid = true;

  // inlets / outletsChanged : in ScriptEditCommand
  return res;
}

Script* ProcessModel::currentObject() const noexcept
{
  return m_cache.tryGet(m_qmlData, m_isFile);
}

QQuickItem* ProcessModel::currentUI() const noexcept
{
  return m_cache.tryGet(m_ui_qmlData);
}

bool ProcessModel::isGpu() const noexcept
{
#if defined(SCORE_HAS_GPU_JS)
  if(auto script = currentObject())
  {
    return script->findChild<JS::TextureOutlet*>() != nullptr;
  }
#endif
  return false;
}

ComponentCache::ComponentCache() { }
ComponentCache::~ComponentCache() { }

Script* ComponentCache::tryGet(const QByteArray& str, bool isFile) const noexcept
{
  QByteArray content;
  QFile f{str};
  if(isFile)
  {
    f.open(QIODevice::ReadOnly);
    content = score::mapAsByteArray(f);
  }
  else
  {
    content = str;
  }

  auto it = ossia::find_if(m_map, [&](const auto& k) { return k.key == content; });
  if(it != m_map.end())
  {
    return it->object.get();
  }
  else
  {
    return nullptr;
  }
}

QQuickItem* ComponentCache::tryGet(const QByteArray& str) const noexcept
{
  auto it = ossia::find_if(m_map, [&](const auto& k) { return k.key == str; });
  if(it != m_map.end())
  {
    return it->item.get();
  }
  else
  {
    return nullptr;
  }
}

static std::once_flag qml_dummy_engine_setup{};
Script* ComponentCache::get(
    const ProcessModel& process, const QByteArray& str, bool isFile) noexcept
{
  QFile f;
  QByteArray content;
  if(isFile)
  {
    f.setFileName(str);
    f.open(QIODevice::ReadOnly);
    content = score::mapAsByteArray(f);
  }
  else
  {
    content = str;
  }

  auto it = ossia::find_if(m_map, [&](const auto& k) { return k.key == content; });
  if(it != m_map.end())
  {
    return it->object.get();
  }
  else
  {
    static QQmlEngine dummyEngine;
    std::call_once(qml_dummy_engine_setup, [] { setupEngineImportPaths(dummyEngine); });

    auto comp = std::make_unique<QQmlComponent>(&dummyEngine);
    if(!isFile)
    {
      auto& lib = score::AppContext().settings<Library::Settings::Model>();
      // FIXME QTBUG-107204
      QString path = lib.getDefaultLibraryPath() + QDir::separator() + "Scripts"
                     + QDir::separator() + "include" + QDir::separator() + "Script.qml";
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
      process.errorMessage(err.line(), str);
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
      process.errorMessage(0, "The component must be of type Script");
      if(obj)
      {
        delete obj;
      }
      return nullptr;
    }
  }
}
QQuickItem*
ComponentCache::getUi(const ProcessModel& process, const QByteArray& str) noexcept
{
  auto it = ossia::find_if(m_map, [&](const auto& k) { return k.key == str; });
  if(it != m_map.end())
  {
    return it->item.get();
  }
  else
  {
    static QQmlEngine dummyEngine;
    std::call_once(qml_dummy_engine_setup, [] { setupEngineImportPaths(dummyEngine); });

    auto comp = std::make_unique<QQmlComponent>(&dummyEngine);
    {
      auto& lib = score::AppContext().settings<Library::Settings::Model>();
      // FIXME QTBUG-107204
      QString path = lib.getDefaultLibraryPath() + QDir::separator() + "Scripts"
                     + QDir::separator() + "include" + QDir::separator() + "Script.qml";
      comp->setData(str, QUrl::fromLocalFile(path));
    }
    const auto& errs = comp->errors();
    if(!errs.empty())
    {
      const auto& err = errs.first();
      qDebug() << err.line() << err.toString();
      auto str = err.toString();
      str.remove("<Unknown File>:");
      process.errorMessage(err.line(), str);
      return nullptr;
    }

    auto obj = comp->create();
    auto script = qobject_cast<QQuickItem*>(obj);
    if(script)
    {
      if(m_map.size() > 5)
        m_map.erase(m_map.begin());

      m_map.emplace_back(
          Cache{str, std::move(comp), {}, std::unique_ptr<QQuickItem>(script)});
      return script;
    }
    else
    {
      process.errorMessage(0, "The component must be of type Script");
      if(obj)
        delete obj;
      return nullptr;
    }
  }
}

void ProcessModel::loadPreset(const Process::Preset& preset)
{
  Process::loadScriptProcessPreset<ProcessModel::p_script>(*this, preset);
}

Process::Preset ProcessModel::savePreset() const noexcept
{
  return Process::saveScriptProcessPreset(*this, this->m_qmlData);
}

}
