#pragma once
#include <ginac/ginac.h>
#include <type_traits>

namespace spacelib
{
// Iterates on all the dimensions of the space and call
// a function on each point


template<typename Space_t, int Dimension>
using dim_less_t = std::enable_if_t< Dimension < std::decay_t<Space_t>::dimension() - 1 >;

template<typename Space_t, int Dimension>
using dim_equal_t = std::enable_if_t< Dimension == std::decay_t<Space_t>::dimension() - 1 >;


template<typename Space, typename Approx, typename Fun>
class dimension_iterator
{
    public:
        dimension_iterator(
                const Space& s,
                const Approx& approx,
                Fun&& f):
            m_space{s},
            m_fun{std::move(f)},
            m_approx{approx}
        {
        }

    protected:
        const Space& m_space;
        const Fun m_fun;
        const Approx m_approx;
};

template<typename Space, typename Approx, typename Fun>
class dimension_apply : public dimension_iterator<Space, Approx, Fun>
{
    public:
        using dimension_iterator<Space, Approx, Fun>::dimension_iterator;

        void rec() const
        {
            GiNaC::exmap var_map;
            rec_impl<0>(var_map);
        }

    private:
        template<int Dimension, dim_less_t<Space, Dimension>* = nullptr >
        void rec_impl(GiNaC::exmap& var_map) const
        {
            for(int i : this->m_approx(Dimension))
            {
                var_map[this->m_space.variables()[Dimension].symbol()] = i;
                rec_impl<Dimension+1>(var_map);
            }
        }

        template<int Dimension, dim_equal_t<Space, Dimension>* = nullptr >
        void rec_impl(GiNaC::exmap& var_map) const
        {
            for(int i : this->m_approx(Dimension))
            {
                var_map[this->m_space.variables()[Dimension].symbol()] = i;
                this->m_fun(var_map);
            }
        }
};

template<typename Space, typename Approx, typename Fun>
class dimension_eval : public dimension_iterator<Space, Approx, Fun>
{
    public:
        using dimension_iterator<Space, Approx, Fun>::dimension_iterator;

        bool rec() const
        {
            GiNaC::exmap var_map;
            return rec_impl<0>(var_map);
        }

    private:
        template<int Dimension, dim_less_t<Space, Dimension>* = nullptr >
        bool rec_impl(GiNaC::exmap& var_map) const
        {
            for(int i : this->m_approx)
            {
                var_map[this->m_space.variables()[Dimension]] = i;
                if(rec_impl<Dimension+1>(var_map))
                    return true;
            }
            return false;
        }

        template<int Dimension, dim_equal_t<Space, Dimension>* = nullptr >
        bool rec_impl(GiNaC::exmap& var_map) const
        {
            for(int i : this->m_approx)
            {
                var_map[this->m_space.variables()[Dimension]] = i;
                if(this->m_fun(var_map))
                    return true;
            }

            return false;
        }
};
}
