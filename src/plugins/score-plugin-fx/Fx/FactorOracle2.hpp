#pragma once
#include <Engine/Node/SimpleApi.hpp>

#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/math.hpp>

#include <random>
#if !defined(NDEBUG) && !defined(_MSC_VER) && !defined(__clang__)
#include <debug/vector>
#define debug_vector_t __gnu_debug::vector
#else
#define debug_vector_t std::vector
#endif

/*! \file FactorOracle2.h
    \brief A file that contains the definitions of the classes needed for the creation of a Factor Oracle.

    Three main classes: FactorOracle, State and SingleTransition.
*/
/*! \fn void AddLetter(int i, const std::vector<T> word)
    \brief Adds new transitions from state i-1 to state i.
    \param T A vector where each position has all the suffix transitions directed to each state.
    \param i The integer of the current state.
    \param word The input template vector.
*/
/*! \fn int LengthCommonSuffix(int phi_one, int phi_two)
 * https://www.researchgate.net/publication/220520646_An_Improved_Algorithm_for_Finding_Longest_Repeats_with_a_Modified_Factor_Oracle
    \brief Finds the length of a common suffix ending at the position phi_one and phi_two by traversing the suffix links.
    \param phi_one The position of the state.
    \param phi_two The position of the state.
    \return The length of the common suffix between two states
*/
/*! \fn int FindBetter(int i, T alpha, const std::vector<T> word)
    \brief Finds the best suffix state where the longest repeated subsequence is found.
    \param i The integer of the current state.
    \param alpha The transition symbol.
    \param word The input template vector.
    \return A better state
*/
/*! \fn string FOGenerate(int& i, std::vector<T> v, float q)
    \brief Generates the Factor Oracle improvisation.
    \param i The integer of the current state.
    \param v The sequence v.
    \param q A float argument.
    \return The factor oracle improvisation
*/
/*! \fn void FactorOracleStart(const std::vector<T> word)
    \brief Starts the process of the Factor Oracle generation .
    \param word The input template vector.
*/

namespace Factor
{
/** The class SingleTransition denotes the elements that belong to each transition
  */
template <class T>
class SingleTransition
{
public:
  int first_state_; /**< denotes the first state of the transition */
  int last_state_;  /**< denotes the last state of the transition */
  T symbol_;        /**< denotes the symbol (letter) of the transition */
};

/** The class State denotes the elements that belong to each state
 * state denotes de number of the state
 * std::vector <SingleTransition> transition is the std::vector where all forward links of the state are defined
 * suffix_transition denotes which is the suffix link of this state
 * lrs is the longest repeated subsequence of this state
 * */
template <class T>
class State
{
public:
  int state_; /*!< denotes the number of the state */
  std::vector<SingleTransition<T>>
      transition_; /*!< denotes the number of the state */
  int suffix_transition_;
  int lrs_ = 0;
  void singleTransitionResize() { transition_.resize(10); }
};

template <class T>
class FactorOracle
{
public:
  int phi = 0, k = 0, fo_iter = 0, current_state = 1;
  std::vector<T> input_values;
  std::vector<State<T>> states_; /**< std::vector of all the states */
  std::vector<std::vector<int>>
      RevSuffix; /**< std::vector where each position has all the suffix transitions directed to each state */
  FactorOracle()
  {
    this->states_.resize(2);
    this->states_[0].state_ = 0;
    this->states_[0].lrs_ = 0;
    this->states_[0].suffix_transition_ = -1; /*!< S[0] = -1 */
    this->RevSuffix.resize(2);
  }
  void AddLetter(int i, const std::vector<T> word)
  {
    //! A normal member taking two arguments and returning no value.
    /*!
      \param i an integer argument.
      \param word a template vector argument.
    */
    if (i != 0)
    {
      this->states_.resize(i + 1);
      this->RevSuffix.resize(i + 1);
      T alpha = word[i - 1];
      this->AddState(i - 1);
      int state_m_plus_one = i;
      this->AddTransition(i - 1, i, alpha);
      k = this->states_[i - 1].suffix_transition_; /*!< k = S[i-1] */
      phi = i - 1;                                 /*!< phi_one = i-1 */
      int flag = 0, iter = 0;
      /**
       * while k > -1 and delta(k,p[i]) is undefined
       *      do delta(k, p[i]) <- i
       *      phi_one = k
       *      k = S[k]
       * */
      while (k > -1 && flag == 0)
      {
        while (iter < this->states_[k].transition_.size())
        {
          if (this->states_[k].transition_[iter].symbol_ == alpha)
          {
            flag = 1;
          }
          iter++;
        }
        if (flag == 0)
        {
          this->AddTransition(k, state_m_plus_one, alpha);
          phi = k;
          k = this->states_[k].suffix_transition_;
          iter = 0;
        }
      }
      if (k == -1)
      {
        this->states_[state_m_plus_one].suffix_transition_ = 0;
        this->states_[state_m_plus_one].lrs_ = 0;
      }
      else
      {
        flag = 0, iter = 0;
        if (this->states_[k].transition_[iter].symbol_ == alpha)
        {
          flag = 1;
          this->states_[state_m_plus_one].suffix_transition_
              = this->states_[k].transition_[iter].last_state_;
          this->states_[state_m_plus_one].lrs_
              = this->LengthCommonSuffix(
                    phi,
                    this->states_[state_m_plus_one].suffix_transition_ - 1)
                + 1;
        }
        while (iter < this->states_[k].transition_.size() && flag == 0)
        {
          if (this->states_[k].transition_[iter].symbol_ == alpha)
          {

            this->states_[state_m_plus_one].suffix_transition_
                = this->states_[k].transition_[iter].last_state_;
            this->states_[state_m_plus_one].lrs_
                = this->LengthCommonSuffix(
                      phi,
                      this->states_[state_m_plus_one].suffix_transition_ - 1)
                  + 1;
            flag = 1;
          }

          iter++;
        }
      }
      T temp_word
          = word[state_m_plus_one - this->states_[state_m_plus_one].lrs_ - 1];
      k = this->FindBetter(state_m_plus_one, temp_word, word);
      if (k != 0)
      {
        this->states_[state_m_plus_one].lrs_
            = this->states_[state_m_plus_one].lrs_ + 1;
        this->states_[state_m_plus_one].suffix_transition_ = k;
      }
      RevSuffix[this->states_[state_m_plus_one].suffix_transition_].push_back(
          std::move(state_m_plus_one));
    }
  };
  int LengthCommonSuffix(int phi_one, int phi_two)
  {
    if (phi_two == this->states_[phi_one].suffix_transition_)
      return this->states_[phi_one].lrs_;
    else
    {
      while (this->states_[phi_one].suffix_transition_
             != this->states_[phi_two].suffix_transition_)
        phi_two = this->states_[phi_two].suffix_transition_;
    }
    if (this->states_[phi_one].lrs_ <= this->states_[phi_two].lrs_)
      return this->states_[phi_one].lrs_;
    else
      return this->states_[phi_two].lrs_;
  };
  int FindBetter(int i, T alpha, const std::vector<T> word)
  {
    //! A normal member taking three arguments and returning an integer value.
    /*!
      \param i an integer argument.
      \param alpha a template symbol argument.
      \param word a template vector argument.
      \return A better state
    */

    int len_t = this->RevSuffix[this->states_[i].suffix_transition_].size();
    int state_i = this->states_[i].suffix_transition_;
    if (len_t == 0)
      return 0;
    sort(this->RevSuffix[state_i].begin(), this->RevSuffix[state_i].end());
    for (int j = 0; j < len_t; j++)
    {
      if (this->states_[this->RevSuffix[this->states_[i].suffix_transition_]
                                       [j]]
                  .lrs_
              == this->states_[i].lrs_
          && word
                     [this->RevSuffix[this->states_[i].suffix_transition_][j]
                      - this->states_[i].lrs_ - 1]
                 == alpha)
      {
        int out = RevSuffix[this->states_[i].suffix_transition_][j];
        return out;
      }
    }
    return 0;
  };
  std::vector<T> FOGenerate(int& i, std::vector<T> v, float q)
  {
    //! A normal member taking three arguments and returning a string value.
    /*!
      \param i an integer argument.
      \param v a template vector argument.
      \param q a float argument.
      \return The factor oracle improvisation
    */
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    float u = dis(gen);

    if (this->states_.size() == 2 || this->states_.size() == 1)
    {
      v.push_back(this->states_[0].transition_[0].symbol_);
    }
    else
    {
      if (u < q)
      {
        i = i + 1;
        int len = this->states_.size();
        if (i >= len)
          i = len - 1;
        T w = this->states_[i].transition_[0].symbol_;
        v.push_back(std::move(w));
      }
      else
      {
        int lenSuffix = this->states_[this->states_[i].suffix_transition_]
                            .transition_.size()
                        - 1;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis_int(0, lenSuffix);
        int rand_alpha = dis_int(gen);
        T alpha = this->states_[this->states_[i].suffix_transition_]
                      .transition_[rand_alpha]
                      .symbol_;
        i = this->states_[this->states_[i].suffix_transition_]
                .transition_[rand_alpha]
                .last_state_;
        if (i == -1)
        {
          i = 0;
        }
        v.push_back(std::move(alpha));
      }
    }
    return v;
  };
  void FactorOracleStart(const std::vector<T> word)
  {
    //! A normal member taking one argument and returning no value.
    /*!
      \param word a template argument.
    */
    int len = word.size();
    this->states_.resize(2);
    this->states_[0].state_ = 0;
    this->states_[0].lrs_ = 0;
    this->states_[0].suffix_transition_ = -1;
    this->RevSuffix.resize(2);
  };
  void AddState(int first_state)
  {
    this->states_[first_state].state_ = first_state;
  };
  void AddTransition(int first_state, int last_state, T symbol)
  {
    SingleTransition<T> transition_i;
    transition_i.first_state_ = first_state;
    transition_i.last_state_ = last_state;
    transition_i.symbol_ = std::move(symbol);
    this->states_[first_state].transition_.push_back(std::move(transition_i));
  };
  std::vector<T> CallGenerate(int len, float q)
  {

    std::vector<T> oracle = {};
    fo_iter = 1;
    for (int x = 0; x < len; x++)
    {
      oracle = this->FOGenerate(fo_iter, oracle, q);
      if (fo_iter == len)
        fo_iter = len - 1;
    }
    return oracle;
  };
};
}
namespace Nodes::FactorOracle2
{
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "New Factor Oracle";
    static const constexpr auto objectKey = "New Factor Oracle";
    static const constexpr auto category = "Impro";
    static const constexpr auto author = "Maria Paula Carrero Rivas";
    static const constexpr auto kind = Process::ProcessCategory::Mapping;
    static const constexpr auto description
        = "Factor Oracle algorithm ."; // TODO cite
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto uuid
        = make_uuid("66F1C352-C48F-40A2-9283-35C2CB376258");

    static const constexpr auto controls
        = std::make_tuple(Control::IntSlider{"Sequence length", 1, 64, 8});
    static const constexpr value_in value_ins[]{"in", "regen", "bang"};
    static const constexpr value_out value_outs[]{"out"};
  };

  struct State
  {
    int i = 0;
    std::size_t sequence_idx{};
    debug_vector_t<ossia::value> sequence;
    Factor::FactorOracle<ossia::value> oracle;
  };
  ossia::value* buffer = nullptr;
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
      self.oracle.input_values.push_back(val.value);
      self.oracle.AddLetter(
          self.oracle.current_state, self.oracle.input_values);
      self.oracle.current_state = self.oracle.current_state + 1;
    }

    if (!regen.get_data().empty())
    {
      self.sequence = self.oracle.CallGenerate(seq_len, 0.6);
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
