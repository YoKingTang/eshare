#ifndef PICKRECEIVER_H
#define PICKRECEIVER_H

#include <QDialog>
#include <QCompleter>
#include "mainwindow.h"

namespace Ui {
class PickReceiver;
}

class PickReceiver : public QDialog
{
  Q_OBJECT

public:
  explicit PickReceiver(QStringList completionWords, MainWindow *parent = 0);
  ~PickReceiver();

  int getSelectedItem() const;

private slots:
  void on_pushButton_OK_clicked();

  void on_pushButton_Cancel_clicked();

private:
  Ui::PickReceiver *ui;
  QCompleter *m_completer = nullptr;

};

#endif // PICKRECEIVER_H