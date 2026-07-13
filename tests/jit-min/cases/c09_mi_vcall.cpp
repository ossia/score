struct I1 { virtual ~I1() {} virtual int a() { return 1; } };
struct I2 { virtual ~I2() {} virtual int b() { return 2; } };
struct D : I1, I2 { int a() override { return 10; } int b() override { return 20; } };
static D g;
extern "C" int entry() { I2* p = &g; return p->b(); } // expect 20
