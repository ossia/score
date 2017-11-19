//#pragma once
//#include <QWidget>
//#include <State/Address.hpp>
//#include <ossia/network/base/parameter.hpp>
//namespace ossia
//{
//namespace net
//{
//class node_base;
//}
//}
//namespace score
//{
//class DoubleSlider;
//}

//namespace Media
//{
//namespace Effect
//{
//class EffectSlider :
//        public QWidget,
//        public Nano::Observer
//{
//    Q_OBJECT
//    public:
//        EffectSlider(const ossia::net::node_base& fx, bool is_output, QWidget* parent);

//        ~EffectSlider();

//        double scaledValue;
//        score::DoubleSlider* m_slider{};
//    signals:
//        void createAutomation(const State::Address&, double min, double max);

//    private:
//        void contextMenuEvent(QContextMenuEvent* event) override;
//        void on_paramDeleted(const ossia::net::node_base&);

//        const ossia::net::node_base& m_param;
//        ossia::net::parameter_base::callback_index m_callback;
//        float m_min{0.};
//        float m_max{1.};

//        QAction* m_addAutomAction{};
//        bool m_paramIsDead{false};
//};
//}
//}
