#include <UI/TransferTreeView.h>
#include <QEvent>
#include <QKeyEvent>
#include <QDebug>

TransferTreeView::TransferTreeView(QWidget *parent) :
  QTreeWidget(parent)
{
  m_delegate = new DynamicTreeWidgetItemDelegate(this);
  connect(m_delegate, SIGNAL(needsUpdate(const QModelIndex&)), this, SLOT(update(const QModelIndex&)));
  connect(m_delegate, SIGNAL(clicked(const QModelIndex)), this, SLOT(clicked(const QModelIndex)));
  this->setItemDelegate(m_delegate);
  this->installEventFilter(this);
}

bool TransferTreeView::TransferTreeView::eventFilter(QObject *obj, QEvent *event)
{
  if (obj != this->viewport())
    return QWidget::eventFilter(obj, event);
  switch (event->type()) {
    case QEvent::Leave:
      m_delegate->notifyMouseLeave();
      break;
    case QEvent::MouseMove:
      QModelIndex index = this->indexAt(static_cast<QMouseEvent*>(event)->pos());
      if (!index.isValid())
        m_delegate->notifyMouseLeave();
      break;
  }
  return QWidget::eventFilter(obj, event);
}

void TransferTreeView::clicked(const QModelIndex item) { // SLOT
  // Set style to progressbar
  this->model()->setData(item, false, Qt::UserRole + 0);
  emit click(item); // Forward on
}
