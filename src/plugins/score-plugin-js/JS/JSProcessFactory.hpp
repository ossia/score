#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/Script/ScriptEditor.hpp>
#include <Process/Script/MultiScriptEditor.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>
#include <JS/JSProcessMetadata.hpp>
#include <JS/JSProcessModel.hpp>

namespace JS
{
struct LanguageSpec
{
  static constexpr const char* language = "JS";
};

// Scan the first lines of a QML script for a `// @documentation <url>`
// comment so a preset can point F1 to its own help page.
inline QUrl documentationUrlFromScript(const QString& script) noexcept
{
  static const QString tag = QStringLiteral("// @documentation");
  int line = 0;
  for(const QString& raw : script.split('\n'))
  {
    if(++line > 20)
      break;
    const QString trimmed = raw.trimmed();
    if(trimmed.startsWith(tag))
    {
      const QString url = trimmed.mid(tag.size()).trimmed();
      if(!url.isEmpty())
        return QUrl(url);
    }
  }
  return {};
}

class ProcessFactory final : public Process::ProcessFactory_T<JS::ProcessModel>
{
public:
  using Process::ProcessFactory_T<JS::ProcessModel>::descriptor;
  Process::Descriptor descriptor(const Process::ProcessModel& m) const noexcept override
  {
    auto desc = Metadata<Process::Descriptor_k, JS::ProcessModel>::get();
    const auto& js = safe_cast<const JS::ProcessModel&>(m);
    if(auto url = documentationUrlFromScript(js.executionScript()); !url.isEmpty())
      desc.documentationLink = url;
    return desc;
  }
};
struct LayerFactory : Process::EffectLayerFactory_Base
{
  using Model_T = JS::ProcessModel;
  using ScriptView_T = Process::ProcessMultiScriptEditDialog<ProcessModel, ProcessModel::p_program>;

  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model_T>::get();
  }

  QWidget* makeScriptUI(
      Process::ProcessModel& proc, const score::DocumentContext& ctx,
      QWidget* parent) const final override
  {
    try
    {
      return new ScriptView_T{safe_cast<Model_T&>(proc), ctx, parent};
    }
    catch(...)
    {
    }
    return nullptr;
  }


  bool hasExternalUI(
      const Process::ProcessModel& proc,
      const score::DocumentContext& ctx) const noexcept override
  {
    return ((Model_T&)proc).hasExternalUI();
  }

  QWidget* makeExternalUI(
      Process::ProcessModel& proc, const score::DocumentContext& ctx,
      QWidget* parent) const final override
  {
    try
    {
      return safe_cast<Model_T&>(proc).createWindowForUI(ctx, parent);
    }
    catch(...)
    {
    }
    return nullptr;
  }

  bool matches(const UuidKey<Process::ProcessModel>& p) const override
  {
    return p == Metadata<ConcreteKey_k, Model_T>::get();
  }
};
}
