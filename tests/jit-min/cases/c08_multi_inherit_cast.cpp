// multiple inheritance cross-cast -> __vmi_class_type_info (what real plugins use)
struct I1 { virtual ~I1() {} virtual int a() { return 1; } };
struct I2 { virtual ~I2() {} virtual int b() { return 2; } };
struct D : I1, I2 { int a() override { return 10; } int b() override { return 20; } };
extern "C" int entry()
{
  I1* p = new D;
  I2* q = dynamic_cast<I2*>(p); // cross-cast through vmi type_info
  int r = q ? q->b() : -1;
  delete p;
  return r; // expect 20
}
