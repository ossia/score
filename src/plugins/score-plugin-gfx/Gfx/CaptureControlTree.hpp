#pragma once

/**
 * @file CaptureControlTree.hpp
 * @brief Publishes a capture node's corrections as `<cam>/render/<name>`.
 *
 * The counterpart to the driver-discovered `/controls/` group: those are
 * whatever the hardware happens to expose and differ per camera, these are
 * declared by score and are the same everywhere. Keeping them apart is what
 * stops a driver that publishes its own `gamma` from colliding with ours.
 *
 * Nothing here is V4L2-specific -- it drives a CaptureAdjustSlot, so any
 * DMACaptureInputNode gets the same group whatever the vendor.
 */

#include <Gfx/ControlTree.hpp>
#include <Gfx/Graph/CaptureAdjust.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/domain/domain.hpp>

#include <QVector3D>

#include <cmath>
#include <memory>
#include <optional>
#include <mutex>
#include <vector>

namespace Gfx
{

/**
 * @brief Builds `/render/` for one capture stream.
 *
 * Holds the working copy the controls edit. Each control owns one field, so a
 * write is a read-modify-write of the whole struct -- serialised here, because
 * two controls written at once from different threads would otherwise lose one
 * of the two edits.
 */
class CaptureControlTree
{
public:
  CaptureControlTree(
      score::gfx::CaptureAdjustSlot& slot, ossia::net::device_base& dev,
      ossia::net::node_base& parent, std::string group = "render")
      : m_slot{slot}
  {
    m_value = slot.value();

    auto edit = [this](auto&& fn) {
      std::lock_guard lk{m_mutex};
      fn(m_value);
      m_slot.set(m_value);
    };

    std::vector<TreeControl> controls;

    {
      TreeControl t;
      t.name = "scale_mode";
      t.description
          = "How the frame is fitted to the viewport: Original, BlackBars, "
            "Fill, Stretch";
      t.type = ossia::val_type::STRING;
      t.domain = ossia::make_domain(
          std::vector<std::string>{"Original", "BlackBars", "Fill", "Stretch"});
      t.initial = std::string{"Stretch"};
      t.onSet = [edit](const ossia::value& v) {
        const auto s = v.target<std::string>();
        if(!s)
          return;
        // A name we do not know leaves the mode alone. Falling through to a
        // default would mean an OSC client with a typo silently resetting the
        // fit -- and the domain does not stop that, since a domain describes
        // the values rather than enforcing them.
        std::optional<score::gfx::ScaleMode> mode;
        if(*s == "Original")
          mode = score::gfx::ScaleMode::Original;
        else if(*s == "BlackBars")
          mode = score::gfx::ScaleMode::BlackBars;
        else if(*s == "Fill")
          mode = score::gfx::ScaleMode::Fill;
        else if(*s == "Stretch")
          mode = score::gfx::ScaleMode::Stretch;
        if(!mode)
          return;
        edit([m = *mode](score::gfx::CaptureAdjust& a) { a.scaleMode = m; });
      };
      controls.push_back(std::move(t));
    }

    // Black level and white balance are per-channel, so one vec3 each rather
    // than six scalars: they are set together in practice, and six nodes would
    // mean six round-trips through the slot for one adjustment.
    {
      TreeControl t;
      t.name = "black_level";
      t.description = "Sensor pedestal per channel, normalised, subtracted "
                      "before any gain";
      t.type = ossia::val_type::VEC3F;
      t.domain = ossia::make_domain(0.f, 1.f);
      t.initial = ossia::vec3f{0.f, 0.f, 0.f};
      t.onSet = [edit](const ossia::value& v) {
        if(auto p = v.target<ossia::vec3f>())
          edit([p = *p](score::gfx::CaptureAdjust& a) {
            for(int i = 0; i < 3; ++i)
              a.blackLevel[i] = p[i];
          });
      };
      controls.push_back(std::move(t));
    }

    {
      TreeControl t;
      t.name = "white_balance";
      t.description = "Per-channel gain. A Bayer sensor reads green without it";
      t.type = ossia::val_type::VEC3F;
      t.domain = ossia::make_domain(0.f, 8.f);
      t.initial = ossia::vec3f{1.f, 1.f, 1.f};
      t.onSet = [edit](const ossia::value& v) {
        if(auto p = v.target<ossia::vec3f>())
          edit([p = *p](score::gfx::CaptureAdjust& a) {
            for(int i = 0; i < 3; ++i)
              a.whiteBalance[i] = p[i];
          });
      };
      controls.push_back(std::move(t));
    }

    const auto scalar
        = [&](std::string name, std::string desc, float lo, float hi, float init,
              auto member) {
      TreeControl t;
      t.name = std::move(name);
      t.description = std::move(desc);
      t.type = ossia::val_type::FLOAT;
      t.domain = ossia::make_domain(lo, hi);
      t.initial = init;
      t.onSet = [edit, member](const ossia::value& v) {
        const auto f = ossia::convert<float>(v);
        // A domain describes values, it does not enforce them, so a client can
        // send anything. NaN would survive every clamp in the shader -- it is
        // not greater than or less than anything -- and take the frame with it.
        if(!std::isfinite(f))
          return;
        edit([f, member](score::gfx::CaptureAdjust& a) { a.*member = f; });
      };
      controls.push_back(std::move(t));
    };

    scalar("exposure", "Linear multiplier applied after white balance", 0.f,
           16.f, 1.f, &score::gfx::CaptureAdjust::exposure);
    // 1 is the identity and the default: raising it to 2.2 approximates sRGB,
    // which is usually what makes a linear sensor frame stop looking dark.
    scalar("gamma", "Encoding exponent; 1 leaves the signal linear, 2.2 is "
                    "roughly sRGB", 0.1f, 4.f, 1.f,
           &score::gfx::CaptureAdjust::gamma);
    scalar("saturation", "0 collapses to luma, 1 leaves colour alone", 0.f, 4.f,
           1.f, &score::gfx::CaptureAdjust::saturation);

    m_params = addControlGroup(dev, parent, group, controls);
  }

  ~CaptureControlTree()
  {
    // The parameters outlive this object -- the device owns them and is
    // destroyed after -- and their callbacks capture `this`. Cut the link
    // before it can dangle.
    for(auto* p : m_params)
      if(p)
        p->callbacks_clear();
  }

  CaptureControlTree(const CaptureControlTree&) = delete;
  CaptureControlTree& operator=(const CaptureControlTree&) = delete;

  std::size_t count() const noexcept { return m_params.size(); }

private:
  score::gfx::CaptureAdjustSlot& m_slot;
  std::mutex m_mutex;
  score::gfx::CaptureAdjust m_value;
  std::vector<ossia::net::parameter_base*> m_params;
};

} // namespace Gfx
