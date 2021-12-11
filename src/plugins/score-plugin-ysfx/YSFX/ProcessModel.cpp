// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "ProcessModel.hpp"

#include "YSFX/ProcessMetadata.hpp"
#include "YSFX/ApplicationPlugin.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/PresetHelpers.hpp>
#include <State/Expression.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/model/Identifier.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/DeleteAll.hpp>
#include <score/tools/File.hpp>

#include <core/document/Document.hpp>

#include <QDebug>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QQmlComponent>

#include <wobjectimpl.h>

#include <vector>
W_OBJECT_IMPL(YSFX::ProcessModel)
namespace YSFX
{
ProcessModel::ProcessModel(
    const TimeVal& duration,
    const QString& data,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{
        duration,
        id,
        Metadata<ObjectKey_k, ProcessModel>::get(),
        parent}
{
  setScript(data);
  metadata().setInstanceName(*this);
}

ProcessModel::~ProcessModel() { }

void ProcessModel::setScript(const QString& script)
{
  auto arr = script.toStdString();
  auto& config = score::GUIAppContext().applicationPlugin<YSFX::ApplicationPlugin>().config;
  ysfx_guess_file_roots(config.get(), arr.c_str());

  fx.reset(ysfx_new(config.get()), ysfx_u_deleter{});
  if (!ysfx_load_file(fx.get(), arr.c_str(), 0))
      return ;

  uint32_t compile_opts = 0;
  if (!ysfx_compile(fx.get(), compile_opts))
      return;

  auto name = ysfx_get_name(fx.get());
  metadata().setName(name);

  if(ysfx_get_num_inputs(fx.get()) > 0)
  {
    auto inl = new Process::AudioInlet{Id<Process::Port>{0}, this};
    this->m_inlets.push_back(inl);
  }
  if(ysfx_get_num_outputs(fx.get()) > 0)
  {
    auto outl = new Process::AudioOutlet{Id<Process::Port>{1}, this};
    outl->setPropagate(true);
    this->m_outlets.push_back(outl);
  }

  {
    for (uint32_t i = 0; i < ysfx_max_sliders; ++i)
    {
      if (!ysfx_slider_exists(fx.get(), i))
          continue;

      //const bool is_visible = ysfx_slider_is_initially_visible(fx.get(), i);
      if (ysfx_slider_is_enum(fx.get(), i))
      {
        std::vector<std::string> values;
        ossia::value init;
        QString name = ysfx_slider_get_name(fx.get(), i);

        uint32_t count = ysfx_slider_get_enum_size(fx.get(), i);
        std::vector<const char *> names(count);
        ysfx_slider_get_enum_names(fx.get(), i, names.data(), count);
        for(const char* val : names)
          values.push_back(val);

        auto slider = new Process::Enum{values, {}, values[0], name, Id<Process::Port>(2 + i), this};
        slider->setName(ysfx_slider_get_name(fx.get(), i));

        this->m_inlets.push_back(slider);
      }
      else if(ysfx_slider_is_path(fx.get(), i))
      {
        auto slider = new Process::LineEdit{Id<Process::Port>(2 + i), this};
        slider->setName(ysfx_slider_get_name(fx.get(), i));
        this->m_inlets.push_back(slider);
      }
      else
      {
        auto slider = new Process::FloatSlider{Id<Process::Port>(2 + i), this};
        slider->setName(ysfx_slider_get_name(fx.get(), i));

        ysfx_slider_range_t range{};
        ysfx_slider_get_range(fx.get(), i, &range);

        slider->setDomain(ossia::make_domain(range.min, range.max));
        slider->setValue(range.def);
        // TODO increment

        this->m_inlets.push_back(slider);
      }
    }
  }

 // const auto trimmed = script.trimmed();
 // const QByteArray data = trimmed.toUtf8();
 //
 // auto path = score::locateFilePath(
 //     trimmed, score::IDocument::documentContext(*this));
 //
 // if (QFileInfo{path}.exists())
 // {
 //   /* Disabling the watch feature for now :
 //    * it does not fix the cables, etc.
 //   m_watch = std::make_unique<QFileSystemWatcher>(QStringList{trimmed});
 //   connect(
 //       m_watch.get(),
 //       &QFileSystemWatcher::fileChanged,
 //       this,
 //       [=](const QString& path) {
 //         // Note:
 //         //
 //   https://stackoverflow.com/questions/18300376/qt-qfilesystemwatcher-signal-filechanged-gets-emited-only-once
 //         QTimer::singleShot(20, this, [this, path] {
 //           m_watch->addPath(path);
 //           QFile f(path);
 //           if (f.open(QIODevice::ReadOnly))
 //           {
 //             setQmlData(path.toUtf8(), true);
 //             m_watch->addPath(path);
 //           }
 //         });
 //       });
 //
 //   */
 //   if (!setQmlData(path.toUtf8(), true))
 //     return;
 // }
 // else
 // {
 //   if (!setQmlData(data, false))
 //     return;
 // }
 //
 // m_script = script;
 // scriptChanged(script);

  /*
  if (!isFile && !data.startsWith("import"))
    return false;

  auto script = m_cache.get(*this, data, isFile);
  if (!script)
    return false;

  m_isFile = isFile;
  m_qmlData = data;

  auto old_inlets = score::clearAndDeleteLater(m_inlets);
  auto old_outlets = score::clearAndDeleteLater(m_outlets);

  SCORE_ASSERT(m_inlets.size() == 0);
  SCORE_ASSERT(m_outlets.size() == 0);

  {
    auto cld_inlet = script->findChildren<Inlet*>();
    int i = 0;
    for (auto n : cld_inlet)
    {
      auto port = n->make(Id<Process::Port>(i++), this);
      port->setName(n->objectName());
      if (auto addr = State::parseAddressAccessor(n->address()))
        port->setAddress(std::move(*addr));
      m_inlets.push_back(port);
    }
  }

  {
    auto cld_outlet = script->findChildren<Outlet*>();
    int i = 0;
    for (auto n : cld_outlet)
    {
      auto port = n->make(Id<Process::Port>(i++), this);
      port->setName(n->objectName());
      if (auto addr = State::parseAddressAccessor(n->address()))
        port->setAddress(std::move(*addr));
      m_outlets.push_back(port);
    }
  }

  if (m_isFile)
  {
    const auto name = QFileInfo{data}.baseName();
    metadata().setName(name);
    metadata().setLabel(name);
  }
  else if (metadata().getName().isEmpty())
  {
    metadata().setName(QStringLiteral("Script"));
  }

  scriptOk();

  qmlDataChanged(data);
  inletsChanged();
  outletsChanged();

  */}

Script* ProcessModel::currentObject() const noexcept
{
  return nullptr;//m_cache.tryGet(m_qmlData, m_isFile);
}


QString ProcessModel::script() const noexcept
{
  return m_script;
}

}
