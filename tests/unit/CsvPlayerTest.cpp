// The CSV object's reader, driven directly: a file written the way the
// recorder writes it, read back into a device's parameters.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <core/document/Document.hpp>

#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/local/local.hpp>
#include <ossia/network/base/parameter_data.hpp>
#include <ossia/network/common/complex_type.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <AvndProcesses/DeviceRecorder.hpp>
#include <catch2/catch_test_macros.hpp>

#include <score/tools/ThreadPool.hpp>

#include <atomic>
#include <chrono>
#include <thread>

namespace
{
struct Fixture
{
  ossia::net::generic_device dev{std::make_unique<ossia::net::multiplex_protocol>(), "dev"};
  ossia::net::parameter_base* a{};
  ossia::net::parameter_base* b{};
  Fixture()
  {
    a = ossia::create_parameter(dev.get_root_node(), "/foo/a", "float");
    b = ossia::create_parameter(dev.get_root_node(), "/foo/b", "int");
  }
};

QString write(QTemporaryDir& dir, const char* text)
{
  const QString path = dir.filePath("in.csv");
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(text);
  return path;
}
}

TEST_CASE("CSV: reads back what the recorder writes", "[csv]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    Fixture fx;
    QTemporaryDir dir;

    for(bool timestamped : {true, false})
    {
      CAPTURE(timestamped);
      fx.a->push_value(0.f);
      fx.b->push_value(0);
      const QString path = write(
          dir, timestamped ? "timestamp,/foo/a,/foo/b\n0,1.5,3\n1,2.5,4\n"
                           : "/foo/a,/foo/b\n1.5,3\n2.5,4\n");

      avnd_tools::DeviceRecorder::player_thread p{doc->context()};
      p.filename = path.toStdString();
      p.roots = {&fx.a->get_node(), &fx.b->get_node()};
      p.first_is_timestamp = timestamped;
      p.setSeparator(avnd_tools::DeviceRecorder::Colon);
      p.setActive(true);

      INFO("mapped columns: " << p.m_map.size());
      p.read();
      CHECK(ossia::convert<float>(fx.a->value()) == 1.5f);
      CHECK(ossia::convert<int>(fx.b->value()) == 3);
    }
  });
}

TEST_CASE("CSV: a recording plays back", "[csv]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    ossia::net::generic_device dev{std::make_unique<ossia::net::multiplex_protocol>(), "dev"};
    auto* a = ossia::create_parameter(dev.get_root_node(), "/foo/a", "float");
    auto* b = ossia::create_parameter(dev.get_root_node(), "/foo/b", "int");
    auto* s = ossia::create_parameter(dev.get_root_node(), "/foo/s", "string");
    auto* v = ossia::create_parameter(dev.get_root_node(), "/foo/v", "vec3f");
    std::vector<ossia::net::node_base*> roots{
        &a->get_node(), &b->get_node(), &s->get_node(), &v->get_node()};
    QTemporaryDir dir;
    const std::string path = dir.filePath("rec.csv").toStdString();

    for(bool timestamped : {true, false})
    {
      CAPTURE(timestamped);
      {
        avnd_tools::DeviceRecorder::recorder_thread r{doc->context()};
        r.filename = path;
        r.roots = roots;
        r.first_is_timestamp = timestamped;
        r.setSeparator(avnd_tools::DeviceRecorder::Colon);
        r.setActive(true);
        a->push_value(1.5f);
        b->push_value(3);
        s->push_value(std::string{"hello, world"});
        v->push_value(ossia::vec3f{1.f, 2.f, 3.f});
        r.write(0);
        r.setActive(false);
      }
      QFile f{QString::fromStdString(path)};
      REQUIRE(f.open(QIODevice::ReadOnly));
      const QByteArray written = f.readAll();
      INFO("written:\n" << written.toStdString());

      a->push_value(0.f);
      b->push_value(0);
      s->push_value(std::string{});
      v->push_value(ossia::vec3f{});

      avnd_tools::DeviceRecorder::player_thread p{doc->context()};
      p.filename = path;
      p.roots = roots;
      p.first_is_timestamp = timestamped;
      p.setSeparator(avnd_tools::DeviceRecorder::Colon);
      p.setActive(true);
      INFO("mapped columns: " << p.m_map.size());
      p.read();
      CHECK(ossia::convert<float>(a->value()) == 1.5f);
      CHECK(ossia::convert<int>(b->value()) == 3);
      CHECK(ossia::convert<std::string>(s->value()) == "hello, world");
      CHECK(v->value() == ossia::value{ossia::vec3f{1.f, 2.f, 3.f}});
    }
  });
}

// The binding runs the object's messages on score's task pool, several
// threads: the file accesses must be serialized, or the header is written
// twice, interleaved, and playback matches nothing.
TEST_CASE("CSV: the object records, then plays back", "[csv]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    ossia::net::generic_device dev{std::make_unique<ossia::net::multiplex_protocol>(), "dev"};
    auto* a = ossia::create_parameter(dev.get_root_node(), "/foo/a", "float");
    auto* b = ossia::create_parameter(dev.get_root_node(), "/foo/b", "int");
    ossia::execution_state st;
    st.register_device(&dev);
    st.apply_device_changes();
    QTemporaryDir dir;
    const std::string path = dir.filePath("obj.csv").toStdString();

    using Obj = avnd_tools::DeviceRecorder;
    // What the binding does: each worker message is a task on the pool
    std::atomic_int pending{0};
    auto drain = [&] {
      const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
      while(pending > 0 && std::chrono::steady_clock::now() < end)
        std::this_thread::yield();
      REQUIRE(pending == 0);
    };
    auto make = [&](auto mode) {
      auto o = std::make_unique<Obj>();
      o->ossia_document_context = &doc->context();
      o->ossia_state = {&st};
      o->worker.request = [&pending](Obj::worker_message m) {
        auto msg = std::make_shared<Obj::worker_message>(std::move(m));
        pending++;
        score::TaskPool::instance().post([msg, &pending] {
          decltype(Obj{}.worker)::work(std::move(*msg));
          pending--;
        });
      };
      o->inputs.pattern.value = "/foo/*";
      o->inputs.pattern.update(*o);
      o->inputs.filename.value = path;
      o->inputs.mode.value = mode;
      o->inputs.time.value = 0.f;
      o->prepare();
      return o;
    };
    auto tick = [&](Obj& o, int64_t ns) {
      halp::tick_musical tk{};
      tk.frames = 64;
      tk.position_in_nanoseconds = ns;
      o(tk);
      drain();
    };
    using Mode = decltype(Obj::inputs_t{}.mode.value);

    {
      auto rec = make(Mode::Record);
      a->push_value(1.5f);
      b->push_value(3);
      tick(*rec, 0);
      a->push_value(2.5f);
      b->push_value(4);
      tick(*rec, 1'000'000);
      rec->inputs.mode.value = Mode::None;
      rec->setMode();
      drain();
    }
    QFile f{QString::fromStdString(path)};
    REQUIRE(f.open(QIODevice::ReadOnly));
    INFO("written:\n" << f.readAll().toStdString());

    a->push_value(0.f);
    b->push_value(0);
    auto play = make(Mode::Playback);
    tick(*play, 0);
    tick(*play, 1'000'000);
    // Rows are timestamped by the wall clock: either one may be played back,
    // but always as a whole
    const float av = ossia::convert<float>(a->value());
    const int bv = ossia::convert<int>(b->value());
    CHECK(((av == 1.5f && bv == 3) || (av == 2.5f && bv == 4)));
  });
}
