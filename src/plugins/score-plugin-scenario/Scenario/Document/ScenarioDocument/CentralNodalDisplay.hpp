#pragma once

#include <Scenario/Document/DisplayedElements/DisplayedElementsPresenter.hpp>

namespace Library
{
struct ProcessData;
}
namespace Scenario
{
class ScenarioDocumentPresenter;
class NodalIntervalView;
class CentralNodalDisplay
{
public:
  explicit CentralNodalDisplay(ScenarioDocumentPresenter& p);
  ~CentralNodalDisplay();

  void recenter();

  void on_addProcessFromLibrary(const Library::ProcessData& dat);
  void on_visibleRectChanged(const QRectF&);
  void on_executionTimer();

  ScenarioDocumentPresenter& parent;
  NodalIntervalView* presenter{};
};
}
