#include <cmath>
float abs_max(float f1, float f2) noexcept
{
    return f2 >= 0.f
            ? f1 < f2 ? f2 : f1
            : f1 < -f2 ? f2 : f1;
}

float abs_max2(float f1, float f2) noexcept
{
    const int mul = (f2 >= 0.f) ? 1 : -1;
    if(f1 < mul * f2)
      return f2;
      else
      return f1;
}

static void abs1(benchmark::State& state) {

std::vector<float> f;
f.resize(2000);
for(int i = 0; i < f.size(); i++) f[i] = i  % 20 - 10;

  for (auto _ : state) {
    float res = 0.f;
    for(int i = 0; i < f.size(); i++) res = abs_max(res, f[i]);
    benchmark::DoNotOptimize(res);
  }
}
// Register the function as a benchmark
BENCHMARK(abs1);

static void abs2(benchmark::State& state) {

std::vector<float> f;
f.resize(2000);

for(int i = 0; i < f.size(); i++) f[i] = i  % 20 - 10;

  for (auto _ : state) {
    float res = 0.f;
    for(int i = 0; i < f.size(); i++) res = abs_max2(res, f[i]);
    benchmark::DoNotOptimize(res);
  }
}
// Register the function as a benchmark
BENCHMARK(abs2);