// Minibang / Kabang pitch modulation.
//
// Two defects lived in the same expression:
//
//   if(v.pitch_track >= 0.) penv *= v.pitch_track; else penv /= -v.pitch_track;
//   pitch = clamp(v.pitch + 5.0 * penv * v.pitch_track, 1./5., 5.);
//
//  * Vel->Pitch multiplied the envelope twice, so with the knob at its default
//    of 0 the pitch envelope contributed exactly nothing however it was set.
//  * A negative Vel->Pitch divided by it: at -0.01 the envelope was multiplied
//    by 100 and then by -0.01 again, which drove the ratio into the bottom of
//    the clamp no matter what the Pitch knob said.

#include <Kabang/PitchEnvelope.hpp>

#include <catch2/catch_all.hpp>

using kbng::pitch_scale;

TEST_CASE("the pitch envelope is audible with Vel->Pitch at its default")
{
  // "P. Env" on, Vel->Pitch at 0: the envelope still moves the pitch.
  const double at_rest = pitch_scale(1., 0., true, 0.);
  const double at_peak = pitch_scale(1., 1., true, 0.);

  CHECK(at_rest == Catch::Approx(1.));
  CHECK(at_peak > at_rest);
  CHECK(at_peak == Catch::Approx(4.)); // two octaves up at full envelope
}

TEST_CASE("a small negative Vel->Pitch barely changes anything")
{
  const double neutral = pitch_scale(1., 0.5, true, 0.);
  const double nudged = pitch_scale(1., 0.5, true, -0.01);

  CHECK(nudged == Catch::Approx(neutral).margin(0.03));
  // What used to happen: straight to the bottom of the clamp.
  CHECK(nudged > kbng::min_pitch_scale + 0.1);
}

TEST_CASE("Vel->Pitch scales the envelope's depth in both directions")
{
  const double base = pitch_scale(1., 1., true, 0.);

  CHECK(pitch_scale(1., 1., true, 1.) > base);  // twice the depth
  CHECK(pitch_scale(1., 1., true, -1.) == Catch::Approx(1.)); // fully cancelled
  CHECK(pitch_scale(1., 1., true, -0.5) < base);
}

TEST_CASE("the envelope off leaves the Pitch knob alone")
{
  CHECK(pitch_scale(2., 1., false, 0.5) == Catch::Approx(2.));
  CHECK(pitch_scale(0.5, 1., false, -0.5) == Catch::Approx(0.5));
}

TEST_CASE("the pitch ratio stays inside what the stretcher accepts")
{
  for(double pitch : {0.25, 1., 5.})
    for(double env : {0., 0.5, 1.})
      for(double track : {-1., -0.01, 0., 0.5, 1.})
      {
        const double v = pitch_scale(pitch, env, true, track);
        INFO("pitch=" << pitch << " env=" << env << " track=" << track);
        CHECK(v >= kbng::min_pitch_scale);
        CHECK(v <= kbng::max_pitch_scale);
        CHECK(std::isfinite(v));
      }
}
