#include "MainWindow.h"
#include <QVBoxLayout>
#include <QFileDialog>
#include <QImage>
#include <QPixmap>
#include <QCursor>
#include <fstream>
#include <iostream>
#include <cmath>
#include <unordered_map>

// 核心参数（针对目标图片优化）
const int COLOR_DISTANCE_THRESHOLD = 35;  // 适度放宽，减少碎片区
const int MIN_REGION_PIXELS = 500;        // 提高最小像素数，过滤小点点
const int NOISE_FILTER_SIZE = 2;          // 降噪核大小
const int MORPHOLOGY_KERNEL_SIZE = 3;     // 形态学操作核大小
const int FLASH_INTERVAL = 300;           // 闪烁间隔（毫秒）
const int RESET_DELAY = 2000;             // 自动重置高亮时间（毫秒）
const cv::Vec3b HIGHLIGHT_COLOR = cv::Vec3b(0, 255, 255); // 高亮色（黄色）

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), highlightedRegionIdx(-1), isFlashOn(false)
{
    // UI初始化（简洁稳定）
    QWidget* central = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(central);

    loadButton = new QPushButton("Load Image", this);
    layout->addWidget(loadButton);

    imageLabel = new QLabel(this);
    imageLabel->setFixedSize(800, 700);    // 适配图片比例
    imageLabel->setScaledContents(true);
    layout->addWidget(imageLabel);

    areaList = new QListWidget(this);
    layout->addWidget(areaList);

    setCentralWidget(central);

    // 初始化定时器
    flashTimer = new QTimer(this);
    resetTimer = new QTimer(this);
    flashTimer->setInterval(FLASH_INTERVAL);
    resetTimer->setSingleShot(true);

    // 连接信号槽
    connect(loadButton, &QPushButton::clicked, this, &MainWindow::loadImage);
    connect(flashTimer, &QTimer::timeout, this, &MainWindow::flashHighlight);
    connect(resetTimer, &QTimer::timeout, this, &MainWindow::resetHighlight);
}

MainWindow::~MainWindow() {}

// 计算RGB颜色距离（整数版，更快更稳定）
int MainWindow::getColorDistance(const cv::Vec3b& a, const cv::Vec3b& b) {
    int dr = abs(a[2] - b[2]);
    int dg = abs(a[1] - b[1]);
    int db = abs(a[0] - b[0]);
    return dr + dg + db; // 曼哈顿距离，比欧式距离更稳定
}

// 形态学操作清除小噪点（核心去小点点逻辑）
cv::Mat MainWindow::removeSmallNoise(const cv::Mat& img) {
    cv::Mat denoised;
    // 1. 非局部均值去噪（针对彩色图的高级降噪，保留边缘）
    cv::fastNlMeansDenoisingColored(img, denoised, 10, 10, 7, 21);

    // 2. 形态学开运算（先腐蚀后膨胀，清除小亮点/暗点）
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT,
        cv::Size(MORPHOLOGY_KERNEL_SIZE, MORPHOLOGY_KERNEL_SIZE));
    cv::morphologyEx(denoised, denoised, cv::MORPH_OPEN, kernel);

    // 3. 中值滤波（最后一步平滑，去除孤立小点点）
    cv::medianBlur(denoised, denoised, NOISE_FILTER_SIZE * 2 + 1);

    return denoised;
}

// 颜色区域划分（核心：基于直方图，避免乱聚类）
std::vector<ColorCluster> MainWindow::getColorRegions(const cv::Mat& img) {
    std::vector<ColorCluster> clusters;
    std::unordered_map<int, int> colorCount; // 临时统计颜色出现次数

    // 第一步：统计主要颜色（降采样，提升速度+减少噪点干扰）
    int sampleStep = 3; // 加大采样步长，跳过孤立小点点
    for (int y = 0; y < img.rows; y += sampleStep) {
        for (int x = 0; x < img.cols; x += sampleStep) {
            cv::Vec3b color = img.at<cv::Vec3b>(y, x);
            // 将颜色转为整数键（B*65536 + G*256 + R）
            int colorKey = color[0] * 65536 + color[1] * 256 + color[2];
            colorCount[colorKey]++;
        }
    }

    // 第二步：合并相似颜色，生成聚类
    for (const auto& pair : colorCount) {
        if (pair.second < MIN_REGION_PIXELS / 8) continue; // 更严格过滤低频小点点

        // 解析颜色键为RGB
        int colorKey = pair.first;
        cv::Vec3b color(
            colorKey / 65536,
            (colorKey % 65536) / 256,
            colorKey % 256
        );

        // 找相似的聚类中心
        bool merged = false;
        for (auto& cluster : clusters) {
            if (getColorDistance(color, cluster.centerColor) < COLOR_DISTANCE_THRESHOLD) {
                cluster.pixelCount += pair.second * sampleStep * sampleStep;
                merged = true;
                break;
            }
        }

        // 无相似聚类则新建
        if (!merged) {
            clusters.emplace_back(color);
            clusters.back().pixelCount = pair.second * sampleStep * sampleStep;
        }
    }

    // 第三步：更严格过滤过小的聚类（彻底清除小点点对应的聚类）
    std::vector<ColorCluster> validClusters;
    for (auto& cluster : clusters) {
        if (cluster.pixelCount >= MIN_REGION_PIXELS) {
            // 初始化区域掩码
            cluster.mask = cv::Mat::zeros(img.size(), CV_8UC1);
            for (int y = 0; y < img.rows; y++) {
                for (int x = 0; x < img.cols; x++) {
                    if (getColorDistance(img.at<cv::Vec3b>(y, x), cluster.centerColor) < COLOR_DISTANCE_THRESHOLD) {
                        cluster.mask.at<uchar>(y, x) = 255;
                    }
                }
            }
            validClusters.push_back(cluster);
        }
    }

    return validClusters;
}

// 高亮指定区域
void MainWindow::highlightRegion(int regionIdx) {
    if (regionIdx < 0 || regionIdx >= regions.size()) return;

    // 停止之前的定时器
    flashTimer->stop();
    resetTimer->stop();

    highlightedRegionIdx = regionIdx;
    isFlashOn = true;

    // 高亮填充区域
    resultImg = originalResultImg.clone();
    resultImg.setTo(HIGHLIGHT_COLOR, regions[regionIdx].mask);

    // 更新显示
    cv::Mat rgbImg;
    cv::cvtColor(resultImg, rgbImg, cv::COLOR_BGR2RGB);
    QImage qimg((uchar*)rgbImg.data, rgbImg.cols, rgbImg.rows, rgbImg.step, QImage::Format_RGB888);
    imageLabel->setPixmap(QPixmap::fromImage(qimg));

    // 选中列表中对应项
    areaList->setCurrentRow(regionIdx);

    // 启动闪烁和重置定时器
    flashTimer->start();
    resetTimer->start(RESET_DELAY);
}

// 闪烁高亮效果
void MainWindow::flashHighlight() {
    if (highlightedRegionIdx < 0 || highlightedRegionIdx >= regions.size()) return;

    isFlashOn = !isFlashOn;
    resultImg = originalResultImg.clone();

    if (isFlashOn) {
        // 显示高亮色
        resultImg.setTo(HIGHLIGHT_COLOR, regions[highlightedRegionIdx].mask);
    }

    // 更新显示
    cv::Mat rgbImg;
    cv::cvtColor(resultImg, rgbImg, cv::COLOR_BGR2RGB);
    QImage qimg((uchar*)rgbImg.data, rgbImg.cols, rgbImg.rows, rgbImg.step, QImage::Format_RGB888);
    imageLabel->setPixmap(QPixmap::fromImage(qimg));
}

// 重置高亮显示
void MainWindow::resetHighlight() {
    flashTimer->stop();
    highlightedRegionIdx = -1;

    // 恢复原始标记图
    resultImg = originalResultImg.clone();
    cv::Mat rgbImg;
    cv::cvtColor(resultImg, rgbImg, cv::COLOR_BGR2RGB);
    QImage qimg((uchar*)rgbImg.data, rgbImg.cols, rgbImg.rows, rgbImg.step, QImage::Format_RGB888);
    imageLabel->setPixmap(QPixmap::fromImage(qimg));

    // 取消列表选中
    areaList->clearSelection();
}

// 鼠标点击事件处理
void MainWindow::mousePressEvent(QMouseEvent* event) {
    // 只处理左键点击，且图片已加载
    if (event->button() != Qt::LeftButton || regions.empty() || imageLabel->pixmap().isNull()) {
        QMainWindow::mousePressEvent(event);
        return;
    }

    // 获取点击位置在图片上的坐标（转换为原图比例）
    QPoint clickPos = imageLabel->mapFromGlobal(QCursor::pos());
    if (!imageLabel->rect().contains(clickPos)) {
        QMainWindow::mousePressEvent(event);
        return;
    }

    // 计算点击位置对应的原图坐标
    double scaleX = (double)denoisedImg.cols / imageLabel->width();
    double scaleY = (double)denoisedImg.rows / imageLabel->height();
    int x = clickPos.x() * scaleX;
    int y = clickPos.y() * scaleY;

    // 防止越界
    x = std::clamp(x, 0, denoisedImg.cols - 1);
    y = std::clamp(y, 0, denoisedImg.rows - 1);

    // 查找点击位置所属的区域
    cv::Vec3b clickColor = denoisedImg.at<cv::Vec3b>(y, x);
    int targetRegionIdx = -1;
    double minDistance = COLOR_DISTANCE_THRESHOLD + 1;

    for (size_t i = 0; i < regions.size(); i++) {
        double distance = getColorDistance(clickColor, regions[i].centerColor);
        if (distance < minDistance) {
            minDistance = distance;
            targetRegionIdx = i;
        }
    }

    // 高亮对应的区域
    if (targetRegionIdx >= 0) {
        highlightRegion(targetRegionIdx);
    }
}

void MainWindow::loadImage() {
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Open Image",
        "",
        "Images (*.png *.jpg *.bmp)"
    );

    if (fileName.isEmpty()) return;

    // 中文路径兼容读取
    std::ifstream file(fileName.toStdWString(), std::ios::binary);
    if (!file) {
        QMessageBox::critical(this, "Error", "Cannot open file!");
        return;
    }

    std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(file)), {});
    img = cv::imdecode(buffer, cv::IMREAD_COLOR);

    if (img.empty()) {
        QMessageBox::critical(this, "Error", "Failed to decode image!");
        return;
    }

    // 关键：先执行降噪，清除小点点
    denoisedImg = removeSmallNoise(img);

    processImage();
}

void MainWindow::processImage() {
    // 基于降噪后的图片获取颜色区域
    regions = getColorRegions(denoisedImg);
    resultImg = denoisedImg.clone(); // 可视化用降噪后的图，更干净
    originalResultImg = resultImg.clone(); // 保存原始标记图

    // 生成区域标记颜色（固定配色，清晰不杂乱）
    std::vector<cv::Vec3b> markColors = {
        cv::Vec3b(0, 0, 255),    // 红
        cv::Vec3b(0, 255, 0),    // 绿
        cv::Vec3b(255, 0, 0),    // 蓝
        cv::Vec3b(0, 255, 255),  // 黄
        cv::Vec3b(255, 0, 255),  // 紫
        cv::Vec3b(255, 255, 0),  // 青
        cv::Vec3b(128, 0, 128),  // 深紫
        cv::Vec3b(0, 128, 128)   // 深青
    };

    // 标记每个颜色区域的边界（可视化更清晰）
    for (size_t i = 0; i < regions.size(); i++) {
        cv::Mat mask = regions[i].mask;
        cv::Vec3b markColor = markColors[i % markColors.size()];

        // 提取边界并标记
        cv::Mat edges;
        cv::Canny(mask, edges, 100, 200);
        resultImg.setTo(markColor, edges);
    }

    // 保存原始标记图（用于重置高亮）
    originalResultImg = resultImg.clone();

    // 更新UI显示
    // 1. 显示标记后的干净图片
    cv::Mat rgbImg;
    cv::cvtColor(resultImg, rgbImg, cv::COLOR_BGR2RGB);
    QImage qimg((uchar*)rgbImg.data, rgbImg.cols, rgbImg.rows, rgbImg.step, QImage::Format_RGB888);
    imageLabel->setPixmap(QPixmap::fromImage(qimg));

    // 2. 显示区域信息（无小点点干扰）
    areaList->clear();
    std::cout << "Detected color regions (cleaned): " << regions.size() << std::endl;
    for (size_t i = 0; i < regions.size(); i++) {
        ColorCluster& region = regions[i];
        QString info = QString("Region %1 | Color (R:%2, G:%3, B:%4) | Pixels: %5")
            .arg(i + 1)
            .arg((int)region.centerColor[2])
            .arg((int)region.centerColor[1])
            .arg((int)region.centerColor[0])
            .arg(region.pixelCount);
        areaList->addItem(info);
        std::cout << info.toStdString() << std::endl;
    }
}