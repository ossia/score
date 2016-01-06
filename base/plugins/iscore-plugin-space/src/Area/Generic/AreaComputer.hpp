#pragma once

#include "src/Area/AreaPresenter.hpp"
#include "GenericAreaView.hpp"
#include <src/Area/AreaParser.hpp>
#include <Space/area.hpp>
#include <Space/square_renderer.hpp>
#include <atomic>
#include <QThread>

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
            //renderer.size = {800, 600};
            m_cp_thread.start();

            this->installEventFilter(new LimitFilter{*this, this});
            this->moveToThread(&m_cp_thread);
        }

    signals:
        void ready(QVector<QRectF>);

    public slots:
        void computeArea(QStringList formula, SpaceMap sm, ValMap vals)
        {
            computing = true;

            auto res = AreaParser{formula}.result();

            auto val_ar = spacelib::valued_area(spacelib::projected_area(res, sm), vals);
            spacelib::square_renderer<QPointF, RectDevice> renderer;

            renderer.size = {800, 600};

            // Convert our dynamic space to a static one for rendering
            GiNaC::exmap gmap;
            for(auto& val : model(this).parameterMapping())
            {
                auto it = map.find(val.first.get_name());
                if(it != map.end())
                {
                    gmap.insert(std::make_pair(val.first, it->second));
                }
            }

            renderer.render(a,s);

            emit ready(renderer.render_device.rects);

            computing = false;
        }

    private:
        QThread m_cp_thread;
        std::atomic_bool computing{false};

};
}
