#pragma once
// Transitional compatibility shim (introduced Phase 2, removed Phase 5).
// The generic was renamed to drop the misleading "GpuDirect" naming; this old
// header + the `using` aliases in the new one keep out-of-tree consumers (the
// addon, on its own branch) building until they repoint to the new name.
#include <Gfx/Graph/interop/CpuStagedVideoOutput.hpp>
