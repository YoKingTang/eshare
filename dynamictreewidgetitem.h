#ifndef DYNAMICTREEWIDGETITEM_H
#define DYNAMICTREEWIDGETITEM_H

#include "peerfiletransfer.h"
#include <QTreeWidget>

// A dynamic tree widget item is a QTreeWidgetItem which also features slots to be
// automatically updated by transfer modules on the progress of file transfers
class DynamicTreeWidgetItem : public QObject, public QTreeWidgetItem {
  Q_OBJECT
public:
  DynamicTreeWidgetItem(PeerFileTransfer& transfer, QTreeWidget *view);

private:
  PeerFileTransfer& m_transfer;

public slots:

  // Updates all dynamic info every time there's progress on the transfer
  void filePercentage(int value);
};

#endif // DYNAMICTREEWIDGETITEM_H
