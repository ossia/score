#pragma once
#include <QPointF>
#include <QString>

#include <score_plugin_scenario_export.h>

#include <optional>

class QWidget;
namespace score
{
struct DocumentContext;
}
namespace Scenario
{
class ScenarioPresenter;

//! Saves what is selected in the focused timeline as a snippet of the user
//! library, under a name the user gives ("Folder/Name" makes a folder).
//! Returns the file written, if any.
SCORE_PLUGIN_SCENARIO_EXPORT
std::optional<QString>
saveSelectionAsSnippet(const score::DocumentContext& ctx, QWidget* parent);

//! Writes a snippet file from the timeline content `json` (the copy format):
//! asks before replacing one of the same name.
SCORE_PLUGIN_SCENARIO_EXPORT
std::optional<QString> writeSnippet(
    const QByteArray& json, const QString& libraryPath, const QString& name,
    QWidget* parent);

//! Pastes the snippet at `path` in the timeline of `pres`, its start at `pos`
//! in the scenario view's coordinates.
SCORE_PLUGIN_SCENARIO_EXPORT
bool insertSnippet(const ScenarioPresenter& pres, QPointF pos, const QString& path);
}
