#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/utilsCPP11.hpp>
#include <typeinfo>
#include <random>

template<typename tag>
class IdentifiedObject : public IdentifiedObjectAbstract
{

    public:
        template<typename... Args>
        IdentifiedObject(const id_type<tag>& id,
                         Args&& ... args) :
            IdentifiedObjectAbstract {std::forward<Args> (args)...},
            m_id {id}
        {
        }

        template<typename ReaderImpl, typename... Args>
        IdentifiedObject(Deserializer<ReaderImpl>& v,
                         Args&& ... args) :
            IdentifiedObjectAbstract {v, std::forward<Args> (args)...}
        {
            v.writeTo(*this);
        }

        const id_type<tag>& id() const
        {
            return m_id;
        }

        virtual int32_t id_val() const override
        {
            return *m_id.val();
        }

        void setId(id_type<tag>&& id)
        {
            m_id = id;
        }

    private:
        id_type<tag> m_id {};
};
////////////////////////////////////////////////
///
///Functions that operate on collections of identified objects.
///
////////////////////////////////////////////////
template<typename T, typename U>
bool operator==(const T* obj, const id_type<U>& id)
{
    return obj->id() == id;
}

template<typename Container>
typename Container::value_type findById(const Container& c, int32_t id)
{
    auto it = std::find_if(std::begin(c),
                           std::end(c),
                           [&id](typename Container::value_type model)
    {
        return model->id_val() == id;
    });

    if(it != std::end(c))
    {
        return *it;
    }

    throw std::runtime_error(QString("findById : id %1 not found in vector of %2").arg(id).arg(typeid(c).name()).toLatin1().constData());
}

template<typename Container, typename id_T>
typename Container::value_type findById(const Container& c, const id_T& id)
{
    auto it = std::find(std::begin(c), std::end(c), id);
    if(it != std::end(c))
    {
        return *it;
    }

    throw std::runtime_error(QString("findById : id %1 not found in vector of %2").arg(*id.val()).arg(typeid(c).name()).toLatin1().constData());
}

inline int32_t getNextId()
{
    using namespace std;
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_int_distribution<int32_t>
    dist(numeric_limits<int32_t>::min(),
         numeric_limits<int32_t>::max());

    return dist(gen);
}

template<typename Vector>
inline int getNextId(const Vector& ids)
{
    using namespace std;
    int id {};

    do
    {
        id = getNextId();
    }
    while(find(begin(ids),
               end(ids),
               id) != end(ids));

    return id;
}

template<typename Vector,
         typename std::enable_if<
                    std::is_pointer<
                      typename Vector::value_type
                    >::value
                  >::type* = nullptr>
auto getStrongId(const Vector& v)
    -> id_type<typename std::remove_pointer<typename Vector::value_type>::type>
{
    using namespace std;
    vector<int> ids(v.size());   // Map reduce

    transform(begin(v),
              end(v),
              begin(ids),
              [](const typename Vector::value_type& elt)
    {
        return * (elt->id().val());
    });

    return id_type<typename std::remove_pointer<typename Vector::value_type>::type> {getNextId(ids)};
}

template<typename Vector,
         typename std::enable_if<
                    not std::is_pointer<
                      typename Vector::value_type
                    >::value
                  >::type* = nullptr>
auto getStrongId(const Vector& ids) ->
    typename Vector::value_type
{
    using namespace std;
    typename Vector::value_type id {};

    do
    {
        id = typename Vector::value_type{getNextId() };
    }
    while(find(begin(ids),
               end(ids),
               id) != end(ids));

    return id;
}

template <typename Vector, typename id_T>
void removeById(Vector& c, const id_T& id)
{
    vec_erase_remove_if(c,
                        [&id](typename Vector::value_type model)
    {
        bool to_delete = model->id() == id;

        if(to_delete)
        {
            delete model;
        }

        return to_delete;
    });
}

template<typename Vector, typename id_T>
void removeFromVectorWithId(Vector& v,
                            const id_T& id)
{
    auto it = std::find(std::begin(v), std::end(v), id);
    if(it != std::end(v))
    {
        delete *it;
        v.erase(it);
    }
}

