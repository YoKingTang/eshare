#ifndef REQUESTLISTENER_H
#define REQUESTLISTENER_H

#include <QThread>
#include <QTcpServer>
#include <memory>

class RequestListener : public QThread {
  Q_OBJECT
public:

private:
  void run() Q_DECL_OVERRIDE;

  std::unique_ptr<QTcpServer> m_server;
};

#endif // REQUESTLISTENER_H
