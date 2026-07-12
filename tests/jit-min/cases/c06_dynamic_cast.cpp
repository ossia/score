// self-contained dynamic_cast: addon-local polymorphic types, but the cxxabi
// type_info vtables (_ZTVN10__cxxabiv1...) + __dynamic_cast come from host libc++.
struct B { virtual ~B() {} virtual int f() { return 1; } };
struct D : B { int f() override { return 2; } };
extern "C" int entry()
{
  B* b = new D;
  D* d = dynamic_cast<D*>(b);
  int r = d ? d->f() : -1;
  delete b;
  return r; // expect 2
}
