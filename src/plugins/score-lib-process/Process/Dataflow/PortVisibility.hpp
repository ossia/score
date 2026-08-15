#pragma once
#include <score_lib_process_export.h>

namespace Process
{
class Port;

//! Above that many controls, a node paginates them instead of laying them all
//! out at once: some plug-ins (VSTs in particular) have thousands of them,
//! which is both unreadable and very slow to render.
static const constexpr int MaxUnpaginatedControls = 32;

//! Upper bound on how many controls a single page shows.
static const constexpr int ControlsPerPage = 20;

//! Rows a column of the control grid holds before the next one starts, i.e.
//! score::GraphicsDefaultLayout's own packing.
static const constexpr int ControlsPerColumn = 5;

//! True for the ports whose value is edited through a widget in the node body.
SCORE_LIB_PROCESS_EXPORT
bool isControlPort(const Process::Port& port) noexcept;

//! True for the ports a folded node still has to show. A folded node displays
//! the routing only: everything that is not a control, plus the controls that
//! take part in the patch - those with a cable or an exposed address.
SCORE_LIB_PROCESS_EXPORT
bool isVisibleWhenFolded(const Process::Port& port) noexcept;

//! Which slice of a node's controls is currently displayed.
struct ControlPage
{
  int page{};       //!< displayed page, clamped to [0, pageCount)
  int pageCount{1}; //!< total number of pages
  int first{};      //!< index of the first displayed control
  int last{};       //!< one past the index of the last displayed control

  bool paginated() const noexcept { return pageCount > 1; }
  int count() const noexcept { return last - first; }

  bool operator==(const ControlPage&) const noexcept = default;
};

//! Pagination for `controlCount` controls: everything on a single page below
//! MaxUnpaginatedControls, up to ControlsPerPage per page above it.
//!
//! `gridOffset` is the number of non-control items sharing the control grid -
//! the leading audio/midi/message inlets when there are too few of them to get
//! a column of their own. The page is shortened so that it ends on a full
//! column instead of leaving one item alone in the last one.
SCORE_LIB_PROCESS_EXPORT
ControlPage
nodeControlPage(int controlCount, int requestedPage, int gridOffset = 0) noexcept;
}
