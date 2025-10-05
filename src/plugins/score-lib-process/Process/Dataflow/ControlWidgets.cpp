#include <Process/Dataflow/ControlWidgets.hpp>

namespace Process
{
PortItemLayout DefaultControlLayouts::knob() noexcept
{
  return Process::PortItemLayout{
      .port = QPointF{0., 17.},
      .control = QPointF{10., 6.},
      .labelAlignment = Qt::AlignCenter};
}

PortItemLayout DefaultControlLayouts::slider() noexcept
{
  return Process::PortItemLayout{
      .port = QPointF{0., 10.},
      .control = QPointF{12., 12.},
      .labelAlignment = Qt::AlignCenter};
}

PortItemLayout DefaultControlLayouts::bargraph() noexcept
{
  return Process::PortItemLayout{
      .port = QPointF{60, 10.},
      .control = QPointF{0., 12.},
      .labelAlignment = Qt::AlignCenter,
      .controlAlignment = Qt::AlignRight};
}

PortItemLayout DefaultControlLayouts::combo() noexcept
{
  return Process::PortItemLayout{
      .port = QPointF{0., 17.},
      .control = QPointF{12., 12.},
      .labelAlignment = Qt::AlignCenter};
}

PortItemLayout DefaultControlLayouts::list() noexcept
{
  return {};
}

PortItemLayout DefaultControlLayouts::lineedit() noexcept
{
  return {};
}

PortItemLayout DefaultControlLayouts::spinbox() noexcept
{
  return slider();
}

PortItemLayout DefaultControlLayouts::toggle() noexcept
{
  return Process::PortItemLayout{
      .port = QPointF{0., 4.}, .label = QPointF{30., 4.}, .control = QPointF{10., 0.}};
}

PortItemLayout DefaultControlLayouts::pad() noexcept
{
  return {};
}

PortItemLayout DefaultControlLayouts::bang() noexcept
{
  return Process::PortItemLayout{
      .port = QPointF{0., 5.}, .label = QPointF{30., 4.}, .control = QPointF{10., 0.}};
}

PortItemLayout DefaultControlLayouts::button() noexcept
{
  return Process::PortItemLayout{
      .port = QPointF{0., 17.},
      .control = QPointF{12., 12.},
      .labelAlignment = Qt::AlignCenter};
}

PortItemLayout DefaultControlLayouts::chooser_toggle() noexcept
{
  return Process::PortItemLayout{
      .port = QPointF{0., 4.}, .control = QPointF{10., 0.}, .labelVisible = false};
}
}
