// JS::AudioOutlet::setChannel(index, value) -- the one call a Javascript
// process uses to produce audio -- handed something that is not an array.
//
// setChannel reads v.property("length"): on a number, a string that is not an
// array, null or an object that property is undefined, QJSValue::toNumber() is
// NaN, and converting a NaN to int is undefined behaviour -- on x86-64 it
// yields INT_MIN, which then reaches QVector::resize() and aborts the process.
// The length is range-checked before the resize; this case holds that check in
// place.
//
// The script of a Javascript process is user content -- it arrives in a .score
// file like any other -- so `out.setChannel(0, 42)` would otherwise be enough
// to take the application down from a document.
//
// Its own executable, because an abort truncates the run of the binary it
// shares and every case after it goes unreported. [!shouldfail] cannot help --
// Catch2 never gets to report -- and ctest's WILL_FAIL does not invert a test
// that died on a signal, it reports "Subprocess aborted". So the test installs
// its own SIGABRT handler, over the one Catch2 installs on entering a test
// case, and turns the abort into a plain exit(1).
//
// On Windows it needs SCORE_DISABLE_FAILSAFE: with score's sticky failsafe bit
// set, the run times out.

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
