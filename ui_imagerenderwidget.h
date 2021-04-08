/********************************************************************************
** Form generated from reading UI file 'imagerenderwidget.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_IMAGERENDERWIDGET_H
#define UI_IMAGERENDERWIDGET_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ImageRenderWidget
{
public:
    QHBoxLayout *horizontalLayout;
    QSpacerItem *horizontalSpacer;
    QPushButton *playButton;
    QSpacerItem *horizontalSpacer_2;

    void setupUi(QWidget *ImageRenderWidget)
    {
        if (ImageRenderWidget->objectName().isEmpty())
            ImageRenderWidget->setObjectName(QString::fromUtf8("ImageRenderWidget"));
        ImageRenderWidget->resize(400, 300);
        horizontalLayout = new QHBoxLayout(ImageRenderWidget);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        playButton = new QPushButton(ImageRenderWidget);
        playButton->setObjectName(QString::fromUtf8("playButton"));

        horizontalLayout->addWidget(playButton);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_2);


        retranslateUi(ImageRenderWidget);

        QMetaObject::connectSlotsByName(ImageRenderWidget);
    } // setupUi

    void retranslateUi(QWidget *ImageRenderWidget)
    {
        ImageRenderWidget->setWindowTitle(QCoreApplication::translate("ImageRenderWidget", "Form", nullptr));
        playButton->setText(QCoreApplication::translate("ImageRenderWidget", "PushButton", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ImageRenderWidget: public Ui_ImageRenderWidget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_IMAGERENDERWIDGET_H
