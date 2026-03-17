#include "MainWindow.h"
#include <QVBoxLayout>
#include <QFileDialog>
#include <QImage>
#include <QPixmap>
#include <vector>
#include <random>
#include <fstream>
#include <iostream>
#include <cmath>

struct Cluster {
    cv::Vec3d sumColor;         // RGB累加
    double sumBrightness;       // 累加亮度
    int count;
    std::vector<cv::Point> pixels;
    Cluster(const cv::Vec3b& color, double brightness, const cv::Point& p)
        : sumColor(color[0], color[1], color[2]), sumBrightness(brightness), count(1) {
        pixels.push_back(p);
    }
};

// 亮度计算
double brightness(const cv::Vec3b& color) {
    return 0.299 * color[2] + 0.587 * color[1] + 0.114 * color[0];
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    QWidget* central = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(central);

    loadButton = new QPushButton("Load Image", this);
    layout->addWidget(loadButton);

    imageLabel = new QLabel(this);
    imageLabel->setFixedSize(800, 600);
    imageLabel->setScaledContents(true);
    layout->addWidget(imageLabel);

    areaList = new QListWidget(this);
    layout->addWidget(areaList);

    setCentralWidget(central);

    connect(loadButton, &QPushButton::clicked, this, &MainWindow::loadImage);
}

MainWindow::~MainWindow() {}

double MainWindow::colorDistance(const cv::Vec3b& a, const cv::Vec3b& b) {
    int dr = int(a[2]) - int(b[2]);
    int dg = int(a[1]) - int(b[1]);
    int db = int(a[0]) - int(b[0]);
    return std::sqrt(dr * dr + dg * dg + db * db);
}

void MainWindow::loadImage() {
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Open Image",
        "",
        "Images (*.png *.jpg *.bmp)"
    );

    if (fileName.isEmpty()) return;

    // 中文路径读取
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

    // 高斯模糊去噪
    cv::GaussianBlur(img, img, cv::Size(5, 5), 0);

    processImage();
}

void MainWindow::processImage() {
    int rows = img.rows;
    int cols = img.cols;
    resultImg = cv::Mat::zeros(rows, cols, CV_8UC3);

    double colorThreshold = 40.0;       // RGB颜色距离阈值
    double brightnessThreshold = 15.0;  // 亮度差阈值

    std::vector<Cluster> clusters;

    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            cv::Vec3b color = img.at<cv::Vec3b>(y, x);
            double b = brightness(color);
            Cluster* nearest = nullptr;
            double minDist = colorThreshold + 1;

            // 找最近的聚类，如果颜色+亮度均符合阈值
            for (auto& c : clusters) {
                cv::Vec3b avgColor(c.sumColor[0] / c.count,
                    c.sumColor[1] / c.count,
                    c.sumColor[2] / c.count);
                double avgBrightness = c.sumBrightness / c.count;
                double dist = colorDistance(color, avgColor);
                if (dist <= colorThreshold && std::abs(b - avgBrightness) <= brightnessThreshold) {
                    if (dist < minDist) {
                        minDist = dist;
                        nearest = &c;
                    }
                }
            }

            if (nearest) {
                nearest->pixels.push_back(cv::Point(x, y));
                nearest->sumColor += cv::Vec3d(color[0], color[1], color[2]);
                nearest->sumBrightness += b;
                nearest->count++;
            }
            else {
                clusters.push_back(Cluster(color, b, cv::Point(x, y)));
            }
        }
    }

    // 可视化
    std::mt19937 rng(time(0));
    std::uniform_int_distribution<int> dist(50, 255);

    std::vector<int> areas;
    for (auto& c : clusters) {
        cv::Vec3b fillColor(dist(rng), dist(rng), dist(rng));
        for (auto& p : c.pixels) {
            resultImg.at<cv::Vec3b>(p.y, p.x) = fillColor;
        }
        areas.push_back(c.count);
    }

    // 显示可视化图片
    cv::Mat rgb;
    cv::cvtColor(resultImg, rgb, cv::COLOR_BGR2RGB);
    QImage qimg((uchar*)rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
    imageLabel->setPixmap(QPixmap::fromImage(qimg));

    // GUI 列表
    areaList->clear();
    for (size_t i = 0; i < areas.size(); i++)
        areaList->addItem(QString("Region %1: %2 pixels").arg(i + 1).arg(areas[i]));

    // 控制台输出
    std::cout << "Total regions: " << areas.size() << std::endl;
    for (size_t i = 0; i < areas.size(); i++)
        std::cout << "Region " << i + 1 << ": " << areas[i] << " pixels" << std::endl;
}