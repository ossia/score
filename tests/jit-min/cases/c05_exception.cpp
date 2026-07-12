// exception throw/catch at call time
extern "C" int entry() { try { throw 3; } catch(int x) { return x; } }
