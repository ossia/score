#pragma once
#include "widget.hpp"
#include <score/widgets/Layout.hpp>
#include <score/widgets/SignalUtils.hpp>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
namespace fxd
{

/// Inspectors

#define INSPECTOR_FACTORY(Factory, InspModel, InspObject, Uuid)               \
  class Factory final : public Inspector::InspectorWidgetFactory              \
  {                                                                           \
    SCORE_CONCRETE(Uuid)                                                      \
  public:                                                                     \
    Factory() = default;                                                      \
                                                                              \
    QWidget* make(                                                            \
        const InspectedObjects& sourceElements,                               \
        const score::DocumentContext& doc,                                    \
        QWidget* parent) const override                                       \
    {                                                                         \
      return new InspObject{                                                  \
          safe_cast<const InspModel&>(*sourceElements.first()), doc, parent}; \
    }                                                                         \
                                                                              \
    bool matches(const InspectedObjects& objects) const override              \
    {                                                                         \
      return dynamic_cast<const InspModel*>(objects.first());                 \
    }                                                                         \
  };

class WidgetInspector : public QWidget
{
public:
  WidgetInspector(
      const Widget& sc,
      const score::DocumentContext& doc,
      QWidget* parent);
};

class DocumentInspector : public QWidget
{
public:
  DocumentInspector(
      const DocumentModel& sc,
      const score::DocumentContext& doc,
      QWidget* parent);
};
INSPECTOR_FACTORY(
    WidgetInspectorFactory,
    fxd::Widget,
    fxd::WidgetInspector,
    "a703d205-3488-4f0e-817d-19fac52e2046")

INSPECTOR_FACTORY(
    DocumentInspectorFactory,
    fxd::DocumentModel,
    fxd::DocumentInspector,
    "fa22746e-39a9-4a42-92da-531e91fcd3af")
}
