#ifndef DYNAMICTREEWIDGETITEM_H
#define DYNAMICTREEWIDGETITEM_H

#include <QTreeWidget>

// A dynamic tree widget item is a QTreeWidgetItem which also features slots to be
// automatically updated by transfer modules on the progress of file transfers
class DynamicTreeWidgetItem : public QObject, public QTreeWidgetItem {
  Q_OBJECT
public:
  DynamicTreeWidgetItem (QTreeWidget *view);

private:
  QTreeWidget *m_view = nullptr;

public slots:

  void update_percentage(int value);
};

#endif // DYNAMICTREEWIDGETITEM_H
