#pragma once

namespace Explorer
{
enum class Column : int
{
    Name = 0,
    Value,
    Get,
    Set,
    Min,
    Max,

    Count //column count, always last
};
}
