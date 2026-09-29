// Which frame a pass of a multi-pass ISF shader sees when it samples a
// PERSISTENT pass.
//
// The reference is VVISF (ISFScene::_render): every pass renders into a fresh
// buffer that replaces the target's buffer as soon as the pass is drawn, and
// the persistent buffers are bound again before each pass. So within a frame a
// pass sees what an EARLIER pass rendered this frame, and the writer itself
// and the passes before it see the previous frame.
//
// RenderedISFNode implements it with a pair of textures per persistent pass
// and two pass sets that swap every frame: the main set (parity 0) renders a
// persistent pass into textures[0], the alternate set into textures[1], and
// the texture a pass samples is persistentTextureIndex(writer, reader,
// parity). A persistent last pass is then copied to the output by a blit that
// comes after every pass.
//
// Pure CPU: both schedules are simulated over every shader of up to four
// passes (each pass persistent or not, each reading any subset of the passes
// VVISF defines a value for) and must agree on what every pass draws and on
// the output, frame after frame. Reads of a non-persistent pass by itself or
// by an earlier pass are left out: VVISF empties those buffers at the end of
// each frame.
#include <Gfx/Graph/RenderedISFNode.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
using score::gfx::persistentTextureIndex;

struct Shader
{
  std::vector<bool> persistent;
  std::vector<std::vector<int>> reads;
};

// What a pass renders: a function of the pass, the frame and what it read, so
// that two schedules agree only if every pass read the same values.
std::uint64_t draw(int pass, int frame, const std::vector<std::uint64_t>& inputs)
{
  std::uint64_t h = 1469598103934665603ull ^ (std::uint64_t(pass) << 32 | unsigned(frame));
  for(auto v : inputs)
    h = (h ^ v) * 1099511628211ull + 0x9e3779b97f4a7c15ull;
  return h;
}

// VVISF: one buffer per pass, replaced after the pass is drawn. A last pass
// draws into the output. Returns what every pass drew, then the output, for
// every frame.
std::vector<std::uint64_t> runVVISF(const Shader& s, int frames)
{
  const int N = (int)s.persistent.size();
  std::vector<std::uint64_t> buf(N, 0), out;
  for(int f = 0; f < frames; f++)
  {
    for(int j = 0; j < N; j++)
    {
      std::vector<std::uint64_t> in;
      for(int r : s.reads[j])
        in.push_back(buf[r]);
      buf[j] = draw(j, f, in);
      out.push_back(buf[j]);
    }
    out.push_back(buf[N - 1]);
  }
  return out;
}

// RenderedISFNode: textures[2] per pass (one texture when not persistent),
// frame parity alternating from 0 after initialization. Same record as
// runVVISF.
std::vector<std::uint64_t> runPingPong(const Shader& s, int frames)
{
  const int N = (int)s.persistent.size();
  std::vector<std::array<std::uint64_t, 2>> tex(N, {0, 0});
  std::vector<std::uint64_t> out;
  for(int f = 0; f < frames; f++)
  {
    const int parity = f % 2;
    auto sample = [&](int writer, int reader) {
      return s.persistent[writer]
                 ? tex[writer][persistentTextureIndex(writer, reader, parity)]
                 : tex[writer][0];
    };
    for(int j = 0; j < N; j++)
    {
      std::vector<std::uint64_t> in;
      for(int r : s.reads[j])
        in.push_back(sample(r, j));
      tex[j][s.persistent[j] ? parity : 0] = draw(j, f, in);
      out.push_back(tex[j][s.persistent[j] ? parity : 0]);
    }
    out.push_back(sample(N - 1, N));
  }
  return out;
}

std::string describe(const Shader& s)
{
  std::string d;
  for(std::size_t j = 0; j < s.persistent.size(); j++)
  {
    d += "pass" + std::to_string(j) + (s.persistent[j] ? "(P) reads {" : " reads {");
    for(int r : s.reads[j])
      d += std::to_string(r) + " ";
    d += "} ";
  }
  return d;
}
}

TEST_CASE(
    "every multi-pass shader sees its persistent passes as VVISF does",
    "[isf][persistent]")
{
  constexpr int frames = 6;
  int checked = 0;
  for(int N = 1; N <= 4; N++)
  {
    for(unsigned pmask = 0; pmask < (1u << N); pmask++)
    {
      Shader s;
      for(int j = 0; j < N; j++)
        s.persistent.push_back(pmask & (1u << j));

      // Each pass j may read every persistent pass and the non-persistent
      // passes before it: enumerate one subset per pass.
      std::vector<std::vector<int>> readable(N);
      for(int j = 0; j < N; j++)
        for(int r = 0; r < N; r++)
          if(s.persistent[r] || r < j)
            readable[j].push_back(r);

      std::vector<unsigned> subset(N, 0);
      while(true)
      {
        s.reads.assign(N, {});
        for(int j = 0; j < N; j++)
          for(std::size_t b = 0; b < readable[j].size(); b++)
            if(subset[j] & (1u << b))
              s.reads[j].push_back(readable[j][b]);

        const auto expected = runVVISF(s, frames);
        const auto got = runPingPong(s, frames);
        if(expected != got)
        {
          INFO(describe(s));
          REQUIRE(expected == got);
        }
        ++checked;

        int j = 0;
        for(; j < N; j++)
        {
          if(++subset[j] < (1u << readable[j].size()))
            break;
          subset[j] = 0;
        }
        if(j == N)
          break;
      }
    }
  }
  CHECK(checked == 147993);
}

// The frame numbers themselves, for the MultiFrame presets: three persistent
// passes copying each other from the last to the first, then a pass showing
// them. The output holds the current frame and the two before it, in order.
TEST_CASE("a chain of persistent passes delays by one frame per stage", "[isf][persistent]")
{
  // pass0 = buffer3 <- buffer2, pass1 = buffer2 <- buffer1, pass2 = buffer1 <-
  // the frame number, pass3 = the output <- (buffer1, buffer2, buffer3).
  std::array<std::array<int, 2>, 3> tex{};
  for(int f = 0; f < 6; f++)
  {
    const int parity = f % 2;
    auto sample = [&](int writer, int reader) {
      return tex[writer][persistentTextureIndex(writer, reader, parity)];
    };
    const int b3 = sample(1, 0);
    tex[0][parity] = b3;
    const int b2 = sample(2, 1);
    tex[1][parity] = b2;
    tex[2][parity] = f + 1;
    const std::array<int, 3> shown{sample(2, 3), sample(1, 3), sample(0, 3)};
    INFO("frame " << f + 1);
    CHECK(shown == std::array<int, 3>{f + 1, f, std::max(f - 1, 0)});
  }
}
