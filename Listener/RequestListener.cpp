#include <Listener/RequestListener.h>

void RequestListener::run() // Main thread entry point
{
  m_server = std::make_unique<QTcpServer>(this);


}
