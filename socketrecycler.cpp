#include "socketrecycler.h"

TcpServerRecycler::TcpServerRecycler(QObject *parent) :
  QTcpServer(parent)
{}

void TcpServerRecycler::incomingConnection(qintptr socketDescriptor) {
  m_socketDescriptors.append(socketDescriptor);
}

bool TcpServerRecycler::hasPendingConnections() const {
  return !m_socketDescriptors.empty();
}

qintptr TcpServerRecycler::nextPendingDescriptor() {
  qintptr d = m_socketDescriptors.front();
  m_socketDescriptors.pop_front();
  return d;
}
