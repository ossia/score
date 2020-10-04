#define HAVE_DFT 1
#include <kfr/base.hpp>
#include <kfr/dft.hpp>
#include <kfr/dsp.hpp>

#include <chrono>
#include <iostream>

#include <kfr/dft/impl/dft-impl-f64.cpp>

using namespace kfr;
using namespace std::chrono;
using clk = steady_clock;

extern "C" [[dllexport]] void benchmark_main()
{
  std::cout << std::flush;
  std::cerr << std::flush;
  println(library_version());
  std::cout << std::flush;
  std::cerr << std::flush;

  // fft size
  const size_t size = 16384;

  // initialize input & output buffers
  univector<complex<fbase>, size> in
      = sin(linspace(0.0, c_pi<fbase, 2> * 4.0, size));
  univector<complex<fbase>, size> out = scalar(qnan);

  // initialize fft
  const dft_plan<fbase> dft(size);

  // allocate work buffer for fft (if needed)
  univector<u8> temp(dft.temp_size);
  auto t0 = clk::now();
  for (int i = 0; i < 10000; i++)
  {
    // perform forward fft
    dft.execute(out, in, temp);

    // and backwards
    dft.execute(in, out, temp);
  }
  auto t1 = clk::now();
  std::cout << " = "
            << std::chrono::duration_cast<milliseconds>(t1 - t0).count()
            << std::endl;

  // scale output
  out = out / size;

  // get magnitude and convert to decibels
  univector<fbase, size> dB = amp_to_dB(cabs(out));

  std::cout << rms(dB) << std::endl;
}

int main()
{
  benchmark_main();
}
