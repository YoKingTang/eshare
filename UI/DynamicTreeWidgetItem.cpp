#include <UI/DynamicTreeWidgetItem.h>

DynamicTreeWidgetItem::DynamicTreeWidgetItem(QTreeWidget *view) :
  m_view(view),
  QTreeWidgetItem(view) {}

void DynamicTreeWidgetItem::update_percentage(int value) // SLOT
{
  this->setData(0, Qt::UserRole + 2/* Progressbar value */, QVariant::fromValue(value));
  this->emitDataChanged();
}
