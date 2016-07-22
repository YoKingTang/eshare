#ifndef SOCKETRECYCLER_H
#define SOCKETRECYCLER_H

#include <QTcpServer>
#include <QList>

// This class overrides incomingConnection(descriptor) to store the descriptor and
// allow for later use
class TcpServerRecycler : public QTcpServer
{
  Q_OBJECT
public:
  TcpServerRecycler(QObject *parent = Q_NULLPTR);

  void incomingConnection(qintptr socketDescriptor) Q_DECL_OVERRIDE;
  bool hasPendingConnections() const Q_DECL_OVERRIDE;
  qintptr nextPendingDescriptor();

private:
  QList<qintptr> m_socketDescriptors;
};

#endif // SOCKETRECYCLER_H
