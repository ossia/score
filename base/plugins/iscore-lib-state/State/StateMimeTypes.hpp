#pragma once

// This file defines the mime types that are used for remote communication.
// The actual data is JSON as encoded by the i-score visitor classes.
namespace iscore
{
    namespace mime
    {
        inline constexpr const char * state() { return "application/x-iscore-state"; }
    }
}
