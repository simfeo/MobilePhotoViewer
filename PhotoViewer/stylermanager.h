#ifndef STYLERMANAGER_H
#define STYLERMANAGER_H

// Copyright (c) 2018, Juergen Skrotzky (https://github.com/Jorgen-VikingGod, JorgenVikingGod@gmail.com)

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QProxyStyle>
#include <QStyleFactory>

class StylerManager : public QProxyStyle {
  Q_OBJECT

 public:
  StylerManager();
  explicit StylerManager(QStyle *style);

  QStyle *baseStyle() const;

  void polish(QPalette &palette) override;
  void polish(QApplication *app) override;

 private:
  QStyle *styleBase(QStyle *style = Q_NULLPTR) const;
};
#endif // STYLERMANAGER_H
