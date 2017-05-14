#include <sstream>
#include <memory>
#include "player.hpp"
#include <get_library_path.hpp>
extern "C" {
#include <cicm_wrapper.h>
}

namespace iscore {
namespace player {

struct t_iscore
{
    t_eobj     obj; // pd object - always placed first in the object's struct
    std::unique_ptr<iscore::Player> p;
};

static t_eclass *iscore_class;

static void iscore_bang(t_iscore* x){

}

static void iscore_float(t_iscore* x, t_float f){
    if (f > 0.){
       x->p->play();
    } else {
       x->p->stop();
    }
}

static void iscore_load(t_iscore* x, t_symbol* s){
    try{
      x->p->load(s->s_name);
    } catch (std::exception& e){
        pd_error(x,"can't open file %s: %s", s->s_name, e.what());
    }
}

static void *iscore_new(t_symbol *name, int argc, t_atom *argv)
{
    t_iscore *x = (t_iscore *)eobj_new(iscore_class);
    auto path = get_library_path("i-score.");
    post("i-score player: %s", path.c_str());

    x->p = std::make_unique<iscore::Player>(path + "/plugins");

    return (x);
}

static void iscore_free(t_iscore *x) {
  x->p.reset();
}

extern "C" void setup_i0x2dscore(void)
{
    t_eclass* c = eclass_new("i-score",
                           (method)iscore_new, (method)iscore_free,
                           sizeof(t_iscore), CLASS_DEFAULT, A_GIMME, 0);

    if (c){
        eclass_addmethod(c, (method) iscore_bang,  "bang",     A_NULL,   0);
        eclass_addmethod(c, (method) iscore_float, "float",    A_FLOAT,  0);
        eclass_addmethod(c, (method) iscore_load,  "load",     A_SYMBOL, 0);
    }
    iscore_class = c;
}


}
}
