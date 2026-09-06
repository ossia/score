#pragma once
#include <score/model/path/ObjectPath.hpp>

#include <QWidget>

#include <score_plugin_scenario_export.h>

#include <verdigris>

class QHBoxLayout;
namespace score
{
struct DocumentContext;
}
namespace Scenario
{
class IntervalModel;

/**
 * @brief Clickable path of the interval shown in the central view.
 *
 * Lives in the navigation bar of the document, above the central views:
 * it is available whatever the central view shows (timeline, nodal display,
 * a code editor, a process UI...).
 */
class SCORE_PLUGIN_SCENARIO_EXPORT AddressBarWidget final : public QWidget
{
  W_OBJECT(AddressBarWidget)
public:
  explicit AddressBarWidget(
      const score::DocumentContext& ctx, QWidget* parent = nullptr);
  ~AddressBarWidget() override;

  void setTargetObject(ObjectPath&& path);

public:
  void intervalSelected(IntervalModel* itv)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, intervalSelected, itv)

private:
  void clear();

  const score::DocumentContext& m_ctx;
  QHBoxLayout* m_layout{};
  std::vector<QWidget*> m_items;
  ObjectPath m_currentPath;
};
}
