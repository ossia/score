#pragma once
#include <ossia/dataflow/dataflow_fwd.hpp>
#include <ossia/detail/hash_map.hpp>

#include <score_plugin_engine_export.h>

#include <span>

namespace ossia::net
{
class device_base;
}

namespace Execution
{
/**
 * @brief The node and parameter addresses a device owns.
 *
 * Taken while the device is alive. A node and a parameter are distinct
 * objects, so one set holds both.
 */
SCORE_PLUGIN_ENGINE_EXPORT
ossia::hash_set<const void*> deviceAddresses(ossia::net::device_base& d);

/**
 * @brief Whether the address a port stores is one of @p owned.
 *
 * Pointer identity only: the stored address may already dangle, and must
 * never be dereferenced here.
 */
SCORE_PLUGIN_ENGINE_EXPORT
bool addressBelongsTo(
    const ossia::destination_t& dest,
    const ossia::hash_set<const void*>& owned) noexcept;

/**
 * @brief Reset every port address in @p nodes that is one of @p owned.
 *
 * Runs on the execution thread. Ports addressed to another device are left
 * alone.
 */
SCORE_PLUGIN_ENGINE_EXPORT
void clearAddresses(
    std::span<ossia::graph_node* const> nodes,
    const ossia::hash_set<const void*>& owned) noexcept;
}
