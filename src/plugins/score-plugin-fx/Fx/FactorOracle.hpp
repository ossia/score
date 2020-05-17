#pragma once
#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/math.hpp>

#include <QDebug>

#include <Engine/Node/PdNode.hpp>
#include <rnd/random.hpp>
#if !defined(NDEBUG) && !defined(_MSC_VER) && !defined(__clang__)
#include <debug/vector>
#define debug_vector_t __gnu_debug::vector
#else
#define debug_vector_t std::vector
#endif
namespace Nodes::FactorOracle
{

template <typename T, T default_value>
struct safe_vector
{
public:
  debug_vector_t<T> impl;

  T& operator[](int i_) { return (*this)[static_cast<std::size_t>(i_)]; }
  T& operator[](std::size_t i_)
  {
    auto i = static_cast<std::size_t>(i_);
    if (i < impl.size() && impl.size() != 0)
    {
      return impl[i];
    }
    impl.resize((i + 1) * 2, default_value);
    return impl[i];
  }

  const T& operator[](int i_) const { return (*this)[static_cast<std::size_t>(i_)]; }
  const T& operator[](std::size_t i_) const
  {
    auto i = static_cast<std::size_t>(i_);
    if (i < impl.size() && impl.size() != 0)
    {
      return impl[i];
    }
    static constexpr auto dval = default_value;
    return dval;
  }
};
template <typename T>
struct safe_vector_simple
{
public:
  debug_vector_t<T> impl;

  T& operator[](int i_) { return (*this)[static_cast<std::size_t>(i_)]; }
  T& operator[](std::size_t i_)
  {
    auto i = static_cast<std::size_t>(i_);
    if (i < impl.size() && impl.size() != 0)
    {
      return impl[i];
    }
    impl.resize((i + 1) * 2);
    return impl[i];
  }
};

class FactorOracle
{
public:
  int cur_alphabet_size = 0;
  debug_vector_t<std::pair<int, ossia::value>> value_map;
  FactorOracle(int sz = 0) : m_forwardLink(sz + 1, debug_vector_t<int>{-1}), m_rand_engine{}
  {
    m_sp.impl.resize(1000);
    m_lrs.impl.resize(1000);
    m_sp[0] = -1;
    m_lrs[0] = 0;
  }

  /// Fonction LCS (longest commun suffix)
  int LCS(int p1, int p2)
  {
    if (p2 == m_sp[p1])
      return m_lrs[p1];

    while (m_sp[p2] != m_sp[p1])
      p2 = m_sp[p2];

    return std::min(m_lrs[p1], m_lrs[p2]);
  }

  void add_char(ossia::value c)
  {
    if (n < (int)m_forwardLink.size() - 1)
    {
      m_sequence.push_back(std::move(c));
      auto it = ossia::find_if(value_map, [&](const auto& pair) { return pair.second == c; });
      if (it != value_map.end())
      {
        add_state(it->first);
      }
      else
      {
        value_map.push_back({cur_alphabet_size, m_sequence.back()});
        add_state(cur_alphabet_size);
        cur_alphabet_size++;
      }
    }
  }

  void add_state(int c)
  {
    m_trans[n][c] = n + 1;
    int p1 = n;
    int j = m_sp[p1];
    ++n;
    while (j != -1 && m_trans[j][c] == -1)
    {
      int& m_trans_j_c = m_trans[j][c];
      m_trans_j_c = n;
      if (m_forwardLink[j][0] == -1)
      {
        m_forwardLink[j][0] = m_trans_j_c;
      }
      else
      {
        m_forwardLink[j].push_back(m_trans_j_c);
      }
      p1 = j;
      j = m_sp[p1];
    }
    int& m_sp_n = m_sp[n];
    m_sp_n = (j == -1 ? 0 : m_trans[j][c]);
    m_lrs[n] = (m_sp_n == 0 ? 0 : LCS(p1, m_sp_n - 1) + 1);
  }

  debug_vector_t<ossia::value> make_rand_sequence(float continuity, int seqSize) const
  {
    auto start = std::uniform_int_distribution<std::size_t>{0, m_sequence.size()}(m_rand_engine);
    return make_sequence(continuity, start, seqSize);
  }

  debug_vector_t<ossia::value>
  make_sequence(float continuity, std::size_t curState, std::size_t seqSize) const
  {
    if (curState > m_sequence.size())
    {
      qDebug() << "Le point initial de l'improvisation doit être comprise "
                  "dans la séquence";
      return {};
    }

    debug_vector_t<ossia::value> v;
    v.reserve(seqSize);
    for (std::size_t i = 0; i < seqSize; i++)
    {
      auto f = std::uniform_real_distribution<float>{}(m_rand_engine);
      if (f <= continuity && curState < m_sequence.size() - 1)
      {
        curState++;
        v.push_back(m_sequence[curState]);
      }
      else
      {
        do
        {
          int links = (curState == 0 ? 0 : 1);
          if (m_forwardLink[curState][0] != -1)
          {
            links += m_forwardLink[curState].size();
          }

          auto linkToFollow = std::uniform_int_distribution<int>{0, links - 1}(m_rand_engine);
          if (linkToFollow == links - 1)
          {
            if (curState != 0)
            {
              curState = m_sp[curState];
            }
          }
          else
          {
            curState = m_forwardLink[curState][linkToFollow];
          }
        } while (curState >= m_sequence.size());

        v.push_back(m_sequence[curState]);
      }
    }
    return v;
  }

private:
  int n{};
  safe_vector<int, 0> m_sp;
  safe_vector<int, 0> m_lrs;
  safe_vector_simple<safe_vector<int, -1>> m_trans;
  debug_vector_t<ossia::value> m_sequence;
  debug_vector_t<debug_vector_t<int>> m_forwardLink;

  mutable rnd::pcg m_rand_engine;
};

struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Factor Oracle";
    static const constexpr auto objectKey = "Factor Oracle";
    static const constexpr auto category = "Impro";
    static const constexpr auto author
        = "Shlomo Dubnov, Ge Wang, Éric Meaux, Jean-Michaël Celerier";
    static const constexpr auto kind = Process::ProcessCategory::Mapping;
    static const constexpr auto description = "Factor Oracle algorithm ."; // TODO cite
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const uuid_constexpr auto uuid = make_uuid("d90284c0-4196-47e0-802d-7e07342029ec");

    static const constexpr auto controls
        = std::make_tuple(Control::IntSlider{"Sequence length", 1, 64, 8});

    static const constexpr value_in value_ins[]{"in", "regen", "bang"};
    static const constexpr value_out value_outs[]{"out"};
  };

  struct State
  {
    FactorOracle oracle{64};
    debug_vector_t<ossia::value> sequence;
    std::size_t sequence_idx{};
  };

  using control_policy = ossia::safe_nodes::last_tick;
  static void
  run(const ossia::value_port& in,
      const ossia::value_port& regen,
      const ossia::value_port& bangs,
      int seq_len,
      ossia::value_port& out,
      ossia::token_request,
      ossia::exec_state_facade,
      State& self)
  {
    // Entrées sont dans p1
    for (auto val : in.get_data())
    {
      self.oracle.add_char(val.value);
    }

    if (!regen.get_data().empty())
    {
      self.sequence = self.oracle.make_rand_sequence(0.4, seq_len);
    }

    if (!self.sequence.empty())
    {
      for (auto& bang : bangs.get_data())
      {
        self.sequence_idx = ossia::clamp<int64_t>(
            (int64_t)self.sequence_idx, 0, (int64_t)self.sequence.size() - 1);
        out.write_value(self.sequence[self.sequence_idx], bang.timestamp);
        self.sequence_idx = (self.sequence_idx + 1) % self.sequence.size();
      }
    }
  }
};
#undef debug_vector_t
}
