#pragma once
/*
#include <QString>
#include <iterator>
#include <Media/MediaStreamEngine/Streams/MediaStreamIScoreExtensions.h>

namespace Media
{
namespace Effect
{
struct InParameter
{
        static auto getControlCount() { return GetControlCountEffect; }
        static auto getControlParam() { return GetControlParamEffect; }
};

#if defined(LILV_SHARED)
struct OutParameter
{
        static auto getControlCount() { return GetControlOutCount; }
        static auto getControlParam() { return GetControlOutParam; }
};
#endif

struct EffectParameter
{
        EffectParameter() = default;
        EffectParameter(const EffectParameter&) = default;
        EffectParameter(EffectParameter&&) = default;
        EffectParameter& operator=(const EffectParameter&) = default;
        EffectParameter& operator=(EffectParameter&&) = default;
        int64_t id{};
        float min{};
        float max{};
        float init{};

        QString label{};
};

template<typename T>
struct MediaEffectParameterAdaptor
{
        MediaEffect effect;
};

template<typename T>
struct EffectParameterIterator final :
    public std::iterator<
        std::input_iterator_tag,
        EffectParameter,
        int64_t,
        const EffectParameter*,
        EffectParameter>
{
        MediaEffectParameterAdaptor<T> effect;
        int64_t num = 0;
        EffectParameter param;

    public:
        EffectParameterIterator(const EffectParameterIterator& other) = default;
        EffectParameterIterator(EffectParameterIterator&& other) = default;
        EffectParameterIterator& operator=(const EffectParameterIterator& other) = default;
        EffectParameterIterator& operator=(EffectParameterIterator&& other) = default;

        explicit EffectParameterIterator(
                MediaEffectParameterAdaptor<T> p,
                int64_t n = 0) :
            effect{p}, num{n}, param{readEffect()}
        {

        }

        EffectParameterIterator& operator++()
        {
            ++num;
            param = readEffect();
            return *this;
        }

        EffectParameterIterator operator++(int)
        {
            EffectParameterIterator retval{*this};
            ++(*this);
            return retval;
        }

        bool operator==(const EffectParameterIterator& other) const
        {
            return num == other.num;
        }

        bool operator!=(const EffectParameterIterator& other) const
        {
            return !(*this == other);
        }

        const EffectParameter& operator*() const
        {
            return param;
        }

    private:
        EffectParameter readEffect()
        {
            EffectParameter e;
            if(num < T::getControlCount()(effect.effect))
            {
                char buf[512]{};
                T::getControlParam()(effect.effect, num, buf, &e.min, &e.max, &e.init);
                e.label = buf;
                e.id = num;
            }
            return e;
        }
};

template<typename T>
auto begin(const MediaEffectParameterAdaptor<T>& fx)
{
    return EffectParameterIterator<T>(fx, 0);
}

template<typename T>
auto end(const MediaEffectParameterAdaptor<T>& fx)
{
    return EffectParameterIterator<T>(fx, T::getControlCount()(fx.effect));
}
}
}
*/
