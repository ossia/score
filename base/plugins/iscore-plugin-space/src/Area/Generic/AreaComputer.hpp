#pragma once

#include "src/Area/AreaPresenter.hpp"
#include "GenericAreaView.hpp"
#include <src/Area/AreaParser.hpp>
#include <Space/area.hpp>
#include <Space/square_renderer.hpp>
#include <atomic>
#include <QThread>

#include <vtk/vtkFunctionParser.h>
#include <vtk/vtkSmartPointer.h>
namespace Space
{
using space2d = spacelib::space_t<spacelib::minmax_symbol, 2>;

/*
class AreaComputer : public QObject
{
        Q_OBJECT

        class LimitFilter : public QObject
        {
                const AreaComputer& m_area;
            public:
                LimitFilter(const AreaComputer& area, QObject* parent):
                    QObject{parent},
                    m_area{area}
                {
                }

                bool eventFilter(QObject *obj, QEvent *event)
                {
                    if(m_area.computing)
                        return true;
                    return QObject::eventFilter(obj, event);
                }
        };
    public:
        AreaComputer()
        {
            renderer.size = {800, 600};
            m_cp_thread.start();

            this->installEventFilter(new LimitFilter{*this, this});
            this->moveToThread(&m_cp_thread);
        }

    signals:
        void ready(QVector<QRectF>);

    public slots:
        void computeArea(spacelib::valued_area a, space2d s)
        {
            computing = true;

            renderer.render(a,s);
            emit ready(renderer.render_device.rects);

            computing = false;
        }

    private:
        QThread m_cp_thread;
        std::atomic_bool computing{false};
        spacelib::square_renderer<QPointF, RectDevice> renderer;

};
*/


class AreaComputer : public QObject
{
        Q_OBJECT

        enum RelOp { inf, inf_eq, sup, sup_eq, eq } ;
        struct LocalSpace
        {
                static constexpr int dimension() { return 2; }
                std::array<GiNaC::symbol, 2> arr;

                auto& variables() const { return arr; }
        };

        class LimitFilter : public QObject
        {
                const AreaComputer& m_area;
            public:
                LimitFilter(const AreaComputer& area, QObject* parent):
                    QObject{parent},
                    m_area{area}
                {
                }

                bool eventFilter(QObject *obj, QEvent *event)
                {
                    if(m_area.computing)
                        return true;
                    return QObject::eventFilter(obj, event);
                }
        };

        struct VtkFun
        {
                vtkSmartPointer<vtkFunctionParser> lhs = vtkSmartPointer<vtkFunctionParser>::New();
                vtkSmartPointer<vtkFunctionParser> rhs = vtkSmartPointer<vtkFunctionParser>::New();
                GiNaC::relational::operators op{};
        };

        std::vector<VtkFun> vtk_vec;

        using pair_t = std::pair<
            QStringList,
            GiNaC::relational::operators
        >;
    public:
        AreaComputer(const QStringList& formulas)
        {

            AreaParser p(formulas);
            auto vec = std::move(p.m_parsed);


            for(pair_t& form : vec)
            {
                VtkFun f;
                f.lhs->SetFunction(form.first[0].toLatin1().constData());
                f.rhs->SetFunction(form.first[1].toLatin1().constData());

                f.op = form.second;
                vtk_vec.push_back(f);

                // Evaluate both parts
                // Check if the relation works
            }


            m_cp_thread.start();

            this->installEventFilter(new LimitFilter{*this, this});
            this->moveToThread(&m_cp_thread);
        }

    signals:
        void ready(QVector<QRectF>);

    public slots:

        void computeArea(SpaceMap sm, ValMap vals)
        {
            computing = true;
            if(sm.size() != 2)
            {
                return;
            }

            auto x_str = sm.begin()->toStdString().c_str();
            auto y_str = (++sm.begin())->toStdString().c_str();

            for(VtkFun& f : vtk_vec)
            {
                for(auto val : vals)
                {
                    f.lhs->SetScalarVariableValue(val.first.c_str(), val.second);
                    f.rhs->SetScalarVariableValue(val.first.c_str(), val.second);
                }
            }


            QVector<QRectF> rects;
            const int max_x = 800;
            const int max_y = 600;
            const double side = 3;
            rects.reserve((max_x / side) * (max_y / side));
            for(int i = 0; i < max_x; i += side)
            {
                for(int j = 0; j < max_y; j += side)
                {
                    double x = i;
                    double y = j;

                    for(VtkFun& fun : vtk_vec)
                    {
                        fun.lhs->SetScalarVariableValue(x_str, x);
                        fun.lhs->SetScalarVariableValue(y_str, y);
                        double lhs_res = fun.lhs->GetScalarResult();
                        fun.rhs->SetScalarVariableValue(x_str, x);
                        fun.rhs->SetScalarVariableValue(y_str, y);
                        double rhs_res = fun.rhs->GetScalarResult();

                        bool ok = false;
                        switch(fun.op)
                        {
                            case GiNaC::relational::operators::equal:
                            {
                                ok = lhs_res == rhs_res;
                                break;
                            }
                            case GiNaC::relational::operators::not_equal:
                            {
                                ok = lhs_res != rhs_res;
                                break;
                            }
                            case GiNaC::relational::operators::less:
                            {
                                ok = lhs_res < rhs_res;
                                break;
                            }
                            case GiNaC::relational::operators::less_or_equal:
                            {
                                ok = lhs_res <= rhs_res;
                                break;
                            }
                            case GiNaC::relational::operators::greater:
                            {
                                ok = lhs_res > rhs_res;
                                break;
                            }
                            case GiNaC::relational::operators::greater_or_equal:
                            {
                                ok = lhs_res > rhs_res;
                                break;
                            }
                        }

                        if(ok)
                        {
                            rects.push_back(QRectF{x - side/2., y- side/2., side, side});
                        }
                    }


                }
            }

            emit ready(rects);

            computing = false;
        }

        /*
        void computeArea(QStringList formula, SpaceMap sm, ValMap vals)
        {
            computing = true;

            std::unique_ptr<spacelib::area> area = AreaParser{formula}.result();

            // Space
            LocalSpace sp;

            // Parameters
            GiNaC::exmap parametermap;

            for(const GiNaC::symbol& sym : area->symbols())
            {
                auto name = sym.get_name();
                if(name == sm.begin()->toStdString())
                {
                    sp.arr[0] = sym;
                }
                else if(name == (++sm.begin())->toStdString())
                {
                    sp.arr[1] = sym;
                }
                else
                {
                    auto it = vals.find(name);
                    if(it != vals.end())
                        parametermap.insert(std::make_pair(sym, it->second));
                }
            }


            std::vector<GiNaC::relational> rels;
            rels.reserve(formula.length());
            for(const GiNaC::relational& rel : area->rels())
            {
                rels.push_back(GiNaC::ex_to<GiNaC::relational>(rel.subs(parametermap)));
            }

            QVector<QRectF> rects;
            const double side = 5;
            for(int i = 0; i < 800; i += side)
            {
                for(int j = 0; j < 600; j += side)
                {
                    double x = i;
                    double y = j;
                    GiNaC::exmap map{std::make_pair(sp.arr[0], x), std::make_pair(sp.arr[1], y)};
                    try {
                        if(std::accumulate(rels.begin(), rels.end(),
                                           true,
                                           [&] (bool cur, const GiNaC::relational& rel) {
                                  return cur && bool(GiNaC::ex_to<GiNaC::relational>(rel.subs(map)));
                    } ))
                        {
                            rects.push_back(QRectF{x - side/2., y- side/2., side, side});
                        }
                    } catch(...)
                    {

                    }

                }
            }

            emit ready(rects);

            computing = false;
        }*/

    private:
        QThread m_cp_thread;
        std::atomic_bool computing{false};

};
}
