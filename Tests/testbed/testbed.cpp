#include <type_traits>

struct Foo
{
        Foo(int) { }
};
struct Bar
{
        Bar() { }
};

struct Blah
{
        template<typename std::enable_if_t<Foo(1)> * = nullptr>
        Blah(int i)
        {
            Foo f(i);
        }
        template<typename std::enable_if_t<Foo(1)> * = nullptr>
        Blah(int i)
        {
            Foo f(i);
        }
};

int main()
{
    //Serializer<DataStream>::marshall(RealNode{});
    return 0;
}
