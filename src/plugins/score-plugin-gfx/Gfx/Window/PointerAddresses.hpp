#pragma once

#include <Gfx/GfxExecContext.hpp>
#include <Gfx/GfxInputDevice.hpp>
#include <Gfx/Graph/PointerState.hpp>
#include <Gfx/Graph/TouchHistoryNode.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/generic/generic_node.hpp>

#include <memory>
#include <vector>

namespace Gfx
{
/**
 * The pointer addresses the window devices publish next to cursor/scaled,
 * cursor/gl and cursor/absolute (Window/WindowDevice.hpp,
 * Window/OffscreenDevice.hpp):
 *
 * - `cursor/clip`: the mouse in clip space, x and y in [-1, 1], y up: the
 *   `mouse` of vertexshaderart (greggman/vertexshaderart src/js/main.js
 *   recordMouseMove, `x * 2 - 1`, `y * -2 + 1`).
 * - `touch/points`: the pointers, a list of 32 [x, y, pressure] in the same
 *   space. Slot 0 is the mouse or the first finger, then one slot per finger in
 *   the order they came down; a released slot keeps its position, its pressure
 *   drops to 0 (main.js addTouchPosition / addTouchPressure / getTouchIndex).
 *   Writable: anything that writes it moves the pointers.
 * - `touch/history`: a texture, vertexshaderart's `touch` -- the last 240
 *   frames of touch/points, 32x240 (score::gfx::TouchHistoryNode).
 *
 * A shader input named `mouse` or `touch` is bound to these by default
 * (ISFProcess.hpp), as any other address, and can be rebound or cabled.
 */
struct pointer_addresses
{
  ossia::net::parameter_base* clip{};
  ossia::net::parameter_base* points{};
  //! What touch/history records; touch/points writes it.
  std::shared_ptr<score::gfx::PointerState> pointers
      = std::make_shared<score::gfx::PointerState>();

  //! cursor/clip, under an existing cursor node.
  void add_clip(ossia::net::device_base& dev, ossia::net::node_base& cursor)
  {
    auto node = std::make_unique<ossia::net::generic_node>("clip", dev, cursor);
    clip = node->create_parameter(ossia::val_type::VEC2F);
    clip->set_domain(ossia::make_domain(-1.f, 1.f));
    clip->push_value(ossia::vec2f{0.f, 0.f});
    ossia::net::set_description(*node, "Clip space, [-1, 1], y up");
    cursor.add_child(std::move(node));
  }

  //! touch/points and touch/history, under @p root.
  void add_touch(
      ossia::net::device_base& dev, ossia::net::node_base& root,
      GfxExecutionAction* context)
  {
    auto touch = std::make_unique<ossia::net::generic_node>("touch", dev, root);
    {
      auto node = std::make_unique<ossia::net::generic_node>("points", dev, *touch);
      points = node->create_parameter(ossia::val_type::LIST);
      ossia::net::set_description(
          *node, "32 pointers, [x, y, pressure] in clip space, y up; slot 0 is the "
                 "mouse or first finger");
      points->set_value_quiet(to_value(pointers->snapshot()));
      points->add_callback([p = pointers](const ossia::value& v) { from_value(*p, v); });
      touch->add_child(std::move(node));
    }
    if(context)
    {
      auto node = std::make_unique<ossia::net::generic_node>("history", dev, *touch);
      ossia::net::set_description(
          *node, "vertexshaderart's touch history: the last 240 frames of "
                 "touch/points, 32x240 RGBA32F (x, y, pressure, time), newest at v = 0");
      node->set_parameter(std::make_unique<simple_texture_input_parameter>(
          new score::gfx::TouchHistoryNode{pointers}, context, *node));
      touch->add_child(std::move(node));
    }
    root.add_child(std::move(touch));
  }

  //! Publish the window's decoded pointers on touch/points.
  void publish(const score::gfx::PointerState& state)
  {
    if(points)
      points->push_value(to_value(state.snapshot()));
  }

  //! Move slot @p slot of touch/points, keeping its pressure, for outputs
  //! that see the mouse but decode no touch events.
  void move_slot(int slot, float x, float y)
  {
    if(!points || slot < 0 || slot >= score::gfx::PointerState::slots)
      return;
    const ossia::value cur = points->value();
    std::vector<ossia::value> l;
    if(auto* p = cur.target<std::vector<ossia::value>>())
      l = *p;
    l.resize(score::gfx::PointerState::slots, ossia::vec3f{0.f, 0.f, 0.f});
    float pressure = 0.f;
    if(auto* p = l[slot].target<ossia::vec3f>())
      pressure = (*p)[2];
    l[slot] = ossia::vec3f{x, y, pressure};
    points->push_value(std::move(l));
  }

  void set_clip(float x, float y)
  {
    if(clip)
      clip->push_value(ossia::vec2f{x, y});
  }

  static ossia::value
  to_value(const std::array<score::gfx::PointerState::Slot, score::gfx::PointerState::slots>& s)
  {
    std::vector<ossia::value> l;
    l.reserve(s.size());
    for(const auto& p : s)
      l.emplace_back(ossia::vec3f{p.x, p.y, p.pressure});
    return l;
  }

  static void from_value(score::gfx::PointerState& state, const ossia::value& v)
  {
    auto* l = v.target<std::vector<ossia::value>>();
    if(!l)
      return;
    const int n = std::min<int>(int(l->size()), score::gfx::PointerState::slots);
    for(int i = 0; i < n; i++)
    {
      const auto& e = (*l)[i];
      if(auto* p = e.target<ossia::vec3f>())
      {
        state.move(i, (*p)[0], (*p)[1]);
        state.press(i, (*p)[2]);
      }
      else if(auto* q = e.target<ossia::vec2f>())
      {
        state.move(i, (*q)[0], (*q)[1]);
      }
      else if(auto* sub = e.target<std::vector<ossia::value>>(); sub && sub->size() >= 2)
      {
        state.move(i, ossia::convert<float>((*sub)[0]), ossia::convert<float>((*sub)[1]));
        if(sub->size() >= 3)
          state.press(i, ossia::convert<float>((*sub)[2]));
      }
    }
  }
};
}
