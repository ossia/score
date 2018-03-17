#pragma once
#include <Effect/EffectFactory.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <Engine/LocalTree/Scenario/ProcessComponent.hpp>
#include <Media/Effect/DefaultEffectItem.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <Media/Effect/Faust/FaustUtils.hpp>
#include <ossia/dataflow/execution_state.hpp>

#include <faust/gui/GUI.h>
namespace FaustDSP
{
template<typename DSP>
class Fx;
}

template <typename DSP>
struct Metadata<Category_k, FaustDSP::Fx<DSP>>
{
    static Q_DECL_RELAXED_CONSTEXPR const char* get()
    {
      return "Audio";
    }
};
template <typename DSP>
struct Metadata<Tags_k, FaustDSP::Fx<DSP>>
{
    static QStringList get()
    {
      QStringList lst{"Audio"};
      return lst;
    }
};
template <typename DSP>
struct Metadata<Process::ProcessFlags_k, FaustDSP::Fx<DSP>>
{
    static Process::ProcessFlags get()
    {
      return Process::ProcessFlags::SupportsAll;
    }
};

namespace FaustDSP
{
template<typename T>
struct Wrap : UI
{
    template<typename... Args>
    Wrap(Args&&... args): t{args...} { }

    T t;
    void openTabBox(const char* label) override
    { t.openTabBox(label); }
    void openHorizontalBox(const char* label) override
    { t.openHorizontalBox(label); }
    void openVerticalBox(const char* label) override
    { t.openVerticalBox(label); }
    void closeBox() override
    { t.closeBox(); }
    void declare(FAUSTFLOAT* zone, const char* key, const char* val) override
    { t.declare(zone, key, val); }
    void addSoundfile(const char* label, const char* filename, Soundfile** sf_zone) override
    { t.addSoundfile(label, filename, sf_zone); }
    void addButton(const char* label, FAUSTFLOAT* zone) override
    { t.addButton(label, zone); }
    void addCheckButton(const char* label, FAUSTFLOAT* zone) override
    { t.addCheckButton(label, zone); }
    void addVerticalSlider(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) override
    { t.addVerticalSlider(label, zone, init, min, max, step); }
    void addHorizontalSlider(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) override
    { t.addHorizontalSlider(label, zone, init, min, max, step); }
    void addNumEntry(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) override
    { t.addNumEntry(label, zone, init, min, max, step); }
    void addHorizontalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max) override
    { t.addHorizontalBargraph(label, zone, min, max); }
    void addVerticalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max) override
    { t.addVerticalBargraph(label, zone, min, max);  }
};

template<typename DSP>
class Fx final: public Process::ProcessModel
{
      SCORE_SERIALIZE_FRIENDS
      PROCESS_METADATA_IMPL(Fx)

  public:
    Fx(TimeVal t,
       const Id<Process::ProcessModel>& id,
       QObject* parent):
      Process::ProcessModel{t, id, Metadata<ObjectKey_k, Fx>::get(), parent}
    {

    }

    ~Fx() override
    {
    }

    template<typename Impl>
    Fx(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
    {
      vis.writeTo(*this);
    }

    QString prettyName() const override
    {
      return Metadata<PrettyName_k, Fx>::get();
    }

    Process::Inlets& inlets() { return m_inlets; }
    Process::Outlets& outlets() { return m_outlets; }

};

template<typename DSP>
class Executor final
    : public Engine::Execution::ProcessComponent_T<Fx<DSP>, ossia::node_process>
{

  public:
    static constexpr bool is_unique = true;

    class exec_node final : public ossia::graph_node
    {
      public:
        DSP dsp;
        ossia::small_vector<std::pair<ossia::value_port*, FAUSTFLOAT*>, 8> controls;
        exec_node()
        {
          m_inlets.push_back(ossia::make_inlet<ossia::audio_port>());
          m_outlets.push_back(ossia::make_outlet<ossia::audio_port>());
          Wrap<Media::Faust::ExecUI<exec_node>> ex{*this};
          dsp.buildUserInterface(&ex);
        }

        void run(ossia::token_request tk, ossia::execution_state&) override
        {
          if(tk.date > this->m_prev_date)
          {
            std::size_t d = tk.date - this->m_prev_date;
            for(auto ctrl : controls)
            {
              auto& dat = ctrl.first->get_data();
              if(!dat.empty())
              {
                *ctrl.second = ossia::convert<float>(dat.back().value);
              }
            }

            auto& audio_in = *m_inlets[0]->data.template target<ossia::audio_port>();
            auto& audio_out = *m_outlets[0]->data.template target<ossia::audio_port>();

            const std::size_t n_in = dsp.getNumInputs();
            const std::size_t n_out = dsp.getNumOutputs();

            float* inputs_ = (float*)alloca(n_in * d * sizeof(float));
            float* outputs_ = (float*)alloca(n_out * d * sizeof(float));

            float** input_n = (float**)alloca(sizeof(float*) * n_in);
            float** output_n = (float**)alloca(sizeof(float*) * n_out);

            // Copy inputs
            // TODO offset !!!
            for(std::size_t i = 0; i < n_in; i++)
            {
              input_n[i] = inputs_ + i * d;
              if(audio_in.samples.size() > i)
              {
                auto num_samples = std::min((std::size_t)d, (std::size_t)audio_in.samples[i].size());
                for(std::size_t j = 0; j < num_samples; j++)
                {
                  input_n[i][j] = (float)audio_in.samples[i][j];
                }

                if(d > audio_in.samples[i].size())
                {
                  for(std::size_t j = audio_in.samples[i].size(); j < d; j++)
                  {
                    input_n[i][j] = 0.f;
                  }
                }
              }
              else
              {
                for(std::size_t j = 0; j < d; j++)
                {
                  input_n[i][j] = 0.f;
                }
              }
            }

            for(std::size_t i = 0; i < n_out; i++)
            {
              output_n[i] = outputs_ + i * d;
              for(std::size_t j = 0; j < d; j++)
              {
                output_n[i][j] = 0.f;
              }
            }
            dsp.compute(d, input_n, output_n);

            audio_out.samples.resize(n_out);
            for(std::size_t i = 0; i < n_out; i++)
            {
              audio_out.samples[i].resize(d);
              for(std::size_t j = 0; j < d; j++)
              {
                audio_out.samples[i][j] = (double)output_n[i][j];
              }
            }

            // TODO handle multichannel cleanly
            if(n_out == 1)
            {
              audio_out.samples.resize(2);
              audio_out.samples[1] = audio_out.samples[0];
            }
          }
        }

        std::string label() const override
        {
          return "Faust";
        }

        void all_notes_off() override
        {
        }
    };

    static Q_DECL_RELAXED_CONSTEXPR score::Component::Key static_key()
    {
      return Metadata<ConcreteKey_k, Fx<DSP>>::get().impl();
    }

    score::Component::Key key() const final override
    {
      return static_key();
    }

    bool key_match(score::Component::Key other) const final override
    {
      return static_key() == other
          || Engine::Execution::ProcessComponent::base_key_match(other);
    }

    Executor(
        Fx<DSP>& proc,
        const Engine::Execution::Context& ctx,
        const Id<score::Component>& id,
        QObject* parent)
      : Engine::Execution::ProcessComponent_T<Fx<DSP>, ossia::node_process>{proc, ctx, id, "FaustComponent", parent}
    {
      auto node = std::make_shared<exec_node>();
      this->node = node;
      this->m_ossia_process = std::make_shared<ossia::node_process>(node);
      node->dsp.instanceInit(ctx.plugin.execState->sampleRate);
      //      for(std::size_t i = 1; i < proc.inlets().size(); i++)
      //      {
      //        auto inlet = static_cast<Process::ControlInlet*>(proc.inlets()[i]);
      //        *node->controls[i-1].second = ossia::convert<double>(inlet->value());
      //        auto inl = this->node->inputs()[i];
      //        connect(inlet, &Process::ControlInlet::valueChanged,
      //                this, [this, inl] (const ossia::value& v) {
      //          this->system().executionQueue.enqueue([inl,val=v] () mutable {
      //            inl->data.template target<ossia::value_port>()->add_value(std::move(val), 0);
      //          });
      //        });
      //      }

    }
};
}

namespace FaustDSP
{
template<typename DSP>
using ProcessFactory = Process::ProcessFactory_T<Fx<DSP>>;

template<typename DSP>
using LayerFactory = Process::EffectLayerFactory_T<Fx<DSP>, Media::Effect::DefaultEffectItem>;

template<typename DSP>
using ExecutorFactory = Engine::Execution::ProcessComponentFactory_T<Executor<DSP>>;
}
