#pragma once
#include <thread>
#include <QThread>

#include "ZeroConfServer.h"
#define FORWARD_DECL(funName) template<typename... K> funName(K&&... args)
#define FORWARD std::forward<K>(args)...

class ZeroConfServerThread : public QThread
{
		Q_OBJECT
		ZeroconfServer* _serv{};
		bool built{false};
	public:
		FORWARD_DECL(void setPort)
		{
			while(!built) QThread::msleep(50);
			_serv->setPort(FORWARD);
		}

		virtual ~ZeroConfServerThread()
		{
			if(_serv)
				delete _serv;
		}

	protected:
		void run()
		{
			_serv = new ZeroconfServer;
			built = true;
			exec();
		}
};
