#include <Foundation/Foundation.h>

NSAutoreleasePool* mac_init_pool()
{
  return NULL;
  //NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  //return pool;
}

void mac_finish_pool(NSAutoreleasePool* pool)
{
  //[pool release];
}
