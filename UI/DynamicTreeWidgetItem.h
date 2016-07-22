#ifndef DYNAMICTREEWIDGETITEM_H
#define DYNAMICTREEWIDGETITEM_H

#include <QTreeWidget>

// A dynamic tree widget item is a QTreeWidgetItem which also features slots to be
// automatically updated by transfer modules on the progress of file transfers
class DynamicTreeWidgetItem : public QObject, public QTreeWidgetItem {
  Q_OBJECT
public:
  DynamicTreeWidgetItem (QTreeWidget *view);

public slots:

  // Updates percentage data
  void filePercentage(int value);
  // Updates destination data (only for server items)
  void destinationAvailable(QString destination);
};

#endif // DYNAMICTREEWIDGETITEM_H
