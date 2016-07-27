#ifndef TRANSFERTREEVIEW_H
#define TRANSFERTREEVIEW_H

#include <UI/DynamicTreeWidgetItemDelegate.h>
#include <QTreeWidget>

// TransfersView is the tree widget which lists all of the ongoing file transfers
class TransferTreeView : public QTreeWidget
{
  Q_OBJECT
public:
    TransferTreeView(QWidget*parent);

    void resetDelegate();

private:
    bool eventFilter(QObject *obj, QEvent *event);

    DynamicTreeWidgetItemDelegate *m_delegate;

public slots:
    void clicked(const QModelIndex item);

signals:
    void click(const QModelIndex item);
};

#endif // TRANSFERTREEVIEW_H
