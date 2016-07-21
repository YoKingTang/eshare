#include "peerthreadtransfer.h"
#include "mainwindow.h"

PeerThreadTransfer::PeerThreadTransfer(TransferType type,
                                       MainWindow *mainWindow,
                                       size_t peerIndex,
                                       std::tuple<QString, int, QString> peer,
                                       QString fileOrDownloadPath, qintptr socketDescriptor,
                                       QObject *parent) :
  m_type(type),
  m_mainWindow(mainWindow),
  m_peerIndex(peerIndex),
  m_peer(peer),
  m_fileOrDownloadPath(fileOrDownloadPath),
  m_socketDescriptor(socketDescriptor),
  QThread(parent)
{}

void PeerThreadTransfer::run() // This function will run in a different thread!
{
  //
  // Initialize a PeerFileTransfer encapsulation and link messages to our proxies
  //
  std::unique_ptr<PeerFileTransfer> transfer;
  Qt::ConnectionType ctype = static_cast<Qt::ConnectionType>(Qt::UniqueConnection | Qt::DirectConnection);
  if (m_type == CLIENT) {
    transfer = std::make_unique<PeerFileTransfer>(this, m_mainWindow, m_peerIndex, m_peer, m_fileOrDownloadPath);
    connect(transfer.get(), SIGNAL(fileSentPercentage(int)), this, SLOT(filePercentageSlot(int)), ctype);
  } else { // SERVER
    transfer = std::make_unique<PeerFileTransfer>(this, m_mainWindow, m_peerIndex, m_peer, m_socketDescriptor, m_fileOrDownloadPath);
    connect(transfer.get(), SIGNAL(fileReceivedPercentage(int)), this, SLOT(filePercentageSlot(int)), ctype);
    connect(transfer.get(), SIGNAL(destinationAvailable(QString)), this, SLOT(destinationAvailableSlot(QString)), ctype);
  }

  //
  // Start the appropriate event loop and resume connection handling
  //

  transfer->execute();

  exec(); // Start thread event loop and keep PeerFileTransfer alive
}

void PeerThreadTransfer::filePercentageSlot(int value) // Forwarding SLOT
{
  emit filePercentage(value);
}

void PeerThreadTransfer::destinationAvailableSlot(QString destination) // Forwarding SLOT
{
  emit destinationAvailable(destination);
}

