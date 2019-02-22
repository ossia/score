// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "player.hpp"

#include <get_library_path.hpp>

#include <memory>
#include <sstream>
extern "C"
{
#include <cicm_wrapper.h>
}
#include <iostream>

#include <ossia-pd/src/device.hpp>
#include <ossia-pd/src/utils.hpp>
namespace score
{
namespace player
{

struct t_score
{
  t_eobj obj; // pd object - always placed first in the object's struct
  std::unique_ptr<score::Player> p;
};

static t_eclass* score_class = nullptr;

static void score_bang(t_score* x)
{
}
static void score_find_device(t_score* x)
{
  if (!x || (x && !x->p))
    return;

  int level{};
  auto parent = ossia::pd::find_parent(&x->obj, "ossia.device", 0, &level);
  if (parent)
  {
    auto dev = ((ossia::pd::device*)parent)->m_device;
    if (dev)
    {
      x->p->registerDevice(*dev);
    }
  }
}

static void score_float(t_score* x, t_float f)
{
  if (f > 0.)
  {
    x->p->play();
  }
  else
  {
    x->p->stop();
  }
}

static void score_load(t_score* x, t_symbol* s)
{
  try
  {
    x->p->load(s->s_name);
  }
  catch (std::exception& e)
  {
    pd_error(x, "can't open file %s: %s", s->s_name, e.what());
  }
}

static void* score_new(t_symbol* name, int argc, t_atom* argv)
{
  t_score* x = (t_score*)eobj_new(score_class);
  auto path = get_library_path("score.");
  post("score player: %s", path.c_str());
  x->p = std::make_unique<score::Player>(path + "/plugins");

  return (x);
}

static void score_free(t_score* x)
{
  x->p.reset();
}

extern "C" void setup_i0x2dscore(void)
{
  setenv("LC_NUMERIC", "C", 1);

  t_eclass* c = eclass_new(
      "score", (method)score_new, (method)score_free, sizeof(t_score),
      CLASS_DEFAULT, A_GIMME, 0);

  // it is checked in eobj_new but never initialized.
  c->c_class.c_patchable = true;

  if (c)
  {
    eclass_addmethod(c, (method)score_bang, "bang", A_NULL, 0);
    eclass_addmethod(c, (method)score_float, "float", A_FLOAT, 0);
    eclass_addmethod(c, (method)score_load, "load", A_SYMBOL, 0);
    eclass_addmethod(c, (method)score_find_device, "device", A_NULL, 0);
  }

  score_class = c;
}
}
}
