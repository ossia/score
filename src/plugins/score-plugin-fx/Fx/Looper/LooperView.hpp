#pragma once
#include <Fx/Looper/Transport.hpp>

#include <halp/custom_widgets.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Nodes::Looper
{
//! A colour, handed to the painter as {r, g, b, a}.
struct rgba
{
  uint8_t r, g, b, a;
};

struct view_note
{
  //! Start and end in the loop, from 0 to 1. An end before the start wraps.
  float start;
  float end;
  uint8_t pitch;
  uint8_t lane;
  uint8_t velocity;
  //! Zero when the note is not heard, e.g. replaced by a later pass.
  uint8_t heard;
};

//! Sent by the view to the looper: erase pass `pass`, counting from the
//! oldest one, of a loop the view showed with `layers` passes. A loop that has
//! changed since is left alone.
struct erase_request
{
  int pass;
  int layers;
};

//! What a looper shows of itself, sent about thirty times a second. Trivial,
//! so that it is copied as it is to the UI.
struct view_message
{
  static constexpr int max_lanes = 8;
  static constexpr int bins = 192;
  static constexpr int max_notes = 768;

  //! LoopMode the looper is in, the one it has taken up, and the one asked
  //! for, which it may still be waiting for the grid to take up.
  int mode;
  int requested;
  int wanted;
  int layers;
  int redo;
  int lanes;
  //! Takes written so far; the files of the last one are take_files().
  int take;
  //! Changes each time Send to timeline is asked from the controls.
  int send_serial;
  int notes;
  bool empty;
  bool full;
  bool writing;
  bool midi;
  float position;
  float length_seconds;
  float max_seconds;
  float tempo;
  //! Where the last take was recorded, in seconds from the process' start.
  double take_date;
  std::array<float, max_lanes * bins> peaks;
  std::array<view_note, max_notes> note;
  //! The last take's files: <base>.<ext> for the mix if there is one,
  //! <base>-layer<k>.<ext> for its layers.
  std::array<char, 512> take_base;
  std::array<char, 4> take_ext;
  bool take_mix;
  int take_layers;

  void set_take(std::string_view base, std::string_view ext, bool mix, int layers) noexcept
  {
    const auto n = std::min(base.size(), take_base.size() - 1);
    std::copy_n(base.data(), n, take_base.data());
    take_base[n] = 0;
    const auto e = std::min(ext.size(), take_ext.size() - 1);
    std::copy_n(ext.data(), e, take_ext.data());
    take_ext[e] = 0;
    take_mix = mix;
    take_layers = layers;
  }

  //! UI thread: the paths of the last take's files, the mix first.
  std::vector<std::string> take_files() const
  {
    std::vector<std::string> files;
    const std::string base{take_base.data()}, ext{take_ext.data()};
    if(base.empty())
      return files;
    if(take_mix)
      files.push_back(base + "." + ext);
    for(int k = 0; k < take_layers; k++)
      files.push_back(base + "-layer" + std::to_string(k + 1) + "." + ext);
    return files;
  }
};

struct looper_view
{
  static constexpr double width() { return 360.; }
  static constexpr double height() { return 132.; }
  static constexpr double header = 26.;
  static constexpr double send_w = 56.;

  view_message state{};
  bool has_state{};
  int last_send_serial{-1};

  std::function<void()> update;
  //! Set by the host when it can place clips in the timeline.
  std::function<void(halp::timeline_clip)> send_to_timeline;
  //! Set by the looper's UI bus: asks the looper to erase a pass.
  std::function<void(erase_request)> erase;

  void receive(const view_message& m) noexcept
  {
    const bool asked = has_state && m.send_serial != last_send_serial;
    state = m;
    has_state = true;
    last_send_serial = m.send_serial;
    if(asked)
      send();
    if(update)
      update();
  }

  bool has_take() const noexcept { return has_state && state.take > 0 && state.take_base[0]; }

  //! Every file of the take, each on a lane of its own.
  void send()
  {
    if(!has_take() || !send_to_timeline)
      return;
    int lane = 0;
    for(auto& f : state.take_files())
      send_to_timeline(
          halp::timeline_clip{.file = std::move(f), .date = state.take_date, .lane = lane++});
  }

  static rgba mode_color(int mode, bool empty) noexcept
  {
    if(empty && mode != int(LoopMode::Record))
      return {110, 110, 116, 255};
    switch(LoopMode(mode))
    {
      case LoopMode::Record:
        return {232, 64, 60, 255};
      case LoopMode::Overdub:
        return {255, 150, 40, 255};
      case LoopMode::Play:
        return {88, 200, 112, 255};
      case LoopMode::Stop:
      default:
        return {150, 150, 156, 255};
    }
  }

  static const char* mode_name(int mode) noexcept
  {
    switch(LoopMode(mode))
    {
      case LoopMode::Record:
        return "Record";
      case LoopMode::Overdub:
        return "Overdub";
      case LoopMode::Play:
        return "Play";
      case LoopMode::Stop:
      default:
        return "Stop";
    }
  }

  static rgba lane_color(int lane, int lanes) noexcept
  {
    static constexpr rgba palette[]{
        {74, 160, 220, 255}, {86, 190, 170, 255}, {150, 200, 90, 255},
        {220, 190, 70, 255}, {230, 140, 70, 255}, {210, 100, 130, 255},
        {160, 110, 210, 255}, {110, 130, 230, 255}};
    (void)lanes;
    return palette[lane % 8];
  }

  void paint(auto ctx)
  {
    const double w = width(), h = height();
    ctx.set_fill_color({24, 24, 26, 255});
    ctx.begin_path();
    ctx.draw_rect(0., 0., w, h);
    ctx.fill();

    const auto& s = state;
    const auto col = mode_color(has_state ? s.mode : int(LoopMode::Stop), !has_state || s.empty);

    // Header: progress ring, mode, what it is waiting for, length and passes.
    {
      const double cx = 13., cy = header / 2., r = 8.;
      ctx.set_stroke_color({60, 60, 64, 255});
      ctx.set_stroke_width(3.);
      ctx.begin_path();
      ctx.draw_circle(cx, cy, r);
      ctx.stroke();
      if(has_state && !s.empty)
      {
        ctx.set_stroke_color({col.r, col.g, col.b, col.a});
        ctx.begin_path();
        ctx.move_to(cx, cy - r);
        ctx.arc_to(cx - r, cy - r, 2. * r, 2. * r, 90., -360. * s.position);
        ctx.stroke();
      }
      ctx.set_stroke_width(1.);

      char text[160];
      if(!has_state)
        std::snprintf(text, sizeof(text), "Play to start looping");
      else if(s.wanted != s.requested)
        std::snprintf(
            text, sizeof(text), "%s, then %s on the grid", mode_name(s.mode),
            mode_name(s.wanted));
      else
        std::snprintf(text, sizeof(text), "%s", mode_name(s.mode));

      ctx.set_fill_color({col.r, col.g, col.b, col.a});
      ctx.begin_path();
      ctx.draw_text(28., cy + 4., text);
      ctx.fill();

      if(has_state)
      {
        char info[160];
        if(s.full)
          std::snprintf(info, sizeof(info), "%.1f s, full", s.length_seconds);
        else
          std::snprintf(
              info, sizeof(info), "%.1f s  %d %s%s", s.length_seconds, s.layers,
              s.layers == 1 ? "layer" : "layers", s.redo > 0 ? "  (redo)" : "");
        ctx.set_fill_color({170, 170, 176, 255});
        ctx.begin_path();
        ctx.draw_text(170., cy + 4., info);
        ctx.fill();
      }

      // Send to timeline
      const bool enabled = has_take() && bool(send_to_timeline);
      const rgba button = enabled ? rgba{60, 110, 160, 255} : rgba{44, 44, 48, 255};
      ctx.set_fill_color({button.r, button.g, button.b, button.a});
      ctx.begin_path();
      ctx.draw_rounded_rect(w - send_w - 4., 4., send_w, header - 8., 3.);
      ctx.fill();
      const rgba label = enabled ? rgba{235, 240, 245, 255} : rgba{110, 110, 116, 255};
      ctx.set_fill_color({label.r, label.g, label.b, label.a});
      ctx.begin_path();
      ctx.draw_text(w - send_w + 6., cy + 4., s.writing ? "Saving" : "Send");
      ctx.fill();
    }

    const double top = header, lanes_h = h - header - 4.;
    if(!has_state || s.empty)
    {
      ctx.set_fill_color({120, 120, 126, 255});
      ctx.begin_path();
      ctx.draw_text(10., top + lanes_h / 2., "Empty. Record to make a loop.");
      ctx.fill();
      if(has_take())
      {
        ctx.begin_path();
        ctx.draw_text(10., top + lanes_h / 2. + 16., "The last take can still be sent.");
        ctx.fill();
      }
      return;
    }

    if(s.midi)
      paint_notes(ctx, top, lanes_h);
    else
      paint_lanes(ctx, top, lanes_h);

    // Playhead
    const double x = 4. + s.position * (w - 8.);
    ctx.set_stroke_color({col.r, col.g, col.b, col.a});
    ctx.set_stroke_width(2.);
    ctx.begin_path();
    ctx.move_to(x, top);
    ctx.line_to(x, top + lanes_h);
    ctx.stroke();
    ctx.set_stroke_width(1.);

    paint_erase_boxes(ctx);
  }

  // The lanes: one per pass, the oldest passes all in the first one when
  // there are more passes than lanes.
  static constexpr double lanes_top = header;
  static constexpr double lanes_x = 4.;
  static double lanes_height() noexcept { return height() - header - 4.; }
  static double lanes_width() noexcept { return width() - 8.; }
  int lane_count() const noexcept { return std::max(1, state.lanes); }
  double lane_height() const noexcept { return lanes_height() / lane_count(); }

  //! The pass a lane shows alone, -1 for a lane of several passes or none.
  int pass_of_lane(int lane) const noexcept
  {
    const auto& s = state;
    if(!has_state || s.empty || lane < 0 || lane >= s.lanes)
      return -1;
    const int merged = s.layers - s.lanes;
    if(merged > 0 && lane == 0)
      return -1;
    return lane + merged;
  }

  struct box
  {
    double x, y, size;
    bool contains(double px, double py) const noexcept
    {
      return px >= x - 1. && px <= x + size + 1. && py >= y - 1. && py <= y + size + 1.;
    }
  };

  //! The erase button of a lane, at its right edge.
  box erase_box(int lane) const noexcept
  {
    const double lh = lane_height();
    const double size = std::clamp(lh - 4., 8., 12.);
    const double y = lanes_top + lane * lh + (lh - size) / 2.;
    return {lanes_x + lanes_width() - size - 3., y, size};
  }

  //! The lane whose erase button is at (x, y), -1 for none.
  int erase_lane_at(double x, double y) const noexcept
  {
    if(!erase)
      return -1;
    for(int l = 0; l < lane_count(); l++)
      if(pass_of_lane(l) >= 0 && erase_box(l).contains(x, y))
        return l;
    return -1;
  }

  void paint_erase_boxes(auto& ctx)
  {
    if(!erase)
      return;
    for(int l = 0; l < lane_count(); l++)
    {
      if(pass_of_lane(l) < 0)
        continue;
      const auto b = erase_box(l);
      ctx.set_fill_color({44, 44, 50, 230});
      ctx.begin_path();
      ctx.draw_rounded_rect(b.x, b.y, b.size, b.size, 2.);
      ctx.fill();
      const double m = std::max(2., b.size * 0.28);
      ctx.set_stroke_color({210, 210, 216, 255});
      ctx.set_stroke_width(1.5);
      ctx.begin_path();
      ctx.move_to(b.x + m, b.y + m);
      ctx.line_to(b.x + b.size - m, b.y + b.size - m);
      ctx.move_to(b.x + b.size - m, b.y + m);
      ctx.line_to(b.x + m, b.y + b.size - m);
      ctx.stroke();
      ctx.set_stroke_width(1.);
    }
  }

  void paint_lanes(auto& ctx, double top, double lanes_h)
  {
    const auto& s = state;
    const int lanes = std::max(1, s.lanes);
    const double lh = lanes_h / lanes;
    const double x0 = 4., lw = width() - 8.;
    const double bw = lw / view_message::bins;
    for(int l = 0; l < lanes; l++)
    {
      const double y = top + l * lh;
      const double mid = y + lh / 2.;
      ctx.set_fill_color({34, 34, 38, 255});
      ctx.begin_path();
      ctx.draw_rect(x0, y + 1., lw, lh - 2.);
      ctx.fill();

      auto c = lane_color(l, lanes);
      ctx.set_fill_color({c.r, c.g, c.b, c.a});
      ctx.begin_path();
      const float* p = s.peaks.data() + l * view_message::bins;
      for(int b = 0; b < view_message::bins; b++)
      {
        const double a = std::min(1.f, p[b]) * (lh / 2. - 1.5);
        if(a > 0.25)
          ctx.draw_rect(x0 + b * bw, mid - a, std::max(1., bw - 0.5), 2. * a);
      }
      ctx.fill();
    }
  }

  //! One small piano roll per lane, over the pitches of that lane. Notes
  //! that are not heard are dimmed.
  void paint_notes(auto& ctx, double top, double lanes_h)
  {
    const auto& s = state;
    const int lanes = std::max(1, s.lanes);
    const double lh = lanes_h / lanes;
    const double x0 = 4., lw = width() - 8.;
    constexpr rgba bg{34, 34, 38, 255};

    std::array<int, view_message::max_lanes> lo, hi;
    lo.fill(127);
    hi.fill(0);
    for(int i = 0; i < s.notes; i++)
    {
      const auto& n = s.note[i];
      const int l = std::min<int>(n.lane, view_message::max_lanes - 1);
      lo[l] = std::min<int>(lo[l], n.pitch);
      hi[l] = std::max<int>(hi[l], n.pitch);
    }

    for(int l = 0; l < lanes; l++)
    {
      const double y = top + l * lh;
      ctx.set_fill_color({bg.r, bg.g, bg.b, bg.a});
      ctx.begin_path();
      ctx.draw_rect(x0, y + 1., lw, lh - 2.);
      ctx.fill();
      if(lo[l] > hi[l])
        continue;
      lo[l] = std::max(0, lo[l] - 1);
      hi[l] = std::min(127, hi[l] + 1);
    }

    // The notes not heard first, so that the ones heard are drawn over them.
    for(int heard = 0; heard < 2; heard++)
    {
      for(int i = 0; i < s.notes; i++)
      {
        const auto& n = s.note[i];
        if(bool(n.heard) != bool(heard))
          continue;
        const int l = std::min<int>(n.lane, lanes - 1);
        const double rh = (lh - 3.) / (hi[l] - lo[l] + 1);
        const double y = top + l * lh + 1.5 + (hi[l] - n.pitch) * rh;
        const double nh = std::clamp(rh - 0.5, 2., 6.);
        auto c = lane_color(l, lanes);
        if(!heard)
          c = rgba{
              uint8_t((c.r + 2 * bg.r) / 3), uint8_t((c.g + 2 * bg.g) / 3),
              uint8_t((c.b + 2 * bg.b) / 3), 255};
        ctx.set_fill_color({c.r, c.g, c.b, c.a});
        ctx.begin_path();
        const double a = x0 + n.start * lw;
        const double yc = std::round(y + (rh - nh) / 2.);
        if(n.end >= n.start)
          ctx.draw_rect(a, yc, std::max(2., (n.end - n.start) * lw), nh);
        else
        {
          ctx.draw_rect(a, yc, std::max(2., (1. - n.start) * lw), nh);
          ctx.draw_rect(x0, yc, std::max(2., n.end * lw), nh);
        }
        ctx.fill();
      }
    }
  }

  bool in_send(double x, double y) const noexcept
  {
    return x >= width() - send_w - 4. && x <= width() - 4. && y >= 4. && y <= header - 4.;
  }

  bool mouse_press(double x, double y)
  {
    if(in_send(x, y))
    {
      send();
      return true;
    }
    if(const int lane = erase_lane_at(x, y); lane >= 0)
    {
      erase(erase_request{.pass = pass_of_lane(lane), .layers = state.layers});
      return true;
    }
    // Pressing elsewhere is the start of a drag of the take.
    return has_take();
  }
  void mouse_move(double, double) { }
  void mouse_release(double, double) { }

  halp::drag_payload drag_payload(double x, double y) const
  {
    halp::drag_payload p;
    if(has_take() && !in_send(x, y) && erase_lane_at(x, y) < 0)
      p.files = state.take_files();
    return p;
  }
};
}
