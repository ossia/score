#include <Pd/Executor/PdExecutor.hpp>

#include <ossia/dataflow/dataflow.hpp>
#include <ossia/editor/scenario/scenario.hpp>
#include <ossia/editor/scenario/time_event.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/scenario/time_sync.hpp>
#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/local/local.hpp>

#include <QDebug>

#include <z_libpd.h>

#include <thread>
// TODO

class PdDataflowTest
{

public:
  PdDataflowTest()
  {
    libpd_init();
    libpd_set_printhook([](const char* s) { qDebug() << "string: " << s; });
  }

private:
  //  void test_pd()
  //  {
  //    using namespace ossia; using namespace ossia::net;
  //    generic_device dev{std::make_unique<multiplex_protocol>(), ""};
  //    auto& filt_node = create_node(dev.get_root_node(), "/filter");
  //    auto filt_addr = filt_node.create_parameter(ossia::val_type::FLOAT);
  //    filt_addr->push_value(100);
  //    auto& vol_node = create_node(dev.get_root_node(), "/param");
  //    auto vol_addr = vol_node.create_parameter(ossia::val_type::FLOAT);
  //    vol_addr->push_value(0.5);
  //    auto& note_node = create_node(dev.get_root_node(), "/note");
  //    auto note_addr = note_node.create_parameter(ossia::val_type::FLOAT);
  //    note_addr->push_value(220.);
  //    auto& l_node = create_node(dev.get_root_node(), "/l");
  //    auto l_addr = l_node.create_parameter(ossia::val_type::IMPULSE);
  //    auto& r_node = create_node(dev.get_root_node(), "/r");
  //    auto r_addr = r_node.create_parameter(ossia::val_type::IMPULSE);

  //    auto graph = std::make_shared<ossia::graph>();
  //    ossia::graph& g = *graph;
  //    using string_vec = std::vector<std::string>;
  //    auto cwd = getcwd(NULL, 0);

  //    using namespace Pd;

  //    auto f1 = std::make_shared<PdGraphNode>(cwd, "gen1.pd", 0, 0, 0, 1,
  //    string_vec{}, string_vec{"out-1"}); auto f1_2 =
  //    std::make_shared<PdGraphNode>(cwd, "gen1.pd", 0, 0, 0, 1, string_vec{},
  //    string_vec{"out-1"}); auto f2 = std::make_shared<PdGraphNode>(cwd,
  //    "gen2.pd", 0, 0, 1, 1, string_vec{"in-1"}, string_vec{"out-1"}); auto
  //    f3 = std::make_shared<PdGraphNode>(cwd, "gen3.pd", 0, 2, 2, 0,
  //    string_vec{"in-1", "in-2"}, string_vec{}); auto f4 =
  //    std::make_shared<PdGraphNode>(cwd, "gen4.pd", 2, 2, 1, 0,
  //    string_vec{"in-2"}, string_vec{}); auto f5 =
  //    std::make_shared<PdGraphNode>(cwd, "gen5.pd", 2, 2, 1, 0,
  //    string_vec{"in-2"}, string_vec{}); auto f5_2 =
  //    std::make_shared<PdGraphNode>(cwd, "gen5.pd", 2, 2, 1, 0,
  //    string_vec{"in-2"}, string_vec{});

  //    g.add_node(f1);
  //    g.add_node(f1_2);
  //    g.add_node(f2);
  //    g.add_node(f3);
  //    g.add_node(f4);
  //    g.add_node(f5);
  //    g.add_node(f5_2);

  //    f1->outputs()[0]->address = note_addr;
  //    f1_2->outputs()[0]->address = note_addr;
  //    f2->outputs()[0]->address = note_addr;

  //    f3->inputs()[0]->address = note_addr;
  //    f3->inputs()[1]->address = vol_addr;
  //    f3->outputs()[0]->address = l_addr;
  //    f3->outputs()[1]->address = r_addr;

  //    f4->inputs()[0]->address = l_addr;
  //    f4->inputs()[1]->address = r_addr;
  //    f4->inputs()[2]->address = filt_addr;
  //    f4->outputs()[0]->address = l_addr;
  //    f4->outputs()[1]->address = r_addr;

  //    f5->inputs()[0]->address = l_addr;
  //    f5->inputs()[1]->address = r_addr;
  //    f5->inputs()[2]->address = filt_addr;
  //    f5->outputs()[0]->address = l_addr;
  //    f5->outputs()[1]->address = r_addr;

  //    f5_2->inputs()[0]->address = l_addr;
  //    f5_2->inputs()[1]->address = r_addr;
  //    f5_2->inputs()[2]->address = filt_addr;
  //    f5_2->outputs()[0]->address = l_addr;
  //    f5_2->outputs()[1]->address = r_addr;

  //    g.connect(make_edge(immediate_strict_connection{}, f3->outputs()[0],
  //    f4->inputs()[0], f3, f4));
  //    g.connect(make_edge(immediate_strict_connection{}, f3->outputs()[1],
  //    f4->inputs()[1], f3, f4));

  //    g.connect(make_edge(immediate_glutton_connection{}, f4->outputs()[0],
  //    f5->inputs()[0], f4, f5));
  //    g.connect(make_edge(immediate_glutton_connection{}, f4->outputs()[1],
  //    f5->inputs()[1], f4, f5));

  //    g.connect(make_edge(delayed_strict_connection{}, f4->outputs()[0],
  //    f5_2->inputs()[0], f4, f5_2));
  //    g.connect(make_edge(delayed_strict_connection{}, f4->outputs()[1],
  //    f5_2->inputs()[1], f4, f5_2));

  //    std::vector<float> samples;

  //    execution_state st;
  //    auto copy_samples = [&] {
  //      auto it_l = st.audioState.find(l_addr);
  //      auto it_r = st.audioState.find(r_addr);
  //      if(it_l == st.audioState.end() || it_r == st.audioState.end())
  //      {
  //        // Just copy silence
  //        samples.resize(samples.size() + 128);
  //        return;
  //      }

  //      auto& audio_l = it_l->second;
  //      auto& audio_r = it_r->second;
  //      QVERIFY(!audio_l.samples.empty());
  //      QVERIFY(!audio_r.samples.empty());

  //      auto pos = samples.size();
  //      samples.resize(samples.size() + 128);
  //      /* TODO reinstate me with channels
  //      for(int i = 0; i < 64; i++)
  //        samples[pos + i * 2] += audio_l.channel(i);
  //      for(int i = 0; i < 64; i++)
  //        samples[pos + i * 2 + 1] += audio_r.channel(i);
  //        */
  //    };

  //    // Create an ossia scenario
  //    auto main_start_node = std::make_shared<time_sync>();
  //    auto main_end_node = std::make_shared<time_sync>();

  //    // create time_events inside Syncs and make them interactive to the
  //    /play address auto main_start_event =
  //    *(main_start_node->emplace(main_start_node->get_time_events().begin(),
  //    {})); auto main_end_event =
  //    *(main_end_node->emplace(main_end_node->get_time_events().begin(),
  //    {}));

  //    const ossia::time_value granul{1000. * 64. / 44100.};
  //    // create the main time_interval

  //    auto cb = [] (double t0, ossia::time_value, const state_element&) {
  //    };

  //    auto cb_2 = [] (double t0, ossia::time_value, const state_element&) {
  //    };
  //    auto main_constraint = std::make_shared<time_interval>(
  //          cb,
  //          *main_start_event,
  //          *main_end_event,
  //          20000_tv,
  //          20000_tv,
  //          20000_tv);

  //    auto main_scenario_ptr = std::make_unique<scenario>();
  //    scenario& main_scenario = *main_scenario_ptr;
  //    main_constraint->add_time_process(std::move(main_scenario_ptr));

  //    auto make_constraint = [&] (auto time, auto s, auto e)
  //    {
  //      ossia::time_value tv{(double)time};
  //      auto cst = std::make_shared<time_interval>(cb_2, *s, *e, tv, tv, tv);
  //      //cst->set_granularity(granul);
  //      s->next_time_intervals().push_back(cst);
  //      e->previous_time_intervals().push_back(cst);
  //      main_scenario.add_time_interval(cst);
  //      return cst;
  //    };

  //    std::vector<std::shared_ptr<time_sync>> t(15); std::generate(t.begin(),
  //    t.end(), [&] {
  //      auto tn = std::make_shared<time_sync>();
  //      main_scenario.add_time_sync(tn);
  //      return tn;
  //    });

  //    t[0] = main_scenario.get_start_time_sync();

  //    std::vector<std::shared_ptr<time_event>> e(15);
  //    for(int i = 0; i < t.size(); i++) e[i] =
  //    *t[i]->emplace(t[i]->get_time_events().begin(), {});

  //    std::vector<std::shared_ptr<time_interval>> c(14);
  //    c[0] = make_constraint(1000, e[0], e[1]);
  //    c[1] = make_constraint(2000, e[1], e[2]);
  //    c[2] = make_constraint(1500, e[2], e[3]);
  //    c[3] = make_constraint(1000, e[3], e[4]);
  //    c[4] = make_constraint(500, e[4], e[5]);
  //    c[5] = make_constraint(3000, e[5], e[6]);

  //    c[6] = make_constraint(500, e[0], e[7]);
  //    c[7] = make_constraint(10000, e[7], e[8]);

  //    c[8] = make_constraint(3000, e[0], e[9]);
  //    c[9] = make_constraint(4000, e[9], e[10]);

  //    c[10] = make_constraint(1000, e[9], e[11]);
  //    c[11] = make_constraint(3000, e[11], e[12]);
  //    c[12] = make_constraint(1000, e[12], e[13]);
  //    c[13] = make_constraint(3000, e[13], e[14]);

  //    c[1]->add_time_process(std::make_shared<node_process>(graph, f1));
  //    c[3]->add_time_process(std::make_shared<node_process>(graph, f2));
  //    c[5]->add_time_process(std::make_shared<node_process>(graph, f1_2));

  //    c[7]->add_time_process(std::make_shared<node_process>(graph, f3));

  //    c[9]->add_time_process(std::make_shared<node_process>(graph, f4));

  //    c[11]->add_time_process(std::make_shared<node_process>(graph, f5));
  //    c[13]->add_time_process(std::make_shared<node_process>(graph, f5_2));

  //    main_constraint->set_speed(1.0);

  //    main_constraint->offset(0.000000001_tv);
  //    main_constraint->start();

  //    const ossia::time_value rate{1000000. * 64. / 44100.};
  //    for(int i = 0; i < 15  * 44100. / 64.; i++)
  //    {
  //      main_constraint->tick(rate);

  //      g.state(st);
  //      copy_samples();
  //      st.clear();

  //      std::this_thread::sleep_for(std::chrono::microseconds(int64_t(1000000.
  //      * 64. / 44100.)));

  //    }

  //    qDebug() << samples.size();
  //    QVERIFY(samples.size() > 0);
  //    SndfileHandle file("/tmp/out.wav", SFM_WRITE, SF_FORMAT_WAV |
  //    SF_FORMAT_PCM_16, 2, 44100); file.write(samples.data(),
  //    samples.size()); file.writeSync();

  //  }
};

int main() { }
