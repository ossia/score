#pragma once
#include <ossia/detail/math.hpp>

#include <Engine/Node/PdNode.hpp>

#include <random>
namespace Nodes::FactorOracle
{

class FactorOracle
{
public:
  FactorOracle(int sz = 0)
      : m_oracleSize{sz}
      , m_forwardLink(m_oracleSize + 1, std::vector<int>{-1})
      , m_rand_engine{std::random_device{}()}
  {
    m_sp[0] = -1;
    m_lrs[0] = 0;

    memset(m_trans, -1, sizeof(m_trans));
  }

  explicit FactorOracle(std::string seq) : FactorOracle{(int)seq.size()}
  {
    m_sequence = std::move(seq);

    for (int i = 0; i < m_oracleSize; i++)
    {
      int c = m_sequence[i] - 'a';
      if (c < sym_count && c >= 0)
        add_state(c);
      else
        add_sep();
    }
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

  void add_char(char c)
  {
    m_sequence += c;
    add_state(c);
  }

  void add_state(int c)
  {
    m_trans[n][c] = n + 1;
    int p1 = n;
    int j = m_sp[p1];
    ++n;
    while (j != -1 && m_trans[j][c] == -1)
    {
      m_trans[j][c] = n;
      if (m_forwardLink[j][0] == -1)
      {
        m_forwardLink[j][0] = m_trans[j][c];
      }
      else
      {
        m_forwardLink[j].push_back(m_trans[j][c]);
      }
      p1 = j;
      j = m_sp[p1];
    }
    m_sp[n] = (j == -1 ? 0 : m_trans[j][c]);
    m_lrs[n] = (m_sp[n] == 0 ? 0 : LCS(p1, m_sp[n] - 1) + 1);
  }

  void add_sep()
  {
    n++;
    m_sp[n] = 0;
    m_lrs[n] = 0;
  }

  std::string make_rand_sequence(float continuity, int seqSize)
  {
    auto start = std::uniform_int_distribution<std::size_t>{
        0, m_sequence.size()}(m_rand_engine);
    return make_sequence(continuity, start, seqSize);
  }

  std::string
  make_sequence(float continuity, unsigned int curSate, int seqSize)
  {
    if (curSate > m_sequence.size())
    {
      qDebug() << "Le point initial de l'improvisation doit être comprise "
                  "dans la séquence";
      return "-1";
    }

    std::string v;
    v.reserve(seqSize);
    for (int i = 0; i < seqSize; i++)
    {
      auto f = std::uniform_real_distribution<float>{}(m_rand_engine);
      if (f <= continuity && curSate < m_sequence.size() - 1)
      {
        curSate++;
        v += m_sequence[curSate];
      }
      else
      {
        int links = (curSate == 0 ? 0 : 1);
        if (m_forwardLink[curSate][0] != -1)
        {
          links += m_forwardLink[curSate].size();
        }

        auto linkToFollow
            = std::uniform_int_distribution<int>{0, links - 1}(m_rand_engine);
        if (linkToFollow == links - 1)
        {
          if (curSate != 0)
          {
            curSate = m_sp[curSate];
          }
        }
        else
        {
          curSate = m_forwardLink[curSate][linkToFollow];
        }

        v += m_sequence[curSate];
      }
    }
    return v;
  }

private:
  static const constexpr auto max_size = 200000;
  static const constexpr auto sym_count = 26;

  int n{};
  int m_oracleSize{};
  int m_sp[max_size];
  int m_lrs[max_size];
  int m_trans[max_size][sym_count];
  std::string m_sequence;
  std::vector<std::vector<int>> m_forwardLink;

  std::minstd_rand m_rand_engine;
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
    static const constexpr auto description
        = "Factor Oracle algorithm ."; // TODO cite
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto uuid
        = make_uuid("d90284c0-4196-47e0-802d-7e07342029ec");

    static const constexpr auto controls
        = std::make_tuple(Control::IntSlider{"Sequence length", 1, 64, 8});
    static const constexpr value_in value_ins[]{"in", "regen", "bang"};
    static const constexpr value_out value_outs[]{"out"};
  };

  struct State
  {
    FactorOracle oracle{64};
    std::string sequence;
    std::size_t sequence_idx{};
  };

  using control_policy = ossia::safe_nodes::last_tick;
  static void
  run(const ossia::value_port& in, const ossia::value_port& regen,
      const ossia::value_port& bangs, int seq_len, ossia::value_port& out,
      ossia::token_request, ossia::exec_state_facade, State& self)
  {
    // Entrées sont dans p1
    for (auto val : in.get_data())
    {
      char c = ossia::convert<char>(val.value);
      c = ossia::clamp(c, 'a', 'z');
      self.oracle.add_char(c - 'a');
    }

    if (!regen.get_data().empty())
    {
      self.sequence = self.oracle.make_rand_sequence(0.4, seq_len);
    }

    if (!self.sequence.empty())
    {
      for (auto& bang : bangs.get_data())
      {
        self.sequence_idx = ossia::clamp<std::size_t>(
            self.sequence_idx, 0, self.sequence.length() - 1);
        out.write_value(
            'a' + self.sequence[self.sequence_idx], bang.timestamp);
        self.sequence_idx = (self.sequence_idx + 1) % self.sequence.length();
      }
    }
  }
};
}
