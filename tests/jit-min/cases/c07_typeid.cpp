// typeid / RTTI name access on an addon-local polymorphic type.
#include <typeinfo>
struct B { virtual ~B() {} };
struct D : B { };
extern "C" int entry()
{
  B* b = new D;
  const char* n = typeid(*b).name();
  int r = (n && n[0]) ? 3 : -1;
  delete b;
  return r; // expect 3
}
