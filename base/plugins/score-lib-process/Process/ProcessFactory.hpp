#pragma once
#include <Process/ProcessMetadata.hpp>
#include <Process/TimeValue.hpp>
#include <score/model/Identifier.hpp>
#include <score/plugins/customfactory/FactoryInterface.hpp>
#include <score_lib_process_export.h>

class QGraphicsItem;
class QObject;
struct VisitorVariant;
namespace score
{
struct DocumentContext;
class RectItem;
}
namespace Process
{
class LayerPresenter;
class HeaderDelegate;
class LayerView;
class MiniLayer;
class ProcessModel;
struct ProcessPresenterContext;

/**
 * @brief The ProcessFactory class
 *
 * Interface to make processes, like Scenario, Automation...
 */
class SCORE_LIB_PROCESS_EXPORT ProcessModelFactory
    : public score::InterfaceBase
{
  SCORE_INTERFACE(ProcessModel, "507ae654-f3b8-4aae-afc3-7ab8e1a3a86f")
public:
  ~ProcessModelFactory() override;

  virtual QString prettyName() const = 0;
  virtual QString category() const = 0;
  virtual QStringList tags() const = 0;
  virtual ProcessFlags flags() const = 0;

  virtual QString customConstructionData() const;

  virtual Process::ProcessModel* make(
      const TimeVal& duration,
      const QString& data,
      const Id<ProcessModel>& id,
      QObject* parent)
      = 0;

  virtual Process::ProcessModel* load(const VisitorVariant&, QObject* parent)
      = 0;
};

class SCORE_LIB_PROCESS_EXPORT LayerFactory
    : public score::InterfaceBase
{
  SCORE_INTERFACE(ProcessModel, "aeee61e4-89aa-42ec-aa33-bf4522ed710b")
public:
  ~LayerFactory() override;

  virtual Process::LayerPresenter* makeLayerPresenter(
      const Process::ProcessModel&,
      Process::LayerView*,
      const Process::ProcessPresenterContext& context,
      QObject* parent) const;

  virtual Process::LayerView*
  makeLayerView(const Process::ProcessModel&, QGraphicsItem* parent) const;

  virtual Process::MiniLayer*
  makeMiniLayer(const Process::ProcessModel&, QGraphicsItem* parent) const;

  virtual QGraphicsItem* makeItem(
      const Process::ProcessModel&,
      const score::DocumentContext& ctx,
      score::RectItem* parent) const;

  virtual bool hasExternalUI(
      const Process::ProcessModel& proc,
      const score::DocumentContext& ctx) const;
  virtual QWidget* makeExternalUI(
      const Process::ProcessModel&,
      const score::DocumentContext& ctx,
      QWidget* parent) const;

  virtual HeaderDelegate*
  makeHeaderDelegate(const Process::LayerPresenter& pres) const;

  bool matches(const Process::ProcessModel& p) const;
  virtual bool matches(const UuidKey<Process::ProcessModel>&) const = 0;
};
}
