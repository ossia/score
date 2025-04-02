#pragma once
#include <Process/ProcessMetadata.hpp>
#include <Process/TimeValue.hpp>

#include <score/model/Identifier.hpp>
#include <score/plugins/Interface.hpp>

#include <score_lib_process_export.h>

class QGraphicsItem;
class QObject;
struct VisitorVariant;
namespace score
{
struct DocumentContext;
class ResizeableItem;
}
namespace Process
{
class LayerPresenter;
class HeaderDelegate;
class FooterDelegate;
class LayerView;
class MiniLayer;
class ProcessModel;
struct Preset;
struct Context;

/**
 * @brief The ProcessFactory class
 *
 * Interface to make processes, like Scenario, Automation...
 */
class SCORE_LIB_PROCESS_EXPORT ProcessModelFactory : public score::InterfaceBase
{
  SCORE_INTERFACE(ProcessModel, "507ae654-f3b8-4aae-afc3-7ab8e1a3a86f")
public:
  ~ProcessModelFactory() override;

  virtual QString prettyName() const noexcept = 0;
  virtual QString category() const noexcept = 0;
  virtual ProcessFlags flags() const noexcept = 0;
  virtual Descriptor descriptor(QString) const noexcept = 0;
  virtual Descriptor descriptor(const Process::ProcessModel&) const noexcept;

  virtual QString customConstructionData() const noexcept;

  virtual Process::ProcessModel* make(
      const TimeVal& duration, const QString& data, const Id<ProcessModel>& id,
      const score::DocumentContext& ctx, QObject* parent)
      = 0;

  virtual Process::ProcessModel*
  load(const VisitorVariant&, const score::DocumentContext& ctx, QObject* parent)
      = 0;
};

class SCORE_LIB_PROCESS_EXPORT LayerFactory : public score::InterfaceBase
{
  SCORE_INTERFACE(ProcessModel, "aeee61e4-89aa-42ec-aa33-bf4522ed710b")
public:
  static const constexpr bool ui_interface = true;
  ~LayerFactory() override;

  virtual std::optional<double> recommendedHeight() const noexcept;

  virtual Process::LayerPresenter* makeLayerPresenter(
      const Process::ProcessModel&, Process::LayerView*, const Process::Context& context,
      QObject* parent) const;

  virtual Process::LayerView* makeLayerView(
      const Process::ProcessModel&, const Process::Context& context,
      QGraphicsItem* parent) const;

  virtual Process::MiniLayer*
  makeMiniLayer(const Process::ProcessModel&, QGraphicsItem* parent) const;

  virtual score::ResizeableItem* makeItem(
      const Process::ProcessModel&, const Process::Context& ctx,
      QGraphicsItem* parent) const;

  virtual bool hasCodeEditor(
      const Process::ProcessModel& proc,
      const score::DocumentContext& ctx) const noexcept;
  virtual QWidget* makeCodeEditor(
      const Process::ProcessModel&, const score::DocumentContext& ctx,
      QWidget* parent) const;

  virtual bool hasExternalUI(
      const Process::ProcessModel& proc,
      const score::DocumentContext& ctx) const noexcept;
  virtual QWidget* makeExternalUI(
      const Process::ProcessModel&, const score::DocumentContext& ctx,
      QWidget* parent) const;

  virtual HeaderDelegate* makeHeaderDelegate(
      const ProcessModel& model, const Process::Context& ctx,
      QGraphicsItem* parent) const;
  virtual FooterDelegate*
  makeFooterDelegate(const ProcessModel& model, const Process::Context& ctx) const;

  bool matches(const Process::ProcessModel& p) const;
  virtual bool matches(const UuidKey<Process::ProcessModel>&) const = 0;
};
}
