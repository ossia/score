#include "Cuda.hpp"

#include <ossia/detail/dylib_loader.hpp>

#ifdef _WIN32
#define CUDAAPI __stdcall
#else
#define CUDAAPI
#endif

enum cuda_result_enum
{
  CUDA_SUCCESS = 0,
  CUDA_ERROR_UNKNOWN = 999
};
typedef int CUdevice_v1;
using cuInit_pt = cuda_result_enum CUDAAPI (*)(unsigned int Flags);
using cuDeviceGetCount_pt = cuda_result_enum CUDAAPI (*)(int* count);
using cuDeviceComputeCapability_pt
    = cuda_result_enum CUDAAPI (*)(int* major, int* minor, int dev);

namespace score
{
bool checkCudaSupported() noexcept
try
{
#if defined(_WIN32) || defined(__linux__)
  ossia::dylib_loader cuda{
#ifdef _WIN32
      "nvcuda.dll"
#else
      "libcuda.so"
#endif
  };

  const auto init = cuda.symbol<cuInit_pt>("cuInit");
  const auto deviceGetCount = cuda.symbol<cuDeviceGetCount_pt>("cuDeviceGetCount");
  const auto deviceComputeCapability
      = cuda.symbol<cuDeviceComputeCapability_pt>("cuDeviceComputeCapability");

  if(!init || !deviceGetCount || !deviceComputeCapability)
    return false;

  if(init(0) != 0)
    return false;

  int count{0};
  if(deviceGetCount(&count) != 0)
    return false;

  for(int i = 0; i < count; i++)
  {
    int major{}, minor{};
    if(deviceComputeCapability(&major, &minor, i) != 0)
      return false;
  }

  return true;
#else
  return false;
#endif
}
catch(...)
{
  return false;
}
}
