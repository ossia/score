#pragma once
#include <Engine/Node/PdNode.hpp>

#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/math.hpp>

#include <random>
#if !defined(NDEBUG) && !defined(_MSC_VER) && !defined(__clang__)
#include <debug/vector>
#define debug_vector_t __gnu_debug::vector
#else
#define debug_vector_t std::vector
#endif

namespace FactorOracle2MIDI
{
/** The class SingleTransition denotes the elements that belong to each transition
  */
class Notes
{
public:
  int note;
  int pitch;
  double startTime;
  double endTime;
};
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
  int phi, k, fo_iter, current_state = 1;
  std::vector<T> input_values;
  std::vector<State<T>> states_; /**< std::vector of all the states */
  std::vector<std::vector<int>>
      RevSuffix; /**< std::vector where each position has all the suffix transitions directed to each state */
  FactorOracle()
  {
    // int len = word.size();
    this->states_.resize(2);
    //SingleTransition statezero; /*!< Create state 0 */
    this->states_[0].state_ = 0;
    this->states_[0].lrs_ = 0;
    this->states_[0].suffix_transition_ = -1; /*!< S[0] = -1 */
    this->RevSuffix.resize(2);
  }
  void AddLetter(int i, std::vector<T> word)
  {
    //! A normal member taking four arguments and returning no value.
    /*!
              \param i an integer argument.
              \param word a string argument.
            */
    this->states_.resize(i + 1);
    this->RevSuffix.resize(i + 1);
    T alpha = word[i - 1];
    this->AddState(i - 1);
    int statemplusone = i;
    this->AddTransition(i - 1, i, alpha);
    k = this->states_[i - 1].suffix_transition_; /*!< k = S[i-1] */
    //std::cout << "statemplusone first: "<< statemplusone << endl;
    phi = i - 1; /*!< phi_one = i-1 */
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
        this->AddTransition(k, statemplusone, alpha);
        phi = k;
        k = this->states_[k].suffix_transition_;
        iter = 0;
      }
    }
    if (k == -1)
    {
      this->states_[statemplusone].suffix_transition_ = 0;
      this->states_[statemplusone].lrs_ = 0;
    }
    else
    {
      flag = 0, iter = 0;
      if (this->states_[k].transition_[iter].symbol_ == alpha)
      {
        flag = 1;
        this->states_[statemplusone].suffix_transition_
            = this->states_[k].transition_[iter].last_state_;
        this->states_[statemplusone].lrs_
            = this->LengthCommonSuffix(
                  phi, this->states_[statemplusone].suffix_transition_ - 1)
              + 1;
      }
      while (iter < this->states_[k].transition_.size() && flag == 0)
      {
        if (this->states_[k].transition_[iter].symbol_ == alpha)
        {

          this->states_[statemplusone].suffix_transition_
              = this->states_[k].transition_[iter].last_state_;
          this->states_[statemplusone].lrs_
              = this->LengthCommonSuffix(
                    phi, this->states_[statemplusone].suffix_transition_ - 1)
                + 1;
          flag = 1;
        }

        iter++;
      }
    }
    T temp_word = word[statemplusone - this->states_[statemplusone].lrs_ - 1];
    k = this->FindBetter(statemplusone, temp_word, word);
    if (k != 0)
    {
      this->states_[statemplusone].lrs_
          = this->states_[statemplusone].lrs_ + 1;
      this->states_[statemplusone].suffix_transition_ = k;
    }
    RevSuffix[this->states_[statemplusone].suffix_transition_].push_back(
        statemplusone);
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
  int FindBetter(int i, T alpha, std::vector<T> word)
  {
    //! A normal member taking five arguments and returning an integer value.
    /*!
              \param RevSuffix a reference to a std::vector of std::vector of integers.
              \param i an integer argument.
              \param alpha a char argument.
              \param word a string argument.
              \return A better state
            */

    int len_t = this->RevSuffix[this->states_[i].suffix_transition_].size();
    int statei = this->states_[i].suffix_transition_;
    if (len_t == 0)
      return 0;
    sort(this->RevSuffix[statei].begin(), this->RevSuffix[statei].end());
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
        int output_midi = RevSuffix[this->states_[i].suffix_transition_][j];
        return output_midi;
      }
    }
    return 0;
  };
  std::vector<T> FOGenerate(int& i, std::vector<T> v, float q)
  {
    //! A normal member taking four arguments and returning a string value.
    /*!
              \param i an integer argument.
              \param v a string argument.
              \param q a float argument.
              \return The factor oracle improvisation
            */
    std::random_device
        rd; ///Will be used to obtain a seed for the random number engine
    std::mt19937 gen(
        rd()); ///Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<> dis(0.0, 1.0);
    float u = dis(gen);
    /// float u = (float)rand() / RAND_MAX;
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
        v.push_back(w);
      }
      else
      {
        int lenSuffix = this->states_[this->states_[i].suffix_transition_]
                            .transition_.size()
                        - 1;
        std::random_device
            rd; ///Will be used to obtain a seed for the random number engine
        std::mt19937 gen(
            rd()); ///Standard mersenne_twister_engine seeded with rd()
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
        v.push_back(alpha);
      }
    }
    return v;
  };
  void FactorOracleStart(std::vector<T> word)
  {
    //! A normal member taking one argument and returning no value.
    /*!
              \param word a string argument.
            */
    int len = word.size();
    this->states_.resize(2);
    this->states_[0].state_ = 0;
    this->states_[0].lrs_ = 0;
    this->states_[0].suffix_transition_ = -1; /*!< S[0] = -1 */
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
    transition_i.symbol_ = symbol;
    this->states_[first_state].transition_.push_back(transition_i);
  };
  std::vector<T> CallGenerate(int len, float q)
  {

    std::vector<T> oracle = {};
    std::cout << "len: " << len << std::endl;
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
namespace Nodes::FactorOracle2MIDI
{
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "New Factor Oracle MIDI";
    static const constexpr auto objectKey = "New Factor Oracle MIDI";
    static const constexpr auto category = "Impro";
    static const constexpr auto author = "Maria Paula Carrero Rivas";
    static const constexpr auto kind = Process::ProcessCategory::Mapping;
    static const constexpr auto description
        = "Factor Oracle algorithm ."; // TODO cite
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto uuid
        = make_uuid("C87B5326-56C2-4489-8E08-AA9E1EF27359");

    static const constexpr auto controls
        = std::make_tuple(Control::IntSlider{"Sequence length", 1, 64, 8});
    static const constexpr midi_in midi_ins[]{"input_midi"};
    static const constexpr midi_out midi_outs[]{"output_midi"};
    static const constexpr value_in value_ins[]{"regen", "bang"};
  };

  struct State
  {
    ::FactorOracle2MIDI::FactorOracle<int> oracle;
    int i = 0;
    debug_vector_t<libremidi::midi_bytes> sequence;
    debug_vector_t<libremidi::midi_bytes> midi_bytes;
    std::size_t sequence_idx{};
  };
  ossia::value* buffer;
  using control_policy = ossia::safe_nodes::last_tick;
  static void
  run(const ossia::midi_port& input_midi,
      const ossia::value_port& regen,
      const ossia::value_port& bangs,
      int seq_len,
      ossia::midi_port& output_midi,
      ossia::token_request,
      ossia::exec_state_facade,
      State& self)
  {

    // Entrées sont dans p1
    for (auto val : input_midi.messages)
    {
      ::FactorOracle2MIDI::Notes temp;
      temp.pitch = val.bytes[1];
      self.oracle.input_values.push_back(val.bytes[1]);
      self.oracle.AddLetter(
          self.oracle.current_state, self.oracle.input_values);
      self.oracle.current_state = self.oracle.current_state + 1;

      std::cout << "current state: " << self.oracle.current_state << std::endl;
      for (int i = 0; i < self.oracle.input_values.size() + 1; i++)
      {

        std::cout << "STATE[" << i << "]:\n"
                  << "LRS: " << self.oracle.states_[i].lrs_ << "\n";
        std::cout << "Suffix: " << self.oracle.states_[i].suffix_transition_
                  << "\n";
        std::cout << "Transitions: "
                  << "\n";
        std::cout << "\n";
      }
    }

    if (!regen.get_data().empty())
    {
      std::cout << self.oracle.input_values.empty() << std::endl;
      if (!self.oracle.input_values.empty())
      {
        std::cout << "first not empty" << std::endl;
        std::vector<int> temp_vec = self.oracle.CallGenerate(seq_len, 0.6);
        //self.sequence = self.oracle.CallGenerate(seq_len, 0.6);
        self.midi_bytes.push_back({144, 0, 40});
        self.midi_bytes[seq_len][1] = temp_vec[seq_len];
        self.sequence = self.midi_bytes;
      }
    }

    if (!self.sequence.empty() && !self.oracle.input_values.empty())
    {
      for (auto& bang : bangs.get_data())
      {
        std::cout << "second not empty" << std::endl;
        self.sequence_idx = ossia::clamp<int64_t>(
            (int64_t)self.sequence_idx, 0, (int64_t)self.sequence.size() - 1);
        libremidi::message tmp;
        tmp.bytes = self.sequence[self.sequence_idx];
        tmp.timestamp = bang.timestamp;
        output_midi.messages.push_back(tmp);
        self.sequence_idx = (self.sequence_idx + 1) % self.sequence.size();
      }
    }
  }
};
#undef debug_vector_t
}
