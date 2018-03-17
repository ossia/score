#pragma once
#include <Process/Dataflow/Port.hpp>
#include <ossia/network/domain/domain.hpp>
#include <faust/dsp/llvm-c-dsp.h>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>

namespace Media
{
namespace Faust
{
template<typename T>
struct setup_ui : UIGlue
{
    setup_ui(T& self)
    {
      uiInterface = &self;

      openTabBox = [] (void* self, const char* arg1)
      { return reinterpret_cast<T*>(self)->openTabBox(arg1); };
      openHorizontalBox = [] (void* self, const char* arg1)
      { return reinterpret_cast<T*>(self)->openHorizontalBox(arg1); };
      openVerticalBox = [] (void* self, const char* arg1)
      { return reinterpret_cast<T*>(self)->openVerticalBox(arg1); };
      closeBox = [] (void* self)
      { return reinterpret_cast<T*>(self)->closeBox(); };
      addButton = [] (void* self, const char* label, FAUSTFLOAT* zone)
      { return reinterpret_cast<T*>(self)->addButton(label, zone); };
      addCheckButton = [] (void* self, const char* label, FAUSTFLOAT* zone)
      { return reinterpret_cast<T*>(self)->addCheckButton(label, zone); };
      addVerticalSlider = [] (void* self,const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step)
      { return reinterpret_cast<T*>(self)->addVerticalSlider(label, zone, init, min, max, step); };
      addHorizontalSlider = [] (void* self,const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step)
      { return reinterpret_cast<T*>(self)->addHorizontalSlider(label, zone, init, min, max, step); };
      addNumEntry = [] (void* self,const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step)
      { return reinterpret_cast<T*>(self)->addNumEntry(label, zone, init, min, max, step); };
      addHorizontalBargraph = [] (void* self,const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max)
      { return reinterpret_cast<T*>(self)->addHorizontalBargraph(label, zone, min, max); };
      addVerticalBargraph = [] (void* self,const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max)
      { return reinterpret_cast<T*>(self)->addVerticalBargraph(label, zone, min, max); };
      addSoundFile = [] (void* self,const char* label, const char* filename, Soundfile** sf_zone)
      { return reinterpret_cast<T*>(self)->addSoundfile(label, filename, sf_zone); };
      declare = [] (void* self,FAUSTFLOAT* zone, const char* key, const char* val)
      { return reinterpret_cast<T*>(self)->declare(zone, key, val); };
    }
};

template<typename Proc>
struct UI
{
    Proc& fx;
    setup_ui<UI> glue{*this};

    UI(Proc& sfx): fx{sfx} { }

    void openTabBox(const char* label) { }
    void openHorizontalBox(const char* label) { }
    void openVerticalBox(const char* label) { }
    void closeBox() { }
    void declare(FAUSTFLOAT* zone, const char* key, const char* val) { }
    void addSoundfile(const char* label, const char* filename, Soundfile** sf_zone) { }

    void addButton(const char* label, FAUSTFLOAT* zone)
    {
      auto inl = new Process::ControlInlet{getStrongId(fx.inlets()), &fx};
      inl->setCustomData(label);
      fx.inlets().push_back(inl);
    }

    void addCheckButton(const char* label, FAUSTFLOAT* zone)
    { addButton(label, zone); }

    void addVerticalSlider(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step)
    {
      auto inl = new Process::ControlInlet{getStrongId(fx.inlets()), &fx};
      inl->setCustomData(label);
      inl->setDomain(ossia::make_domain(min, max));
      inl->setValue(init);
      fx.inlets().push_back(inl);
    }

    void addHorizontalSlider(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step)
    { addVerticalSlider(label, zone, init, min, max, step); }

    void addNumEntry(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step)
    { addVerticalSlider(label, zone, init, min, max, step); }

    void addHorizontalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max)
    { }

    void addVerticalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max)
    { addHorizontalBargraph(label, zone, min, max); }
};

template<typename Proc>
struct UpdateUI
{
    Proc& fx;
    std::size_t i = 1;
    setup_ui<UpdateUI> glue{*this};

    UpdateUI(Proc& sfx): fx{sfx} { }

    void openTabBox(const char* label) { }
    void openHorizontalBox(const char* label) { }
    void openVerticalBox(const char* label) { }
    void closeBox() { }
    void declare(FAUSTFLOAT* zone, const char* key, const char* val) { }
    void addSoundfile(const char* label, const char* filename, Soundfile** sf_zone) { }

    void addButton(const char* label, FAUSTFLOAT* zone)
    {
      Process::ControlInlet* inlet{};
      if(i < fx.inlets().size())
      {
        inlet = static_cast<Process::ControlInlet*>(fx.inlets()[i]);
        inlet->setCustomData(label);
        inlet->setDomain(ossia::make_domain(false, true));
      }
      else
      {
        inlet = new Process::ControlInlet{getStrongId(fx.inlets()), &fx};
        inlet->setCustomData(label);
        inlet->setDomain(ossia::make_domain(false, true));
        fx.inlets().push_back(inlet);
        fx.controlAdded(inlet->id());
      }
      i++;
    }

    void addCheckButton(const char* label, FAUSTFLOAT* zone)
    { addButton(label, zone); }

    void addVerticalSlider(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step)
    {
      Process::ControlInlet* inlet{};
      if(i < fx.inlets().size())
      {
        inlet = static_cast<Process::ControlInlet*>(fx.inlets()[i]);
        inlet->setCustomData(label);
        inlet->setDomain(ossia::make_domain(min, max));
        inlet->setValue(init);
        inlet->hidden = true;
      }
      else
      {
        inlet = new Process::ControlInlet{getStrongId(fx.inlets()), &fx};
        inlet->setCustomData(label);
        inlet->setDomain(ossia::make_domain(min, max));
        inlet->setValue(init);
        inlet->hidden = true;
        fx.inlets().push_back(inlet);
        fx.controlAdded(inlet->id());
      }
      i++;
    }

    void addHorizontalSlider(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step)
    { addVerticalSlider(label, zone, init, min, max, step); }

    void addNumEntry(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step)
    { addVerticalSlider(label, zone, init, min, max, step); }

    void addHorizontalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max)
    { }

    void addVerticalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max)
    { addHorizontalBargraph(label, zone, min, max); }
};


template<typename Node>
struct ExecUI final
{
    Node& fx;
    setup_ui<ExecUI> glue{*this};
    ExecUI(Node& n): fx{n} { }

    void addButton(const char* label, FAUSTFLOAT* zone)
    {
      fx.inputs().push_back(ossia::make_inlet<ossia::value_port>());
      fx.controls.push_back({fx.inputs().back()->data.template target<ossia::value_port>(), zone});
    }

    void addCheckButton(const char* label, FAUSTFLOAT* zone)
    { addButton(label, zone); }

    void addVerticalSlider(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step)
    {
      fx.inputs().push_back(ossia::make_inlet<ossia::value_port>());
      fx.controls.push_back({fx.inputs().back()->data.template target<ossia::value_port>(), zone});
    }

    void addHorizontalSlider(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step)
    { addVerticalSlider(label, zone, init, min, max, step); }

    void addNumEntry(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step)
    { addVerticalSlider(label, zone, init, min, max, step); }

    void addHorizontalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max)
    { fx.outputs().push_back(ossia::make_outlet<ossia::value_port>()); }

    void addVerticalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max)
    { addHorizontalBargraph(label, zone, min, max); }

    void openTabBox(const char* label) { }
    void openHorizontalBox(const char* label) { }
    void openVerticalBox(const char* label) { }
    void closeBox() { }
    void declare(FAUSTFLOAT* zone, const char* key, const char* val) { }
    void addSoundfile(const char* label, const char* filename, Soundfile** sf_zone) { }
};


template<typename Node, typename Dsp>
void faust_exec(Node& self, Dsp& dsp, const ossia::token_request& tk)
{
  if(tk.date > self.prev_date())
  {
    std::size_t d = tk.date - self.prev_date();
    for(auto ctrl : self.controls)
    {
      auto& dat = ctrl.first->get_data();
      if(!dat.empty())
      {
        *ctrl.second = ossia::convert<float>(dat.back().value);
      }
    }

    auto& audio_in = *self.inputs()[0]->data.template target<ossia::audio_port>();
    auto& audio_out = *self.outputs()[0]->data.template target<ossia::audio_port>();

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
}


}
