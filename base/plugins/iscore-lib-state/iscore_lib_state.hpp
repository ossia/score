#pragma once

/**
 * \namespace State
 * \brief Utilities for OSSIA data structures
 *
 * This namespace contains :
 *
 * * PIMPL wrappers for OSSIA classes, to diminish compile time at the cost of
 *   performance (which is not relevant at all for the user interface).
 * * Useful widgets for the relevant types : widgets to select values, units,
 * etc.
 * * Parsers for various types : expressions, values, addresses.
 * * Serializable forms of OSSIA classes. For instance we want to be able to
 *   represent addresses without using ossia::net::node_base since there may be
 *   no device matching this address yet. So instead we represent an address
 *   with a list of strings.
 */
