#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QTimer>
#include <opencv2/opencv.hpp>
#include <vector>
#include <map>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// 颜色聚类（简化版，保证稳定性）
struct ColorCluster {
    cv::Vec3b centerColor;  // 聚类中心颜色
    int pixelCount;         // 像素数量
    cv::Mat mask;           // 区域掩码（用于点击高亮）
    ColorCluster(const cv::Vec3b& color) : centerColor(color), pixelCount(0) {}
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    // 重写鼠标点击事件
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void loadImage();
    void flashHighlight(); // 高亮闪烁定时器槽函数
    void resetHighlight(); // 重置高亮

private:
    Ui::MainWindow* ui;
    QLabel* imageLabel;
    QPushButton* loadButton;
    QListWidget* areaList;
    QTimer* flashTimer;    // 闪烁定时器
    QTimer* resetTimer;    // 重置定时器

    cv::Mat img;                // 原图
    cv::Mat resultImg;          // 可视化区域
    cv::Mat denoisedImg;        // 降噪后的图
    cv::Mat originalResultImg;  // 原始标记图（用于重置）
    std::vector<ColorCluster> regions; // 颜色区域列表
    int highlightedRegionIdx;   // 当前高亮的区域索引
    bool isFlashOn;             // 闪烁状态标记

    void processImage();
    // 颜色距离计算（RGB空间，更稳定）
    int getColorDistance(const cv::Vec3b& a, const cv::Vec3b& b);
    // 基于颜色直方图的区域划分（非过度聚类）
    std::vector<ColorCluster> getColorRegions(const cv::Mat& img);
    // 形态学操作清除小噪点
    cv::Mat removeSmallNoise(const cv::Mat& img);
    // 高亮指定区域
    void highlightRegion(int regionIdx);
};