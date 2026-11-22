#pragma once
#include <QtGlobal>

//! How a NodeItem draws a process: the title above its position, the ports
//! beside its content, one every PortSpacing.
namespace Process::NodeItemGeometry
{
inline constexpr qreal TitleHeight = 20.;
inline constexpr qreal PortSpacing = 10.;
inline constexpr qreal SideWidth = 10.;
}
