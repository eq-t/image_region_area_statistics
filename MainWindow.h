#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QMessageBox>
#include <opencv2/opencv.hpp>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void loadImage();

private:
    Ui::MainWindow* ui;
    QLabel* imageLabel;
    QPushButton* loadButton;
    QListWidget* areaList;

    cv::Mat img;        // 原图
    cv::Mat resultImg;  // 可视化区域

    void processImage();
    double colorDistance(const cv::Vec3b& a, const cv::Vec3b& b);
};