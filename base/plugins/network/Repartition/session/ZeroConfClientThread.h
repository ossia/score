#ifndef MYTHREAD_H
#define MYTHREAD_H

#include <QThread>

#include <session/ZeroConfClient.h>
#include <thread>

class ZeroConfClientThread : public QThread
{
		Q_OBJECT
	public:
		std::list<BonjourRecord> getRecords()
		{
			return _cb->getRecords().toStdList();
		}

		std::vector<ConnectionData> getData()
		{
			return _cb->_connectData;
		}

	protected:
		void run()
		{
			_cb.reset(new ZeroConfClient);

			exec();
		}

	private:
		std::unique_ptr<ZeroConfClient> _cb;
};

#endif // MYTHREAD_H
