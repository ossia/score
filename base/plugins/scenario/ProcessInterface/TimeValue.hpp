#pragma once
#include <QTime>
#include <boost/optional.hpp>
#include <chrono>

using namespace std::literals::chrono_literals;

using TimeValue = QTime;

// Value not set means infinity.
using OptionalTimeValue = boost::optional<TimeValue>;
