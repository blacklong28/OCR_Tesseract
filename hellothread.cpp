void HelloThread::run()
{
	qDebug() << "hello from worker thread " << thread()->currentThreadId();
}