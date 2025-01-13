// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "IdentifierGeneration.hpp"

#include <cstdint>
#include <limits>
#include <random>

namespace score
{
// Older valgrind versions would crash with random number generation
#ifdef SCORE_VALGRIND_IDS
int32_t random_id_generator::getRandomId()
{
  static int x = 15;
  return x++;
}

#else

struct IdGen
{
  std::random_device rd;
  std::mt19937 gen;
  std::uniform_int_distribution<int32_t> dist;

  IdGen() noexcept
      : rd{}
      , gen{rd()}
      , dist{std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()}
  {
  }

  int32_t make() noexcept { return dist(gen); }
};

static IdGen g_idgen;
int32_t random_id_generator::getRandomId()
{
  using namespace std;

  return g_idgen.make();
}

#endif
}
