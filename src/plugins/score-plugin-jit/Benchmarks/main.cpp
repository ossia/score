#include "Gain.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>

using clk = std::chrono::steady_clock;

int main()
{
  std::vector<aligned_vec> in, out;
  in.resize(16);
  for (int i = 0; i < 16; i++)
  {
    in[i].resize(1024);
    for (int j = 0; j < 1024; j++)
    {
      in[i][j] = (double)rand() / 10000.;
    }
  }

  out = in;
  {
    auto t0 = clk::now();

    for (int i = 0; i < 100000; i++)
    {
      gain(in, 0.5, out);
    }
    auto t1 = clk::now();
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0)
                     .count()
              << " ms";
    std::cout << std::endl;
  }
}
