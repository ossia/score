#pragma once

#include <Process/TimeValue.hpp>
#include <QByteArray>
#include <QString>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore_lib_process_export.h>

class QGraphicsItem;
class QObject;
struct VisitorVariant;
namespace iscore
{
struct DocumentContext;
struct RelativePath;
}
namespace Process
{
class LayerPresenter;
class LayerView;
class ProcessModel;
class LayerPanelProxy;
struct ProcessPresenterContext;

/**
     * @brief The ProcessFactory class
     *
     * Interface to make processes, like Scenario, Automation...
     */

class ISCORE_LIB_PROCESS_EXPORT ProcessModelFactory
    : public iscore::Interface<ProcessModel>
{
  ISCORE_INTERFACE("507ae654-f3b8-4aae-afc3-7ab8e1a3a86f")
public:
  virtual ~ProcessModelFactory();

  virtual QString prettyName() const = 0;

  virtual Process::ProcessModel*
  make(const TimeVal& duration, const Id<ProcessModel>& id, QObject* parent)
      = 0;

  virtual Process::ProcessModel* load(const VisitorVariant&, QObject* parent)
      = 0;
};

class ISCORE_LIB_PROCESS_EXPORT LayerFactory
    : public iscore::Interface<ProcessModel>
{
  ISCORE_INTERFACE("aeee61e4-89aa-42ec-aa33-bf4522ed710b")
public:
  virtual ~LayerFactory();

  // TODO Make it take a view name, too (cf. logical / temporal).
  // Or make it be created by the ViewModel, and the View be created by the
  // presenter.
  virtual Process::LayerPresenter* makeLayerPresenter(
      const Process::ProcessModel&,
      Process::LayerView*,
      const Process::ProcessPresenterContext& context,
      QObject* parent);

  virtual Process::LayerView*
  makeLayerView(const Process::ProcessModel& view, QGraphicsItem* parent);

  virtual Process::LayerPanelProxy*
  makePanel(const ProcessModel& layer, QObject* parent);

  bool matches(const Process::ProcessModel& p) const;
  virtual bool matches(const UuidKey<Process::ProcessModel>&) const = 0;

protected:
};
}
