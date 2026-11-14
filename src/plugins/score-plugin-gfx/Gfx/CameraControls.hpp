#pragma once
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/parameter.hpp>

#include <memory>

namespace Gfx
{
struct CameraSettings;

//! Hardware controls of a camera, published under <device>:/controls/color
struct CameraControls
{
  virtual ~CameraControls() = default;

  virtual void setup(ossia::net::node_base& root) = 0;
  virtual bool push(const ossia::net::parameter_base&, const ossia::value&) = 0;

  //! Must be called before the nodes given to setup() are destroyed
  virtual void stop() = 0;
};

//! Null when the camera cannot be found or has no control
std::shared_ptr<CameraControls> make_camera_controls(const CameraSettings&);
}
