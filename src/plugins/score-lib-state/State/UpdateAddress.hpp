#pragma once
#include <State/MessageListSerialization.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <QMimeData>

#include <optional>
namespace State
{

SCORE_LIB_STATE_EXPORT
std::optional<State::Address>
onUpdatableAddress(const State::Address& current, const QMimeData& mime);

SCORE_LIB_STATE_EXPORT
std::optional<State::AddressAccessor>
onUpdatableAddress(const State::AddressAccessor& current, const QMimeData& mime);
}
