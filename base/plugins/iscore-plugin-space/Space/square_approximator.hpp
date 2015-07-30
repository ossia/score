#pragma once

namespace spacelib {

template<int size = 800, int side = 5>
class square_approx
{
    public:
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

}
