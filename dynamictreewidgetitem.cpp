#include "dynamictreewidgetitem.h"

DynamicTreeWidgetItem::DynamicTreeWidgetItem(QTreeWidget *view) :
  QTreeWidgetItem(view) {}

void DynamicTreeWidgetItem::filePercentage(int value) { // SLOT
  this->setData(0, Qt::UserRole /* Progressbar value */, QVariant::fromValue(value));
}

void DynamicTreeWidgetItem::destinationAvailable(QString destination) { // SLOT
  this->setText(2, destination); // Destination (local file written)
}
