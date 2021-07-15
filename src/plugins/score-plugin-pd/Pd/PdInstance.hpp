#pragma once
#if !defined(PDINSTANCE)
#define PDINSTANCE
#endif
struct _pdinstance;
namespace Pd
{
struct Instance
{
  explicit Instance();
  ~Instance();
  _pdinstance* instance{};
  void* file_handle{};
  int dollarzero = 0;
};

}
