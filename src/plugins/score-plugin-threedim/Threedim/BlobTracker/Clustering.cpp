#include "Clustering.hpp"

#include <cmath>

#include <algorithm>
#include <limits>
#include <numeric>

namespace Threedim::Blobs
{
namespace
{
// The GPU pipeline's posValid(): NaN, Inf, and coordinates so large that the
// cell index would not survive the float-to-int conversion are all rejected.
bool position_valid(float x, float y, float z) noexcept
{
  return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && std::abs(x) < 1e18f
         && std::abs(y) < 1e18f && std::abs(z) < 1e18f;
}

struct Cell
{
  int32_t x{}, y{}, z{};
};

// Cell size equals the cluster distance, so two points closer than that always
// land in the same cell or in touching cells — which is what makes the 27-cell
// neighbourhood exhaustive.
Cell cell_of(float px, float py, float pz, float inv_cell_size) noexcept
{
  const auto q = [inv_cell_size](float v) {
    return (int32_t)std::floor(std::clamp(v * inv_cell_size, -1e9f, 1e9f));
  };
  return {q(px), q(py), q(pz)};
}

uint32_t bucket_of(Cell c, uint32_t table_size) noexcept
{
  return ((uint32_t)c.x * 73856093u ^ (uint32_t)c.y * 19349663u
          ^ (uint32_t)c.z * 83492791u)
         & (table_size - 1u);
}

// Power of two, so bucket_of can mask instead of dividing. Saturating at
// max_table_size only lengthens the bucket chains; the distance test still
// decides membership, so a saturated table costs time, never correctness.
uint32_t table_size_for(int64_t points) noexcept
{
  uint32_t ts = min_table_size;
  while(ts < (uint32_t)points && ts < max_table_size)
    ts <<= 1;
  return ts;
}
}

int32_t Clusterer::find(int32_t x) noexcept
{
  while(m_parent[x] != x)
  {
    m_parent[x] = m_parent[m_parent[x]];
    x = m_parent[x];
  }
  return x;
}

void Clusterer::build_grid(int32_t n, float inv_cell_size, uint32_t table_size)
{
  m_cell_start.assign((size_t)table_size + 1, 0u);

  // Histogram, offset by one so the exclusive scan can run in place: bucket b's
  // count lands in slot b+1 and the running sum turns it into b's start.
  for(int32_t i = 0; i < n; i++)
  {
    const auto c = cell_of(
        m_scratch[i * 3], m_scratch[i * 3 + 1], m_scratch[i * 3 + 2], inv_cell_size);
    m_cell_start[bucket_of(c, table_size) + 1]++;
  }

  for(uint32_t b = 0; b < table_size; b++)
    m_cell_start[b + 1] += m_cell_start[b];

  m_cursor.assign(m_cell_start.begin(), m_cell_start.end() - 1);
  m_sorted.resize((size_t)n * 3);
  m_origin.resize(n);

  for(int32_t i = 0; i < n; i++)
  {
    const auto c = cell_of(
        m_scratch[i * 3], m_scratch[i * 3 + 1], m_scratch[i * 3 + 2], inv_cell_size);
    const uint32_t slot = m_cursor[bucket_of(c, table_size)]++;
    m_sorted[(size_t)slot * 3 + 0] = m_scratch[i * 3 + 0];
    m_sorted[(size_t)slot * 3 + 1] = m_scratch[i * 3 + 1];
    m_sorted[(size_t)slot * 3 + 2] = m_scratch[i * 3 + 2];
    // Everything downstream works in cell order; this is the only way back to
    // where a point sat in the input, which is what orders the blobs.
    m_origin[slot] = i;
  }
}

void Clusterer::union_neighbours(
    int32_t n, float inv_cell_size, uint32_t table_size, float cluster_dist2)
{
  m_parent.resize(n);
  std::iota(m_parent.begin(), m_parent.end(), 0);

  // Always reparent the higher index onto the lower one: parent indices then
  // strictly decrease along any path, so find() cannot cycle and a component's
  // root is its lowest-numbered point.
  const auto unite = [this](int32_t a, int32_t b) {
    a = find(a);
    b = find(b);
    if(a == b)
      return;
    if(a > b)
      std::swap(a, b);
    m_parent[b] = a;
  };

  for(int32_t i = 0; i < n; i++)
  {
    const float px = m_sorted[i * 3 + 0];
    const float py = m_sorted[i * 3 + 1];
    const float pz = m_sorted[i * 3 + 2];
    const auto c = cell_of(px, py, pz, inv_cell_size);

    for(int dx = -1; dx <= 1; dx++)
      for(int dy = -1; dy <= 1; dy++)
        for(int dz = -1; dz <= 1; dz++)
        {
          const uint32_t b = bucket_of({c.x + dx, c.y + dy, c.z + dz}, table_size);
          const uint32_t first = m_cell_start[b];
          const uint32_t last = m_cell_start[b + 1];

          for(uint32_t j = first; j < last; j++)
          {
            // Each unordered pair is tested once: cell adjacency is symmetric,
            // so the higher index always sees the lower one.
            if((int32_t)j <= i)
              continue;

            const float ex = px - m_sorted[(size_t)j * 3 + 0];
            const float ey = py - m_sorted[(size_t)j * 3 + 1];
            const float ez = pz - m_sorted[(size_t)j * 3 + 2];
            if(ex * ex + ey * ey + ez * ez < cluster_dist2)
              unite(i, (int32_t)j);
          }
        }
  }
}

int32_t Clusterer::label(int32_t n, bool& overflow)
{
  m_cluster.assign(n, -1);

  int32_t clusters = 0;
  for(int32_t i = 0; i < n; i++)
  {
    const int32_t root = find(i);
    if(root == i)
    {
      // A component's root is its lowest index, so by the time a member is
      // reached its root already has an id — flatten, label and propagate
      // collapse into this one ascending pass.
      const int32_t id = clusters++;
      if(id < max_clusters)
        m_cluster[i] = id;
      else
        overflow = true;
    }
    else
    {
      m_cluster[i] = m_cluster[root];
    }
  }
  return clusters;
}

void Clusterer::accumulate(int32_t n, int32_t clusters)
{
  constexpr float fmax = std::numeric_limits<float>::max();

  m_sum.assign((size_t)clusters * 3, 0.);
  m_counts.assign(clusters, 0);
  m_first.assign(clusters, std::numeric_limits<int32_t>::max());
  m_bounds.assign((size_t)clusters * 6, 0.f);
  for(int32_t c = 0; c < clusters; c++)
  {
    m_bounds[(size_t)c * 6 + 0] = fmax;
    m_bounds[(size_t)c * 6 + 1] = fmax;
    m_bounds[(size_t)c * 6 + 2] = fmax;
    m_bounds[(size_t)c * 6 + 3] = -fmax;
    m_bounds[(size_t)c * 6 + 4] = -fmax;
    m_bounds[(size_t)c * 6 + 5] = -fmax;
  }

  for(int32_t i = 0; i < n; i++)
  {
    const int32_t cid = m_cluster[i];
    if(cid < 0)
      continue;

    const float px = m_sorted[i * 3 + 0];
    const float py = m_sorted[i * 3 + 1];
    const float pz = m_sorted[i * 3 + 2];

    m_sum[(size_t)cid * 3 + 0] += px;
    m_sum[(size_t)cid * 3 + 1] += py;
    m_sum[(size_t)cid * 3 + 2] += pz;
    m_counts[cid]++;
    m_first[cid] = std::min(m_first[cid], m_origin[i]);

    float* b = m_bounds.data() + (size_t)cid * 6;
    b[0] = std::min(b[0], px);
    b[1] = std::min(b[1], py);
    b[2] = std::min(b[2], pz);
    b[3] = std::max(b[3], px);
    b[4] = std::max(b[4], py);
    b[5] = std::max(b[5], pz);
  }
}

float Clusterer::estimate_spacing(
    int32_t n, float inv_cell_size, uint32_t table_size, int k)
{
  // Fewer points than neighbours asked for: nothing meaningful to measure.
  if(k <= 0 || n <= k)
    return 0.f;

  const int samples = std::min<int>(n, knn_samples);
  const float cell_size = 1.f / inv_cell_size;

  m_knn.clear();
  m_knn.reserve(samples);

  float topk[knn_max_k];
  for(int tid = 0; tid < samples; tid++)
  {
    // Points are already in cell order, so striding the sorted index is a
    // spatially even sample of the cloud.
    int64_t idx = (int64_t)tid * n / samples;
    if(idx >= n)
      idx = n - 1;

    const float px = m_sorted[(size_t)idx * 3 + 0];
    const float py = m_sorted[(size_t)idx * 3 + 1];
    const float pz = m_sorted[(size_t)idx * 3 + 2];
    const auto c = cell_of(px, py, pz, inv_cell_size);

    std::fill_n(topk, knn_max_k, std::numeric_limits<float>::max());
    int found = 0;
    int scanned = 0;

    for(int nb = 0; nb < 27 && scanned < knn_max_candidates; nb++)
    {
      // Visit the sample's own cell first, so the nearest neighbours are seen
      // before the candidate cap can bite.
      const int m = (nb == 0) ? 13 : ((nb <= 13) ? nb - 1 : nb);
      const Cell d{c.x + m % 3 - 1, c.y + (m / 3) % 3 - 1, c.z + m / 9 - 1};

      const uint32_t b = bucket_of(d, table_size);
      const uint32_t first = m_cell_start[b];
      const uint32_t last = m_cell_start[b + 1];

      for(uint32_t j = first; j < last && scanned < knn_max_candidates; j++)
      {
        if((int64_t)j == idx)
          continue;
        scanned++;
        found++;

        const float ex = m_sorted[(size_t)j * 3 + 0] - px;
        const float ey = m_sorted[(size_t)j * 3 + 1] - py;
        const float ez = m_sorted[(size_t)j * 3 + 2] - pz;
        const float d2 = ex * ex + ey * ey + ez * ez;
        if(d2 >= topk[k - 1])
          continue;

        topk[k - 1] = d2;
        for(int t = k - 2; t >= 0; t--)
        {
          if(topk[t + 1] >= topk[t])
            break;
          std::swap(topk[t], topk[t + 1]);
        }
      }
    }

    // Fewer than k neighbours within reach means the cell is smaller than the
    // real spacing. Reporting the cell size grows the estimate, so the next
    // frame searches wider and the loop converges upward instead of stalling.
    m_knn.push_back((found >= k) ? std::sqrt(topk[k - 1]) : cell_size);
  }

  const auto end = std::remove_if(
      m_knn.begin(), m_knn.end(), [](float v) { return !(v > 0.f && v < 1e18f); });
  const auto count = (int)std::distance(m_knn.begin(), end);
  if(count <= 0)
    return 0.f;

  // The 75th percentile rather than the median: more forgiving of sparse
  // regions, so thin parts of a body still join their cluster.
  const int q = std::min((count * 3) / 4, count - 1);
  std::nth_element(m_knn.begin(), m_knn.begin() + q, m_knn.begin() + count);
  return m_knn[q];
}

int Clusterer::detect(
    const float* positions, int64_t point_count, const ClusterParams& params,
    std::vector<Detection>& blobs, ClusterResult& result)
{
  result = ClusterResult{};
  blobs.clear();

  if(!positions || point_count <= 0)
    return 0;

  const float cluster_dist = std::max(params.cluster_dist, 1e-8f);
  const int max_blobs = std::clamp(params.max_blobs, 1, max_blobs_limit);
  const int min_points = std::max(params.min_points, 1);
  const int knn_k = std::clamp(params.knn_k, 0, knn_max_k);

  m_scratch.clear();
  m_scratch.reserve((size_t)point_count * 3);
  for(int64_t i = 0; i < point_count; i++)
  {
    const float x = positions[i * 3 + 0];
    const float y = positions[i * 3 + 1];
    const float z = positions[i * 3 + 2];
    if(!position_valid(x, y, z))
      continue;
    m_scratch.push_back(x);
    m_scratch.push_back(y);
    m_scratch.push_back(z);
  }

  const int32_t n = (int32_t)(m_scratch.size() / 3);
  result.num_valid_points = n;
  if(n == 0)
    return 0;

  const float inv_cell_size = 1.f / cluster_dist;
  const uint32_t table_size = table_size_for(n);

  build_grid(n, inv_cell_size, table_size);
  union_neighbours(n, inv_cell_size, table_size, cluster_dist * cluster_dist);

  bool overflow = false;
  const int32_t clusters = label(n, overflow);
  result.num_clusters = clusters;
  result.cluster_overflow = overflow;

  const int32_t labelled = std::min(clusters, max_clusters);
  accumulate(n, labelled);

  // Cell order is hash order, which says nothing to whoever supplied the cloud.
  // Report the clusters that passed the filter in the order their first point
  // appeared in the input, so Max blobs also keeps the first ones.
  m_emit.clear();
  for(int32_t cid = 0; cid < labelled; cid++)
    if(m_counts[cid] >= min_points)
      m_emit.push_back(cid);
  std::sort(m_emit.begin(), m_emit.end(), [this](int32_t a, int32_t b) {
    return m_first[a] < m_first[b];
  });
  if((int)m_emit.size() > max_blobs)
    m_emit.resize(max_blobs);

  for(const int32_t cid : m_emit)
  {
    const int32_t count = m_counts[cid];
    const float* b = m_bounds.data() + (size_t)cid * 6;
    Detection d;
    d.cx = (float)(m_sum[(size_t)cid * 3 + 0] / count);
    d.cy = (float)(m_sum[(size_t)cid * 3 + 1] / count);
    d.cz = (float)(m_sum[(size_t)cid * 3 + 2] / count);
    d.bmnx = b[0];
    d.bmny = b[1];
    d.bmnz = b[2];
    d.bmxx = b[3];
    d.bmxy = b[4];
    d.bmxz = b[5];
    d.point_count = count;
    blobs.push_back(d);
  }

  result.num_blobs = (int)blobs.size();

  for(int a = 0; a < 3; a++)
  {
    result.bounds_min[a] = std::numeric_limits<float>::max();
    result.bounds_max[a] = std::numeric_limits<float>::lowest();
  }
  for(int32_t i = 0; i < n; i++)
    for(int a = 0; a < 3; a++)
    {
      const float v = m_sorted[(size_t)i * 3 + a];
      result.bounds_min[a] = std::min(result.bounds_min[a], v);
      result.bounds_max[a] = std::max(result.bounds_max[a], v);
    }

  result.knn_spacing = estimate_spacing(n, inv_cell_size, table_size, knn_k);
  return result.num_blobs;
}

}
