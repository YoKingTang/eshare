#include <UI/TransferTreeView.h>
#include <QEvent>
#include <QKeyEvent>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

#ifdef _WIN32
#include <windows.h>
#endif

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

void TransferTreeView::clicked(const QModelIndex item) // SLOT
{
  // Set style to progressbar
  this->model()->setData(item, false, Qt::UserRole + 0);
  emit click(item); // Forward on
}

void TransferTreeView::double_clicked(const QModelIndex item) // SLOT
{
#ifdef _WIN32
  auto file_path = this->topLevelItem(item.row())->data(2, Qt::DisplayRole).toString();
  QFileInfo finfo(file_path);
  ShellExecuteA(NULL, "open", finfo.dir().absolutePath().toStdString().data(),
               NULL, NULL, SW_SHOWDEFAULT);
#endif
}

// FIXME: this can probably be done in a better way (i.e. reset the delegate/model association)
void TransferTreeView::resetDelegate()
{
  if (m_delegate)
    delete m_delegate;
  m_delegate = new DynamicTreeWidgetItemDelegate(this);
  connect(m_delegate, SIGNAL(needsUpdate(const QModelIndex&)), this, SLOT(update(const QModelIndex&)));
  connect(m_delegate, SIGNAL(clicked(const QModelIndex)), this, SLOT(clicked(const QModelIndex)));
  this->setItemDelegate(m_delegate);
}
