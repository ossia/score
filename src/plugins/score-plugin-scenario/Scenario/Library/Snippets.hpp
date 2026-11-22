#pragma once
#include <QPointF>
#include <QString>

#include <score_plugin_scenario_export.h>

#include <functional>
#include <optional>

class QWidget;
class QMenu;
namespace score
{
struct DocumentContext;
}
namespace Process
{
class Cable;
}
namespace Scenario
{
class ScenarioPresenter;
class IntervalModel;
class ProcessModel;
class StateModel;

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

//! The snippet at `path` in a new box starting from `state`'s event, at
//! the height `y`: what the menu of a state, event or time sync adds. The
//! state itself starts it when nothing starts from it yet.
SCORE_PLUGIN_SCENARIO_EXPORT
bool insertSnippetAfter(
    const score::DocumentContext& ctx, const Scenario::ProcessModel& scenario,
    const StateModel& state, double y, const QString& path);

//! Adds the processes of every box of the snippet at `path` to `interval`,
//! with the cables between them: what the node view shows of a snippet. Their
//! nodes keep their layout, its top-left corner at `nodePos`.
SCORE_PLUGIN_SCENARIO_EXPORT
bool insertSnippetInInterval(
    const score::DocumentContext& ctx, const IntervalModel& interval, QPointF nodePos,
    const QString& path);

//! The snippet at `path` put on `cable`, in the interval of its source: its
//! processes in between the two ends, as a process dropped on a cable.
SCORE_PLUGIN_SCENARIO_EXPORT
bool insertSnippetInCable(
    const score::DocumentContext& ctx, const Process::Cable& cable, const QString& path);

//! The snippets of the library in `menu`, their folders as sub-menus: picking
//! one calls `insert` with its file.
SCORE_PLUGIN_SCENARIO_EXPORT
void fillSnippetsMenu(QMenu& menu, std::function<void(const QString&)> insert);
}
