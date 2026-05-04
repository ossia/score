#include <Gfx/AssetTable.hpp>

#include <cstddef>
#include <cstdint>

namespace Gfx
{

namespace
{
std::size_t estimateSize(const AssetTable::DecodedAsset& a) noexcept
{
  std::size_t total = 0;
  if(!a.image.isNull())
    total += static_cast<std::size_t>(a.image.sizeInBytes());
  if(a.bytes)
    total += a.bytes->size();
  return total;
}
}

void AssetTable::stage(uint64_t content_hash, QImage image)
{
  std::lock_guard lock{m_mutex};
  auto it = m_entries.find(content_hash);
  if(it != m_entries.end())
    return; // Hash contract: same hash = same bytes. Idempotent stage.

  auto e = std::make_shared<DecodedAsset>();
  e->image = std::move(image);
  e->byte_size = estimateSize(*e);
  const std::size_t sz = e->byte_size;
  m_total_bytes += sz;

  Slot s;
  s.asset = std::move(e);
  // A freshly-staged entry has refcount 0 and no holder yet. Put it in the
  // cold LRU immediately so that a stage() whose consumer never acquire()s it
  // (material removed / model swapped / undo before upload) is still trimmable;
  // otherwise it would be counted in m_total_bytes forever with no path to
  // eviction. acquire() splices it back out of the cold pool on first use.
  m_lru.push_front(content_hash);
  s.lru_it = m_lru.begin();
  s.in_lru = true;
  m_cold_bytes += sz;
  m_entries.emplace(content_hash, std::move(s));
}

void AssetTable::stage(
    uint64_t content_hash,
    std::shared_ptr<const std::vector<uint8_t>> bytes,
    std::string mime_type)
{
  std::lock_guard lock{m_mutex};
  auto it = m_entries.find(content_hash);
  if(it != m_entries.end())
    return;

  auto e = std::make_shared<DecodedAsset>();
  e->bytes = std::move(bytes);
  e->mime_type = std::move(mime_type);
  e->byte_size = estimateSize(*e);
  const std::size_t sz = e->byte_size;
  m_total_bytes += sz;

  Slot s;
  s.asset = std::move(e);
  // See the QImage stage() overload: staged-but-never-acquired entries must be
  // reclaimable, so seed them into the cold LRU. acquire() removes them again.
  m_lru.push_front(content_hash);
  s.lru_it = m_lru.begin();
  s.in_lru = true;
  m_cold_bytes += sz;
  m_entries.emplace(content_hash, std::move(s));
}

std::shared_ptr<const AssetTable::DecodedAsset>
AssetTable::acquire(uint64_t content_hash)
{
  std::lock_guard lock{m_mutex};
  auto it = m_entries.find(content_hash);
  if(it == m_entries.end())
    return {};
  auto& slot = it->second;

  // Resurrect from LRU if cold.
  if(slot.in_lru)
  {
    m_lru.erase(slot.lru_it);
    slot.in_lru = false;
    m_cold_bytes -= slot.asset->byte_size;
  }

  ++slot.asset->refcount;
  return slot.asset;
}

std::shared_ptr<const AssetTable::DecodedAsset>
AssetTable::peek(uint64_t content_hash) const
{
  std::lock_guard lock{m_mutex};
  auto it = m_entries.find(content_hash);
  if(it == m_entries.end())
    return {};
  // Intentionally does NOT move out of LRU nor bump refcount — the
  // caller just wants a read-through. If the entry is cold it stays
  // cold (still evictable next trim). shared_ptr semantics keep the
  // DecodedAsset alive as long as the caller holds the returned ptr,
  // even if eviction happens concurrently on another thread.
  return it->second.asset;
}

void AssetTable::release(uint64_t content_hash)
{
  std::lock_guard lock{m_mutex};
  auto it = m_entries.find(content_hash);
  if(it == m_entries.end())
    return;
  auto& slot = it->second;
  if(slot.asset->refcount > 0)
    --slot.asset->refcount;
  if(slot.asset->refcount == 0 && !slot.in_lru)
  {
    // Newest-first: push_front, tail is oldest. trim() pops from tail.
    m_lru.push_front(content_hash);
    slot.lru_it = m_lru.begin();
    slot.in_lru = true;
    m_cold_bytes += slot.asset->byte_size;
  }
}

void AssetTable::evictOne() noexcept
{
  // Caller holds m_mutex.
  if(m_lru.empty())
    return;
  const uint64_t hash = m_lru.back();
  m_lru.pop_back();

  auto it = m_entries.find(hash);
  if(it == m_entries.end())
    return;

  const std::size_t sz = it->second.asset->byte_size;
  m_total_bytes -= sz;
  m_cold_bytes -= sz;
  m_entries.erase(it);
}

std::size_t AssetTable::trim(std::size_t max_bytes_budget)
{
  std::lock_guard lock{m_mutex};
  std::size_t evicted = 0;
  // Only evict from cold pool — hot entries stay regardless of budget.
  while(m_cold_bytes > max_bytes_budget && !m_lru.empty())
  {
    const std::size_t before_total = m_total_bytes;
    evictOne();
    evicted += (before_total - m_total_bytes);
  }
  return evicted;
}

void AssetTable::maybeAutoTrim(
    float utilization, float high_watermark, float target)
{
  if(utilization < high_watermark)
    return;

  std::lock_guard lock{m_mutex};
  if(m_cold_bytes == 0)
    return;

  // Convert target utilization to a cold-pool budget. Heuristic:
  // scale the current cold pool by (target / utilization). At
  // util=0.85, target=0.60 → trim to ~70% of current cold total.
  // Not a proper memory-pressure solver — a low-cost knob that
  // kicks in on sustained overload.
  const float scale = target / utilization;
  const auto budget
      = static_cast<std::size_t>(static_cast<float>(m_cold_bytes) * scale);
  while(m_cold_bytes > budget && !m_lru.empty())
    evictOne();
}

std::size_t AssetTable::size() const noexcept
{
  std::lock_guard lock{m_mutex};
  return m_entries.size();
}

std::size_t AssetTable::totalBytes() const noexcept
{
  std::lock_guard lock{m_mutex};
  return m_total_bytes;
}

std::size_t AssetTable::coldCount() const noexcept
{
  std::lock_guard lock{m_mutex};
  return m_lru.size();
}

} // namespace Gfx
