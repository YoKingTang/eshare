#ifndef PEERTHREADTRANSFER_H
#define PEERTHREADTRANSFER_H

#include "peerfiletransfer.h"
#include <QThread>
#include <tuple>

class MainWindow;

class PeerThreadTransfer : public QThread
{
  Q_OBJECT
public:
  explicit PeerThreadTransfer(TransferType type,
                              MainWindow *mainWindow,
                              size_t peerIndex,
                              std::tuple<QString /* Ip */, int /* port */, QString /* hostname */>
                              peer, QString fileOrDownloadPath, qintptr socketDescriptor, QObject *parent = 0);

private:
  friend class MainWindow; // Only authorized classes can run this

  void run() Q_DECL_OVERRIDE;  

  TransferType m_type;
  MainWindow *m_mainWindow = nullptr;
  size_t m_peerIndex = -1;
  std::tuple<QString, int, QString> m_peer;
  QString m_fileOrDownloadPath;
  qintptr m_socketDescriptor;

private slots:
  void filePercentageSlot(int value);
  void destinationAvailableSlot(QString destination);

signals:
  void filePercentage(int value);
  void destinationAvailable(QString destination);
};

#endif // PEERTHREADTRANSFER_H
