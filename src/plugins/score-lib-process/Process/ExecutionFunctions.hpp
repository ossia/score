#pragma once
#include <score/tools/std/Optional.hpp>

#include <ossia/network/value/destination.hpp>

#include <score_lib_process_export.h>

namespace State
{
struct Address;
struct AddressAccessor;
}
namespace ossia
{
struct execution_state;
}
namespace ossia::net
{
class node_base;
}
namespace Execution
{

SCORE_LIB_PROCESS_EXPORT
ossia::net::node_base* findNode(const ossia::execution_state& st, const State::Address& addr);

SCORE_LIB_PROCESS_EXPORT
optional<ossia::destination>
makeDestination(const ossia::execution_state& devices, const State::AddressAccessor& addr);
}
