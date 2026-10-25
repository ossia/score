#pragma once
#include <Threedim/LiveLink/Protocol.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <cstring>

// Light and camera components from their live link records: the mapping
// GltfParser's to_light and to_camera make from the same glTF values, used
// for streamed overrides and for native scenes alike.
namespace Threedim::LiveLink
{

inline void apply_light(ossia::light_component& lc, const light_record& r)
{
  switch(r.type)
  {
    case light_record::directional:
      lc.type = ossia::light_type::directional;
      break;
    case light_record::spot:
      lc.type = ossia::light_type::spot;
      break;
    default:
      lc.type = ossia::light_type::point;
      break;
  }
  std::memcpy(lc.color, r.color, sizeof(lc.color));
  lc.intensity = r.intensity;
  lc.range = r.range;
  lc.inner_cone_angle = r.inner_cone;
  lc.outer_cone_angle = r.outer_cone;
}

inline void apply_camera(ossia::camera_component& cc, const camera_record& r)
{
  if(r.projection == camera_record::orthographic)
  {
    cc.projection = ossia::camera_projection::orthographic;
    cc.xmag = r.xmag;
    cc.ymag = r.ymag;
  }
  else
  {
    cc.projection = ossia::camera_projection::perspective;
    cc.yfov = r.yfov;
    cc.aspect_ratio = r.aspect > 0.f ? r.aspect : 1.f;
  }
  cc.znear = r.znear;
  cc.zfar = r.zfar > 0.f ? r.zfar : 1000.f;
}

}
