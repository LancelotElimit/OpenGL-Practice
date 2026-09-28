# OpenGL-Practice 项目完整说明

## 1. 项目定位

这是一个使用 C++ 和 OpenGL 3.3 Core Profile 编写的实时渲染练习项目。

项目从最基础的三角形开始，逐步加入摄像机、模型、阴影、实例化、HDR 后处理、PBR、IBL、glTF 骨骼动画和基础性能优化。目前它已经是一个具备现代实时渲染流程的小型教学渲染器，而不只是单个 OpenGL API 的练习。

当前实际编译的文件是：

```text
src/main_clean.cpp
```

`src/main.cpp` 保留为早期三角形和变换练习记录，CMake 当前不会编译它。

## 2. 当前功能总览

### 窗口和 OpenGL 初始化

- GLFW 创建窗口和 OpenGL Context。
- 请求 OpenGL 3.3 Core Profile。
- GLAD 在 Context 创建后加载现代 OpenGL 函数。
- 启用深度测试和可编程点大小。
- 启动时输出 OpenGL 版本和显卡 Renderer。

### GPU 几何数据

- 使用 VAO 保存顶点属性布局。
- 使用 VBO 保存模型、地面和调试四边形数据。
- 模型顶点格式为：

```text
位置：3 个 float
UV：2 个 float
法线：3 个 float
切线和 handedness：4 个 float
总计：12 个 float/顶点
```

- 模型使用 VAO/VBO/EBO 索引绘制，地面拥有独立的 VAO/VBO。
- 调试阴影图和所有屏幕后处理 Pass 共用一个全屏四边形。

### 着色器

项目中有几组 GLSL 程序：

1. 主渲染程序：处理 Cook–Torrance PBR、纹理、法线、IBL 和阴影。
2. 灯光标记程序：绘制黄色和蓝色的光源圆点。
3. 深度程序：用于聚光灯二维阴影图和点光源立方体阴影图。
4. 调试程序：按住 `F1` 时显示聚光灯深度图。
5. 亮区提取程序：筛选 HDR 场景中亮度超过阈值的像素。
6. 高斯模糊程序：在两个浮点纹理之间交替进行横向和纵向模糊。
7. 最终合成程序：叠加 Bloom、执行 Tone Mapping、Gamma 校正和可选灰度。
8. 程序化环境程序：生成包含高亮太阳的 HDR Environment Cubemap。
9. Irradiance 程序：对环境进行半球卷积，生成漫反射 IBL。
10. Prefilter 程序：按不同 roughness 预过滤环境镜面反射。
11. BRDF Integration 程序：预计算 split-sum BRDF LUT。
12. Skybox 程序：在 HDR 场景中绘制环境背景。

所有程序都会检查 Shader 编译和 Program 链接错误，并将错误信息输出到终端。

### 摄像机

- 第一人称摄像机。
- 鼠标控制 yaw 和 pitch。
- `W/A/S/D` 控制移动。
- 使用 `deltaTime`，移动速度不依赖帧率。
- 鼠标俯仰角限制在 `-89°` 到 `89°`，避免摄像机翻转。
- 使用 `glm::lookAt` 生成 View 矩阵。

### 纹理和材质

- 使用 `stb_image` 从文件加载 PNG 等图像。
- OBJ 基础纹理通过 MTL 的 `map_Kd` 指定，并由 `MaterialLibrary` 统一管理。
- 地面使用真实 Albedo、OpenGL Normal、Roughness 和 AO 贴图组。
- MTL 的 `Kd` 颜色通过 `uMaterialColor` 传入片段着色器。
- 最终反照率大致为：

```text
纹理颜色 × 材质颜色
```

- 法线贴图目前使用一张程序中生成的 2×2 测试纹理。
- 法线贴图通过 TBN 矩阵从切线空间转换到世界空间。
- 模型加载时根据三角形边和 UV 差计算逐顶点切线，并保存 handedness。
- 顶点着色器把切线变换到世界空间，片元着色器再重建 TBN。
- Albedo 纹理按 sRGB 转换回线性空间后参与 PBR 计算。
- Metallic、Roughness、AO 作为 PBR 参数控制材质响应。

### 光照

当前场景包含三类光照：

#### 第一个点光源

- 白色/暖色标记。
- 围绕模型运动。
- 使用 Cook–Torrance BRDF、反平方距离衰减和 PBR 镜面反射。
- 支持点光源立方体阴影。

#### 第二个点光源

- 蓝色光源。
- 从相反方向运动。
- 使用 Cook–Torrance BRDF、反平方距离衰减和 PBR 镜面反射。
- 当前不投射阴影。

#### 摄像机聚光灯

- 位置跟随摄像机。
- 方向跟随摄像机视线。
- 内锥角约为 `12.5°`。
- 外锥角约为 `17.5°`。
- 内外角之间使用平滑过渡。
- 支持二维阴影贴图。

### PBR 与 IBL

直接光照使用 Cook–Torrance 微表面模型：

- GGX/Trowbridge-Reitz 描述微表面法线分布。
- Smith + Schlick-GGX 描述视线和光线的几何遮蔽。
- Schlick Fresnel 描述反射率随观察角度的变化。
- Metallic 在介电材质与金属材质之间变化。
- Roughness 控制高光从锐利镜面到宽阔模糊反射。
- 能量守恒会从漫反射中扣除已经进入镜面反射的能量。

环境光使用 split-sum IBL。程序启动时加载等距柱状 `.hdr` 环境并转换成 Cubemap，然后预计算：

```text
Environment Cubemap
├─ Irradiance Cubemap  → 漫反射环境光
├─ Prefilter Cubemap   → 不同 roughness 的镜面环境反射
└─ BRDF LUT            → NdotV / roughness 的积分结果
```

天空盒直接使用 HDR Environment Cubemap。由于这些资源只依赖环境，昂贵的积分
只在启动时执行一次，正常帧只需要纹理查询。HDR 文件缺失时会自动回退到程序天空。

### 阴影

#### 聚光灯阴影

每帧从聚光灯视角渲染一张深度纹理，然后在正常渲染时将当前片段投影到光源空间，比较当前深度和深度纹理中的最近深度。

使用 3×3 PCF 采样平滑阴影边缘，并使用 bias 减少 Shadow Acne。

#### 点光源阴影

点光源向六个方向发光，因此使用深度立方体纹理：

```text
+X、-X、+Y、-Y、+Z、-Z
```

每帧渲染六个深度面，主渲染时使用 `samplerCube` 查询遮挡关系。目前只有第一个点光源启用了点光源阴影。

### 模型加载

- 使用 `tinyobjloader` 解析 Wavefront OBJ。
- 读取顶点位置、UV、法线和材质编号。
- 使用 `triangulate=true` 将多边形转换为三角形。
- 按 position / UV / normal 组合键去重，并生成 EBO 索引。
- 同一模型的不同材质区间分别绘制。

### 场景实例和层级

当前场景中有三个同源模型实例：

- 中央根节点。
- 左侧子节点。
- 右侧子节点。

子节点保存局部矩阵，世界矩阵通过父子矩阵相乘得到：

```text
WorldChild = WorldParent × LocalChild
```

三个实例共用同一份模型顶点数据、材质和纹理资源，但拥有不同的位移、旋转和缩放。
这些世界矩阵每帧上传到 instance VBO，并占用顶点属性 location 4–7。设置
`glVertexAttribDivisor(..., 1)` 后，GPU 会在实例之间切换矩阵，而不是在普通顶点之间切换。

### glTF 骨骼动画和 Skinning

项目使用 tinygltf 加载 `SimpleSkin.gltf`。它把动画分为两层：

- CPU 根据当前时间采样关键帧，平移/缩放用线性插值，旋转用四元数 Slerp。
- CPU 沿节点层级计算每根 Joint 的全局矩阵，再结合逆绑定矩阵生成 Bone Matrix。
- GPU 顶点着色器读取每个顶点的 `JOINTS_0` 与 `WEIGHTS_0`，混合最多四根骨骼的影响。

核心关系为：

```text
BoneMatrix = inverse(MeshGlobal) × JointGlobal × InverseBindMatrix
SkinnedPosition = Σ(weight[i] × BoneMatrix[joint[i]] × LocalPosition)
```

因此骨骼动画不是每帧重写 VBO。静态顶点、关节编号和权重只上传一次；每帧只更新少量
骨骼矩阵，GPU 再并行计算所有顶点的新位置。

### 视锥剔除和资源缓存

- OBJ 和 glTF 模型加载时计算局部包围球。
- 每帧从 ViewProjection 矩阵提取六个视锥平面。
- 包围球完全位于任一平面外侧时，该实例不会写入本帧 instance VBO。
- `TextureCache` 以规范化文件路径和翻转方式为键，同一图片只创建一个 OpenGL 纹理对象。
- `MaterialLibrary` 与 `PbrMaterial` 引用缓存纹理，不再重复拥有和销毁相同资源。

## 3. 每帧渲染流程

每一帧大致执行以下步骤：

```text
1. 读取键盘和鼠标输入
2. 更新摄像机位置和方向
3. 计算场景节点的世界矩阵
4. 采样 glTF 动画并更新骨骼矩阵
5. 测试实例包围球，只上传视锥内的实例矩阵
6. 渲染聚光灯二维深度图
7. 从第一个点光源的六个方向渲染深度立方体
8. 绑定场景离屏 Framebuffer并清空颜色/深度
9. 使用实例化 Draw Call 绘制可见 OBJ 实例
10. 使用 Cook–Torrance 计算直接光、阴影和 IBL
11. 绘制 PBR 地面与 GPU 蒙皮后的 glTF 模型
12. 绘制 HDR 天空盒和两个灯光标记
13. 如果按住 F1，覆盖显示聚光灯深度图
14. 提取 HDR 亮区并执行十次 Ping-Pong 高斯模糊
15. 合成 Bloom，执行 Tone Mapping、Gamma 校正和可选灰度
16. 交换前后缓冲区
```

## 4. 主要矩阵关系

模型顶点最终经过以下变换：

```text
ClipPosition = Projection × View × Model × LocalPosition
```

其中：

- `Model`：模型在世界中的位置、旋转和缩放。
- `View`：摄像机的位置和朝向。
- `Projection`：透视投影参数。
- `Projection × View × Model`：传给顶点着色器的变换结果。

法线不能直接使用普通 Model 矩阵变换，而是使用：

```text
NormalMatrix = transpose(inverse(Model))
```

这是为了正确处理非均匀缩放。

## 5. 项目文件结构

```text
OpenGL-Practice/
├─ assets/
│  ├─ cube.obj              # OBJ 模型
│  ├─ cube.mtl              # 模型材质和 map_Kd
│  └─ *.png                 # 测试纹理
├─ src/
│  ├─ main_clean.cpp        # 资源准备、输入和主循环
│  ├─ Renderer.cpp          # 阴影、PBR、天空盒和后处理 Pass
│  ├─ EnvironmentIBL.cpp    # 环境 Cubemap 与 IBL 预计算
│  ├─ Mesh.cpp / Model.cpp  # GPU 几何与 OBJ 数据
│  ├─ Material.cpp          # MTL 纹理资源
│  ├─ ShadowMap.cpp         # 二维和 Cubemap 阴影资源
│  ├─ RenderTarget.cpp      # HDR 场景 framebuffer
│  ├─ BlurBuffer.cpp        # Bloom Ping-Pong framebuffer
│  └─ main.cpp              # 早期练习代码
├─ include/                 # 各模块公开接口
├─ shaders/                 # PBR、IBL、阴影和后处理 GLSL
├─ docs/
│  └─ PROJECT_GUIDE.md      # 本文档
├─ CMakeLists.txt           # 依赖和构建配置
├─ README.md                # 项目入口说明
└─ build/                   # 本地生成目录，不提交到 Git
```

## 6. 使用的依赖

| 依赖 | 用途 |
|---|---|
| GLFW 3.4 | 窗口、输入和 OpenGL Context |
| GLAD 2.0.6 | 加载 OpenGL 函数 |
| GLM 1.0.3 | 向量、矩阵和摄像机数学 |
| stb_image | PNG/JPG 等图像加载 |
| tinyobjloader | OBJ 模型解析 |
| tinygltf 2.9.7 | glTF 2.0、Skin 和 Animation 解析 |

依赖由 CMake `FetchContent` 管理，不需要手动复制到项目目录。

首次配置时还会下载测试 PNG 到 `assets/`。

## 7. 环境要求

- Windows
- Visual Studio，包含 MSVC、Windows SDK 和 CMake 工具
- 支持 OpenGL 3.3 的显卡驱动
- 首次配置需要网络

当前机器已验证运行环境：

```text
OpenGL version: 3.3
Renderer: Intel(R) UHD Graphics
```

## 8. 构建和运行

在 Visual Studio Developer Command Prompt 中执行：

```powershell
cmake -S . -B build
cmake --build build --config Debug
.\build\Debug\OpenGLPractice.exe
```

也可以用 Visual Studio 打开 CMake 项目，构建并运行 `OpenGLPractice`。

`build/` 是生成目录，已经被 `.gitignore` 忽略。如果构建配置损坏，可以删除本地 `build/` 后重新执行上面的命令。

## 9. 操作方式

| 操作 | 功能 |
|---|---|
| 鼠标移动 | 旋转摄像机视角 |
| `W` | 向前移动 |
| `S` | 向后移动 |
| `A` | 向左移动 |
| `D` | 向右移动 |
| `ESC` | 退出程序 |
| 按住 `F1` | 显示聚光灯深度图 |
| 按住 `F2` | 显示灰度后处理结果 |
| `F3` | 开启或关闭 Bloom |
| `↑` / `↓` | 调整 HDR 曝光值（0.1–5.0） |
| `Z` / `X` | 降低/提高模型 Metallic（0–1） |
| `C` / `V` | 降低/提高模型 Roughness（0.05–1） |

## 10. 当前限制

- `main_clean.cpp` 已缩减为资源准备和主循环，核心职责已拆分到独立模块。
- 第二个蓝色点光源目前不投射阴影。
- 法线贴图仍是很小的程序内测试纹理，不是从外部 PNG 加载。
- 点光源阴影每帧渲染六个面，性能开销较大。
- 阴影贴图分辨率、投影范围和窗口宽高目前部分使用固定值。
- 当前场景图只是简单的父子节点数组，还不是通用递归场景图。
- 当前只对共享同一 Mesh 和材质分段的模型使用 GPU Instancing。
- OBJ 模型的 Metallic、Roughness 和 AO 仍是标量参数；真实 PBR 贴图组当前应用于地面。
- glTF 示例当前读取第一套网格、Skin 和动画，尚未覆盖完整 glTF 场景与标准 PBR 材质。
- 动画支持 LINEAR/STEP 关键帧采样，尚未加入 CUBICSPLINE、动画切换和混合。
- 蒙皮示例当前不写入两类阴影贴图，因此自身暂不投射动态阴影。
- 视锥剔除使用包围球，并以摄像机视锥筛选共享实例；尚未加入遮挡剔除和空间索引。

## 11. 推荐学习路线

建议接下来按以下顺序继续：

1. 完整导入 glTF 2.0 场景和标准 PBR 材质/纹理。
2. 加入多个动画片段、动画混合和蒙皮阴影 Pass。
3. 使用 GPU Timer、RenderDoc 等工具定位真实性能瓶颈。
4. 扩展为 ECS、异步资源加载和场景资源系统。

## 12. 学习阶段定位

当前项目已经完成 OpenGL 基础 API、传统光照、材质、阴影和基础模型加载，处于：

```text
OpenGL 基础
        ↓
传统实时光照
        ↓
模型/材质/阴影
        ↓
基础场景结构
        ↓
GPU Instancing
        ↓
离屏 Framebuffer / 基础后处理
        ↓
HDR / Tone Mapping / Bloom
        ↓
PBR / IBL 和现代材质流程
        ↓
真实 HDR / PBR 贴图资产管线
        ↓
glTF / 骨骼动画 / 视锥剔除 / 纹理缓存  ← 当前阶段完成
        ↓
完整 glTF 场景 / 动画混合 / ECS  ← 后续扩展
```

因此当前已完成这套学习计划的最后阶段：你已经跨过 OpenGL 基础 API，进入“理解小型渲染器完整数据流”的中级层次。后续不再只是继续堆 API，而是选择资产系统、动画系统、性能分析或引擎架构中的一条路线深入。
