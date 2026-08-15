#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortVisibility.hpp>

#include <algorithm>

namespace Process
{
bool isControlPort(const Process::Port& port) noexcept
{
  return qobject_cast<const Process::ControlInlet*>(&port)
         || qobject_cast<const Process::ControlOutlet*>(&port);
}

bool isVisibleWhenFolded(const Process::Port& port) noexcept
{
  if(!isControlPort(port))
    return true;
  if(!port.cables().empty())
    return true;
  return port.address() != State::AddressAccessor{};
}

ControlPage
nodeControlPage(int controlCount, int requestedPage, int gridOffset) noexcept
{
  ControlPage res;
  controlCount = std::max(controlCount, 0);

  if(controlCount <= MaxUnpaginatedControls)
  {
    res.last = controlCount;
    return res;
  }

  // Land on a column boundary: whatever already sits in the grid shifts where
  // the boundaries fall.
  gridOffset = std::max(gridOffset, 0);
  const int ragged = (ControlsPerPage + gridOffset) % ControlsPerColumn;
  const int perPage = std::max(1, ControlsPerPage - ragged);

  res.pageCount = 1 + (controlCount - 1) / perPage;
  res.page = std::clamp(requestedPage, 0, res.pageCount - 1);
  res.first = res.page * perPage;
  res.last = std::min(controlCount, res.first + perPage);
  return res;
}
}
