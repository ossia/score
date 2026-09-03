// JS::AudioOutlet::setChannel(index, value) -- the one call a Javascript
// process uses to produce audio -- handed something that is not an array.
//
// QmlObjects.cpp:282 reads v.property("length") and converts it to int with no
// check. On a number, a string that is not an array, null or an object, that
// property is undefined, QJSValue::toNumber() is NaN, and converting a NaN to
// int is undefined behaviour: on x86-64 it yields INT_MIN, which then reaches
// QVector::resize(). Measured:
//
//   QmlObjects.cpp:282:11: runtime error: nan is outside the range of
//     representable values of type 'int'
//   Fatal: ASSERT: "newSize >= 0" in file qlist.h, line 830
//   terminate called without an active exception
//
// The script of a Javascript process is user content -- it arrives in a .score
// file like any other -- so `out.setChannel(0, 42)` is enough to take the
// application down from a document.
//
// FIXED: setChannel validates its argument now, so this runs to the end and
// exits 0 on Linux. The scaffolding below is kept as a regression guard, not
// because it is load-bearing today.
//
// Its own executable because the abort killed the process: kept inside
// JsCpuNodeTest it truncated that binary's run and every case after it went
// unreported. [!shouldfail] cannot help -- Catch2 never gets to report -- and
// ctest's WILL_FAIL does not invert a test that died on a signal, it reports
// "Subprocess aborted". So the test installs its own SIGABRT handler, over the
// one Catch2 installs on entering a test case, and turns the abort into a
// plain exit(1).
//
// The paragraph that stood here said the entry going red would be "the signal
// to drop WILL_FAIL". No WILL_FAIL was ever registered on this test, so that
// signal could not fire -- the defect was fixed and the bookkeeping was never
// done. See tests/integration/CMakeLists.txt.
//
// STILL BROKEN ON WINDOWS: this test TIMES OUT there. Live defect, separate
// from the abort, and consequential -- a ctest timeout kills score before it
// clears the failsafe bit, forcing every later launch in the run into failsafe
// mode with opengl disabled.

#include <score_test/App.hpp>

#include <JS/Executor/CPUNode.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/token_request.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <csignal>
#include <cstdio>
#include <unistd.h>

using Catch::Approx;

namespace
{
extern "C" void on_abort(int)
{
  static const char msg[] = "setChannel aborted the process\n";
  ssize_t ignored = ::write(2, msg, sizeof(msg) - 1);
  (void)ignored;
  ::_exit(1);
}
}

TEST_CASE("A Javascript audio outlet handed a non-array survives",
          "[integration][js][gui][cpu]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext&) {
    std::signal(SIGABRT, on_abort);

    ossia::execution_state st;
    st.sampleRate = 48000;
    st.bufferSize = 64;

    auto node = std::make_shared<JS::js_node>(st);
    node->root_outputs().push_back(new ossia::audio_outlet);
    node->setScript({}, QStringLiteral(R"_(import Score
Script {
  AudioOutlet { id: out; objectName: "out" }
  tick: function(token, state) {
    out.setChannel(0, 42);
    out.setChannel(0, null);
    out.setChannel(0, {});
    out.setChannel(0, [0.25, 0.5]);
  }
})_"));

    ossia::token_request tk;
    tk.prev_date = ossia::time_value{0};
    tk.date = ossia::time_value{705600000 / 48000 * 64};
    tk.parent_duration = ossia::time_value{705600000};
    tk.speed = 1.;
    tk.tempo = 120.;
    tk.signature = ossia::time_signature{4, 4};
    tk.start_sample = 0;
    tk.length_sample = 64;

    node->run(tk, ossia::exec_state_facade{&st});

    auto& out = node->root_outputs()[0]->target<ossia::audio_port>()->get();
    REQUIRE(out.size() >= 1);
    REQUIRE(out[0].size() == 2);
    CHECK(out[0][0] == Approx(0.25));
    CHECK(out[0][1] == Approx(0.5));

    node->clear();
  });
}
