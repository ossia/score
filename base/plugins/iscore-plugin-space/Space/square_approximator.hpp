#pragma once

namespace spacelib {

template<int size = 800, int side = 5>
class square_approx
{
    public:
        square_approx()
        {

        }

        template<typename Space>
        square_approx(const Space& s, int dim) // TODO  require concept bounded_space with min/max
        {

        }

        class iterator
        {
            public:
                constexpr iterator() = default;
                constexpr iterator(int p): pos{p} { }

                constexpr iterator operator++()
                { return ++pos, *this; }
                constexpr iterator operator++(int)
                { return ++pos, *this; }

                constexpr int operator*() const
                { return pos * side; }

                constexpr bool operator!=(iterator other) const
                { return other.pos != pos; }

            private:
                int pos = 0;
        };


        constexpr iterator begin() const
        { return iterator(); }
        constexpr iterator end() const
        { return iterator(size/side); }
};

template<int size = 800, int side = 5>
class square_approx_factory
{
    public:
        auto operator()(int) const { return square_approx<size, side>(); }
};

class dynamic_square_approx
{
    private:
        const int m_size{};
        const int m_side = 5;
    public:
        dynamic_square_approx(int size, int side):
            m_size{size},
            m_side{side}
        {
        }

        class iterator
        {
            public:
                constexpr iterator(int side): m_side{side} { }
                constexpr iterator(int p, int side): m_pos{p}, m_side{side} { }

                constexpr iterator operator++()
                { return ++m_pos, *this; }
                constexpr iterator operator++(int)
                { return ++m_pos, *this; }

                int operator*() const
                { return m_pos * m_side; }

                constexpr bool operator!=(iterator other) const
                { return other.m_pos != m_pos; }

            private:
                int m_pos = 0;
                int m_side = 0;
        };


        iterator begin() const
        { return iterator(m_side); }
        iterator end() const
        { return iterator(m_size/m_side, m_side); }
};

template<typename Space>
class dynamic_square_approx_factory
{
        const Space& m_space;
    public:
        dynamic_square_approx_factory(const Space& s):
            m_space{s}
        {

        }


        auto operator()(int dim) const{
            const auto& dom = m_space.variables()[dim].domain();
            return dynamic_square_approx(dom.max - dom.min, 5);
        }

};
}
