# Color Region Segmentation Tool

基于 OpenCV 和 Qt 开发的图像颜色区域分割与交互工具，支持自动识别图像中的颜色区域、边缘标记、鼠标点击高亮及闪烁提示。

---

## 📌 功能特性

- **图像加载**：支持 PNG、JPG、BMP 格式
- **颜色区域聚类**：基于颜色相似度自动划分区域
- **边缘轮廓标记**：为每个区域绘制不同颜色的边缘线
- **鼠标交互**：点击图像任意位置，自动高亮所属颜色区域
- **高亮闪烁**：选中区域以黄色闪烁提示
- **区域列表**：左侧列表显示各区域的颜色信息与像素数量
- **自动重置**：高亮显示 2 秒后自动恢复

---

## 🛠️ 开发环境

| 工具          | 版本        |
| ------------- | ----------- |
| Visual Studio | 2019 / 2022 |
| Qt            | 5.x / 6.x   |
| OpenCV        | 4.x         |

---

## 📦 OpenCV 配置

### 1. 下载 OpenCV
前往 [OpenCV Releases](https://opencv.org/releases/) 下载 Windows 版本，解压到：
```
D:\opencv\
```

### 2. Visual Studio 项目配置

#### 包含目录
```
D:\opencv\build\include
```

#### 库目录
```
D:\opencv\build\x64\vc16\lib
```
> `vc16` 对应 Visual Studio 2019/2022

#### 附加依赖项（链接器 → 输入）
根据 OpenCV 版本添加，例如：
```
opencv_world480d.lib
```
> 使用 `d` 后缀版本用于 Debug 模式

### 3. 运行时 DLL
将以下路径添加到系统 `PATH` 环境变量，或复制所需 DLL 到 exe 所在目录：
```
D:\opencv\build\x64\vc16\bin
```

---

## 🧩 Qt 配置

- 使用 **Qt VS Tools** 插件进行 Visual Studio 中的 Qt 项目配置
- 确保 Qt 的 `include` 和 `lib` 路径正确指向 Qt 安装目录
- 本项目使用 Qt 的 `QMainWindow`、`QLabel`、`QListWidget` 等控件

---

## 🚀 编译与运行

1. 使用 Visual Studio 打开项目解决方案（`.sln`）
2. 选择 `x64` 平台（OpenCV 需匹配）
3. 确保 OpenCV 和 Qt 的包含目录、库目录已正确配置
4. 编译运行

---

## 🖱️ 使用说明

| 操作               | 说明                                         |
| ------------------ | -------------------------------------------- |
| 点击「Load Image」 | 选择图片文件并加载                           |
| 鼠标左键点击图像   | 高亮该位置所属的颜色区域                     |
| 区域列表           | 显示各区域中心颜色和像素数量，点击可选中区域 |

---

## 📁 项目结构

```
├── MainWindow.h          // 主窗口类声明
├── MainWindow.cpp        // 主窗口类实现
├── main.cpp              // 程序入口
├── README.md             // 项目说明
```

---

## ⚙️ 核心参数说明

| 参数                       | 值     | 说明                         |
| -------------------------- | ------ | ---------------------------- |
| `COLOR_DISTANCE_THRESHOLD` | 35     | 颜色距离阈值（曼哈顿距离）   |
| `MIN_REGION_PIXELS`        | 500    | 最小区域像素数（过滤小区域） |
| `DOWNSAMPLE_SCALE`         | 4      | 降采样比例（提升处理速度）   |
| `FLASH_INTERVAL`           | 300ms  | 高亮闪烁间隔                 |
| `RESET_DELAY`              | 2000ms | 高亮自动重置时间             |

---

## 📝 注意事项

- 图像处理过程中会对图像进行降采样以提升性能，实际区域判断基于原始图像
- 颜色聚类采用曼哈顿距离，阈值可根据图像特点调整
- 若区域数量过多，可适当提高 `MIN_REGION_PIXELS` 过滤小区域

---

## 📄 License

本项目仅供学习交流使用。
