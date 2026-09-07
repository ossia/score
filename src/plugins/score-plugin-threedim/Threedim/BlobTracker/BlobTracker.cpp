#include "BlobTracker.hpp"

#include <utility>

namespace Threedim
{

void BlobTracker::operator()()
{
  if(std::exchange(m_reset_requested, false))
    m_tracker.reset();

  Blobs::TrackerSettings s;
  s.auto_scale = inputs.auto_scale.value;
  s.scale_factor = inputs.scale_factor.value;
  s.cluster_dist = inputs.cluster_dist.value;
  s.match_dist = inputs.match_dist.value;
  s.min_points = inputs.min_points.value;
  s.max_blobs = inputs.max_blobs.value;
  s.max_age = inputs.max_age.value;
  s.min_hits = inputs.min_hits.value;
  s.use_point_count = inputs.use_point_count.value;
  s.point_count_weight = inputs.point_count_weight.value;
  s.merge_memory_frames = inputs.merge_memory.value;
  s.smoothing = inputs.smoothing.value;
  s.velocity_threshold = inputs.velocity_threshold.value;

  // The GPU path: 3D Blob Clustering already produced the frame's detections
  // and the engine read its results buffer back into this inlet. Decoding is
  // all that is left before tracking.
  const auto& blob_buf = inputs.blobs_in.buffer;
  const bool on_gpu
      = blob_buf.raw_data != nullptr
        && blob_buf.byte_size >= (int64_t)(Blobs::gpu_meta_words * sizeof(uint32_t));
  outputs.on_gpu.value = on_gpu;

  if(on_gpu)
  {
    Blobs::ClusterResult res;
    Blobs::decode_gpu_results(
        blob_buf.raw_data + blob_buf.byte_offset,
        blob_buf.byte_size - blob_buf.byte_offset, s.max_blobs, m_gpu_detections, res);

    m_gpu_blobs.resize(m_gpu_detections.size());
    for(std::size_t i = 0; i < m_gpu_detections.size(); i++)
      m_gpu_blobs[i] = Blobs::Blob::from_detection(m_gpu_detections[i]);

    // Auto scale is driven by the spacing the clustering measured, exactly as
    // it is on the CPU path.
    m_tracker.track_gpu(m_gpu_blobs, res, s);
  }
  else
  {
    // A trailing partial triple is ignored rather than read past.
    const auto& points = inputs.points.value;
    const int64_t count = (int64_t)(points.size() / 3);

    // An empty cloud is a normal state for a live sensor: the tracks still age,
    // so stale blobs expire instead of freezing in place.
    m_tracker.process(count > 0 ? points.data() : nullptr, count, s);
  }

  const auto& blobs = m_tracker.blobs();

  outputs.count.value = (int)blobs.size();
  outputs.detected.value = m_tracker.cluster_result().num_blobs;
  outputs.spacing.value = m_tracker.point_spacing();
  outputs.cluster_distance.value = m_tracker.cluster_distance();

  auto& out_blobs = outputs.blobs.value;
  auto& out_ids = outputs.ids.value;
  auto& out_centroids = outputs.centroids.value;
  out_blobs.clear();
  out_ids.clear();
  out_centroids.clear();
  out_blobs.reserve(blobs.size());
  out_ids.reserve(blobs.size());
  out_centroids.reserve(blobs.size() * 3);

  for(const auto& b : blobs)
  {
    out_blobs.push_back(
        BlobValue{
            .id = b.id,
            .x = b.cx,
            .y = b.cy,
            .z = b.cz,
            .size_x = b.bmxx - b.bmnx,
            .size_y = b.bmxy - b.bmny,
            .size_z = b.bmxz - b.bmnz,
            .velocity_x = b.vx,
            .velocity_y = b.vy,
            .velocity_z = b.vz,
            .points = b.point_count});
    out_ids.push_back(b.id);
    out_centroids.push_back(b.cx);
    out_centroids.push_back(b.cy);
    out_centroids.push_back(b.cz);
  }

  write_geometry();
}

void BlobTracker::write_geometry()
{
  Blobs::build_wireframe(
      m_tracker.blobs(), inputs.velocity_threshold.value, m_vertices, m_indices);

  auto& mesh = outputs.geometry.mesh;
  mesh.buffers.clear();
  mesh.bindings.clear();
  mesh.attributes.clear();
  mesh.input.clear();
  mesh.topology = halp::primitive_topology::lines;
  mesh.cull_mode = halp::cull_mode::none;
  mesh.front_face = halp::front_face::counter_clockwise;
  mesh.instances = 1;

  if(m_vertices.empty())
  {
    // The renderer skips a mesh with no vertices, which is what we want with
    // nothing to show — rather than the previous frame's boxes lingering.
    mesh.vertices = 0;
    mesh.indices = 0;
    mesh.index.buffer = -1;
    outputs.geometry.dirty_mesh = true;
    return;
  }

  mesh.buffers.push_back(
      halp::geometry_cpu_buffer{
          .raw_data = m_vertices.data(),
          .byte_size = (int64_t)(m_vertices.size() * sizeof(float)),
          .dirty = true});
  mesh.buffers.push_back(
      halp::geometry_cpu_buffer{
          .raw_data = m_indices.data(),
          .byte_size = (int64_t)(m_indices.size() * sizeof(uint32_t)),
          .dirty = true});

  // xyz then the track id, interleaved, one binding.
  mesh.bindings.push_back(
      halp::geometry_binding{
          .stride = 4 * sizeof(float),
          .step_rate = 1,
          .classification = halp::binding_classification::per_vertex});

  mesh.attributes.push_back(
      halp::geometry_attribute{
          .binding = 0,
          .semantic = halp::attribute_semantic::position,
          .format = halp::attribute_format::float3,
          .byte_offset = 0});
  mesh.attributes.push_back(
      halp::geometry_attribute{
          .binding = 0,
          .semantic = halp::attribute_semantic::custom,
          .format = halp::attribute_format::float1,
          .byte_offset = 3 * sizeof(float),
          .name = "blobId"});

  mesh.input.push_back(halp::geometry_input{.buffer = 0, .byte_offset = 0});

  mesh.vertices = (int)(m_vertices.size() / 4);
  mesh.indices = (int)m_indices.size();
  mesh.index.buffer = 1;
  mesh.index.byte_offset = 0;
  mesh.index.format = halp::index_format::uint32;

  outputs.geometry.dirty_mesh = true;
}

}
