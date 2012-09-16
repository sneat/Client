#pragma once

#include "Global.h"

#include "ui_MainWindow.h"

#include "CasparDevice.h"

#include <QtCore/QEvent>
#include <QtGui/QMainWindow>
#include <QtGui/QStackedLayout>
#include <QtGui/QWidget>

class MainWindow : public QMainWindow, Ui::MainWindow
{
    Q_OBJECT

    public:
        explicit MainWindow(QWidget* parent = 0);

    protected:
        bool eventFilter(QObject* target, QEvent* event);

    private:
        QStackedLayout* stackedLayout;

        void setupUiMenu();
        void removeWidgets();
        void enableDemoButton(const QString& buttonName);

        Q_SLOT void showAboutDialog();
        Q_SLOT void showStart();
        Q_SLOT void showRecorder();
        Q_SLOT void showBigFour();
        Q_SLOT void showSqueeze();
        Q_SLOT void deviceConnectionStateChanged(CasparDevice&);
};
