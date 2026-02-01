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
std::pair<int, int> availableCudaDevice() noexcept
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
    return {};

  if(init(0) != 0)
    return {};

  int count{0};
  if(deviceGetCount(&count) != 0)
    return {};

  for(int i = 0; i < count; i++)
  {
    int major{}, minor{};
    if(deviceComputeCapability(&major, &minor, i) != 0)
      return {};
    else
      return {major, minor};
  }

  return {};
#else
  return {};
#endif
}
catch(...)
{
  return {};
}

bool availableCudaToolkitDylibs(int major, int minor) noexcept
{
#if defined(_WIN32) || defined(__linux__)
  std::vector<std::string> libraries;

  switch(major)
  {
    case 12:
      libraries = {
#ifdef _WIN32
          "cublasLt64_12.dll", "cublas64_12.dll", "cufft64_11.dll", "cudart64_12.dll",
          "cudnn64_9.dll"
#else
          "libcublasLt.so.12", "libcublas.so.12", "libcurand.so.10", "libcufft.so.11",
          "libcudart.so.12",   "libcudnn.so.9"
#endif
      };
      break;
    case 13:
      libraries = {
#ifdef _WIN32
          "cublasLt64_13.dll", "cublas64_13.dll", "cufft64_12.dll", "cudart64_13.dll",
          "cudnn64_9.dll"
#else
          "libcublasLt.so.13", "libcublas.so.13", "libcufft.so.12", "libcudart.so.13",
          "libcudnn.so.9"
#endif
      };
      break;
  }

  for(auto& lib : libraries)
  {
    try
    {
      ossia::dylib_loader cu{lib.c_str()};
    }
    catch(...)
    {
      return false;
    }
  }
  return true;
#endif
  return false;
}
}
