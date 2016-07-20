#include "dynamictreewidgetitem.h"

DynamicTreeWidgetItem::DynamicTreeWidgetItem(PeerFileTransfer &transfer, QTreeWidget *view) :
  m_transfer(transfer),
  QTreeWidgetItem(view) {}

void DynamicTreeWidgetItem::filePercentage(int value) { // SLOT
  qDebug() << "Updating item with value " << value;
  this->setData(0, Qt::UserRole /* Progressbar value */, QVariant::fromValue(value));
  if (m_transfer.getType() == CLIENT) {
    QString localFile = m_transfer.getClientSourceFilePath();
    if (localFile.isEmpty())
      localFile = "[N/A]"; // Should never happen
    this->setText(2, localFile); // Source file (local file that we're sending)
  } else { // SERVER
    QString destinationFile = m_transfer.getServerDestinationFilePath();
    if (destinationFile.isEmpty())
      destinationFile = "[N/A]";
    this->setText(2, destinationFile); // Destination (local file written)
  }
}
