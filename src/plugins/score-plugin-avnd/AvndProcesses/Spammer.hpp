#include <ossia/detail/jthread.hpp>
#include <ossia/detail/triple_buffer.hpp>

#include <AvndProcesses/AddressTools.hpp>

namespace avnd_tools
{

struct Spammer : PatternObject
{
  halp_meta(name, "Spammer")
  halp_meta(author, "ossia team")
  halp_meta(category, "Control/Data processing")
  halp_meta(description, "Send a message at a given frequency")
  halp_meta(c_name, "avnd_pattern_spam")
  halp_meta(uuid, "0fa9415b-dcda-4ab8-b3f5-353c5d35fc8a")

  struct
  {
    halp::val_port<"Input", ossia::value> input;

    struct : halp::time_chooser<"Delay", halp::range{0.0001, 30., 0.2}>
    {
      using mapper = halp::log_mapper<std::ratio<95, 100>>;
    } delay;

    PatternSelector pattern;
    halp::knob_f32<"Smooth", halp::range{0., 1., 0.5}> smooth;
  } inputs;

  struct
  {

  } outputs;

  void update()
  {
    m_thread = std::jthread{};
    inputs.pattern.reprocess();
    m_thread = std::jthread([this, roots = this->roots, cur = ossia::value{},
                             smooth = ossia::value{}](std::stop_token tk) mutable {
      while(!tk.stop_requested())
      {
        std::this_thread::sleep_for(m_delay.load(std::memory_order_relaxed));
        float s = m_smooth.load(std::memory_order_relaxed);
        this->m_buffer.consume(cur);

        smooth_recursively(smooth, cur, 1.f - s);
        cur = smooth;

        if(cur.valid())
        {
          for(auto p : roots)
          {
            p->get_parameter()->push_value(cur);
          }
        }
      }
    });
  }

  void operator()()
  {
    if(!m_path)
      return;

    m_smooth.store(inputs.smooth.value, std::memory_order_relaxed);
    m_buffer.produce(inputs.input.value);

    m_delay.store(
        std::chrono::nanoseconds(int64_t(1e9 * inputs.delay.value)),
        std::memory_order_release);
    if(!m_thread.joinable() || inputs.pattern.devices_dirty)
    {
      update();
    }
  }

  struct do_smooth
  {
    float a{};
    void operator()(const int& in, int& filtered)
    {
      filtered = in * a + filtered * (1.0f - a);
    }

    void operator()(const float& in, float& filtered)
    {
      filtered = in * a + filtered * (1.0f - a);
    }

    void operator()(const bool& in, bool& filtered)
    {
      filtered = in * a + filtered * (1.0f - a);
    }

    template <std::size_t N>
    void operator()(const std::array<float, N>& in, std::array<float, N>& filtered)
    {
      for(std::size_t i = 0; i < N; i++)
        filtered[i] = in[i] * a + filtered[i] * (1.0f - a);
    }

    void
    operator()(const std::vector<ossia::value>& in, std::vector<ossia::value>& filtered)
    {
      if(in.size() != filtered.size()) [[unlikely]]
        filtered.resize(in.size());

      for(std::size_t i = 0; i < in.size(); i++)
      {
        if(filtered[i].get_type() != in[i].get_type()) [[unlikely]]
          filtered[i] = in[i];
        else
          ossia::apply(*this, in[i], filtered[i]);
      }
    }

    void operator()(const std::string& in, std::string& filtered) { filtered = in; }
    template <typename T>
    void operator()(const T& in, T& filtered)
    {
      filtered = in;
    }

    template <typename T>
    void operator()(const ossia::impulse& in, T& filtered)
    {
      (*this)(filtered, filtered);
    }

    void operator()(const ossia::impulse& in, ossia::impulse& filtered) { }
    template <typename T, typename U>
    void operator()(const T& in, U& filtered)
    {
    }
  };

  static void
  smooth_recursively(ossia::value& prev, const ossia::value& next, float alpha)
  {
    if(prev.get_type() != next.get_type()) [[unlikely]]
      prev = next;
    else [[likely]]
      ossia::apply(do_smooth{alpha}, next, prev);
  }

  static_assert(std::atomic<std::chrono::nanoseconds>::is_always_lock_free);
  std::atomic<std::chrono::nanoseconds> m_delay{std::chrono::nanoseconds{100000}};
  std::atomic<float> m_smooth{};
  std::jthread m_thread;
  ossia::triple_buffer<ossia::value> m_buffer{ossia::value{}};

  ossia::value m_prev;
};
}
