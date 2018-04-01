#pragma once
#include <Process/Dataflow/Port.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/dataflow/nodes/faust/faust_utils.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>

namespace Media
{
namespace Faust
{

template<typename Proc>
struct UI
{
    Proc& fx;
    ossia::nodes::faust_setup_ui<UI> glue{*this};

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
      inl->hidden = true;
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
      inl->hidden = true;
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
    ossia::nodes::faust_setup_ui<UpdateUI> glue{*this};

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
        inlet->hidden = true;
      }
      else
      {
        inlet = new Process::ControlInlet{getStrongId(fx.inlets()), &fx};
        inlet->setCustomData(label);
        inlet->setDomain(ossia::make_domain(false, true));
        inlet->hidden = true;
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

}
}
