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
  uint8_t pad;
};

//! What a looper shows of itself, sent about thirty times a second. Trivial,
//! so that it is copied as it is to the UI.
struct view_message
{
  static constexpr int max_lanes = 8;
  static constexpr int bins = 192;
  static constexpr int max_notes = 384;

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
        const int count = s.midi ? s.notes : s.layers;
        const char* unit = s.midi ? (count == 1 ? "note" : "notes")
                                  : (count == 1 ? "layer" : "layers");
        if(s.full)
          std::snprintf(info, sizeof(info), "%.1f s, full", s.length_seconds);
        else
          std::snprintf(
              info, sizeof(info), "%.1f s  %d %s%s", s.length_seconds, count, unit,
              s.redo > 0 ? "  (redo)" : "");
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

  void paint_notes(auto& ctx, double top, double lanes_h)
  {
    const auto& s = state;
    const double x0 = 4., lw = width() - 8.;
    ctx.set_fill_color({34, 34, 38, 255});
    ctx.begin_path();
    ctx.draw_rect(x0, top + 1., lw, lanes_h - 2.);
    ctx.fill();

    int lo = 127, hi = 0;
    for(int i = 0; i < s.notes; i++)
    {
      lo = std::min<int>(lo, s.note[i].pitch);
      hi = std::max<int>(hi, s.note[i].pitch);
    }
    if(lo > hi)
      return;
    lo = std::max(0, lo - 2);
    hi = std::min(127, hi + 2);
    const double rh = lanes_h / (hi - lo + 1);
    for(int i = 0; i < s.notes; i++)
    {
      const auto& n = s.note[i];
      const double y = top + (hi - n.pitch) * rh;
      const auto lc = lane_color(n.lane, s.lanes);
      ctx.set_fill_color({lc.r, lc.g, lc.b, lc.a});
      ctx.begin_path();
      const double a = x0 + n.start * lw;
      if(n.end >= n.start)
        ctx.draw_rect(a, y, std::max(2., (n.end - n.start) * lw), std::max(1.5, rh - 0.5));
      else
      {
        ctx.draw_rect(a, y, std::max(2., (1. - n.start) * lw), std::max(1.5, rh - 0.5));
        ctx.draw_rect(x0, y, std::max(2., n.end * lw), std::max(1.5, rh - 0.5));
      }
      ctx.fill();
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
    // Pressing elsewhere is the start of a drag of the take.
    return has_take();
  }
  void mouse_move(double, double) { }
  void mouse_release(double, double) { }

  halp::drag_payload drag_payload(double x, double y) const
  {
    halp::drag_payload p;
    if(has_take() && !in_send(x, y))
      p.files = state.take_files();
    return p;
  }
};
}
