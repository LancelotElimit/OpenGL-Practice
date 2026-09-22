# OpenGL-Practice

一个使用 C++、OpenGL 3.3、GLFW、GLAD 和 GLM 的实时渲染练习项目。

当前项目已经从“绘制一个三角形”逐步扩展到带模型、材质、光照和阴影的基础渲染器。

## 当前进度

- GLFW 创建窗口和 OpenGL 3.3 Core Context
- GLAD 加载 OpenGL 函数
- VAO、VBO 和顶点属性布局
- 顶点/片段着色器编译与链接错误检查
- 纹理坐标和 PNG 纹理加载
- GLM 的 Model / View / Projection 变换
- 深度测试和立方体渲染
- 第一人称摄像机：鼠标视角、W/A/S/D 移动、ESC 退出
- 顶点法线、漫反射和 Phong 高光
- 材质参数：高光强度和光泽度
- 两个彩色点光源及距离衰减
- 跟随摄像机的聚光灯
- 聚光灯二维阴影贴图
- 点光源立方体阴影贴图
- `F1` 阴影深度图调试视图
- 法线贴图和 TBN 变换
- `stb_image` 加载 PNG/JPG 等图像
- `tinyobjloader` 加载 OBJ 模型
- OBJ/MTL 材质颜色和 `map_Kd` 材质纹理
- 一个模型的多个实例
- 父节点/子节点场景层级变换

## 项目结构

```text
OpenGL-Practice/
├─ assets/
│  ├─ cube.obj              # OBJ 模型
│  ├─ cube.mtl              # OBJ 材质
│  └─ *.png                 # 测试纹理
├─ src/
│  ├─ main_clean.cpp        # 当前实际编译的主程序
│  └─ main.cpp              # 早期三角形练习代码
├─ CMakeLists.txt
└─ README.md
```

`main_clean.cpp` 是当前主程序。原来的 `main.cpp` 保留作为早期练习记录，CMake 不会编译它。

## 环境要求

- Windows
- Visual Studio，包含 MSVC、Windows SDK 和 CMake 工具
- 支持 OpenGL 3.3 的显卡驱动
- 首次配置需要网络，用于获取 GLFW、GLM、GLAD、stb_image、tinyobjloader 和测试纹理

## 构建和运行

在 Visual Studio Developer Command Prompt 中执行：

```powershell
cmake -S . -B build
cmake --build build --config Debug
.\build\Debug\OpenGLPractice.exe
```

也可以直接使用 Visual Studio 打开 CMake 项目，构建并运行 `OpenGLPractice`。

## 操作方式

- 鼠标：旋转摄像机视角
- `W/A/S/D`：移动摄像机
- `ESC`：退出程序
- 按住 `F1`：显示聚光灯阴影深度图

## 渲染流程概览

每帧大致执行以下步骤：

```text
更新输入和摄像机
        ↓
计算场景节点的世界矩阵
        ↓
聚光灯深度 pass
        ↓
点光源六面阴影 pass
        ↓
正常场景渲染
        ↓
材质、纹理、法线和光照计算
        ↓
绘制灯光标记和调试视图
        ↓
交换前后缓冲区
```

## 依赖说明

- GLFW：窗口、输入和 OpenGL Context
- GLAD：加载现代 OpenGL 函数
- GLM：向量、矩阵和摄像机数学
- stb_image：图像文件加载
- tinyobjloader：OBJ 模型解析

依赖由 CMake `FetchContent` 管理，不需要手动复制到项目目录。
