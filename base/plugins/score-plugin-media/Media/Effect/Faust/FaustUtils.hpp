#pragma once
#include <Process/Dataflow/Port.hpp>
#include <ossia/network/domain/domain.hpp>
#include <faust/dsp/llvm-c-dsp.h>
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
      }
      else
      {
        inlet = new Process::ControlInlet{getStrongId(fx.inlets()), &fx};
        inlet->setCustomData(label);
        inlet->setDomain(ossia::make_domain(min, max));
        inlet->setValue(init);
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

}


}
