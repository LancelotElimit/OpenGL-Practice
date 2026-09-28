# OpenGL-Practice

一个使用 C++、OpenGL 3.3、GLFW、GLAD 和 GLM 的实时渲染练习项目。

当前项目已经从“绘制一个三角形”逐步扩展到带 PBR、IBL、阴影、HDR 后处理、glTF 骨骼动画和基础性能优化的小型实时渲染器。

完整说明请参阅：[项目完整文档](docs/PROJECT_GUIDE.md)

## 当前进度

- GLFW 创建窗口和 OpenGL 3.3 Core Context
- GLAD 加载 OpenGL 函数
- VAO、VBO、EBO、索引绘制和顶点属性布局
- 顶点/片段着色器编译与链接错误检查
- 纹理坐标和 PNG 纹理加载
- GLM 的 Model / View / Projection 变换
- 深度测试和立方体渲染
- 第一人称摄像机：鼠标视角、W/A/S/D 移动、ESC 退出
- 窗口缩放时自动更新 framebuffer 视口和透视比例
- Cook–Torrance PBR：GGX、Smith、Schlick Fresnel
- PBR 材质参数：Albedo、Metallic、Roughness、AO、Normal
- 两个彩色点光源及距离衰减
- 跟随摄像机的聚光灯
- 聚光灯二维阴影贴图
- 点光源立方体阴影贴图
- `F1` 阴影深度图调试视图
- 模型加载阶段生成顶点切线和 handedness，使用稳定的 TBN 法线贴图
- `stb_image` 加载 PNG/JPG 等图像
- `tinyobjloader` 加载 OBJ 模型
- OBJ/MTL 材质颜色和 `map_Kd` 材质纹理
- GPU Instancing：实例矩阵缓冲区和实例化索引绘制
- 离屏 Framebuffer、全屏后处理和 `F2` 灰度效果
- `RGBA16F` HDR、曝光 Tone Mapping、Gamma 校正和 Bloom
- 亮区提取与双缓冲 Ping-Pong 高斯模糊
- HDR 环境 Cubemap、天空盒和程序化回退环境
- Irradiance Map、Prefilter Map、BRDF LUT 与 split-sum IBL
- Poly Haven CC0 `.hdr` 环境加载与等距柱状图转 Cubemap
- 真实 Albedo、Normal、Roughness、AO PBR 贴图组和缺失资源回退
- 父节点/子节点场景层级变换
- tinygltf 加载 glTF 2.0 网格、Skin、逆绑定矩阵和动画通道
- CPU 动画采样与节点层级更新、GPU 骨骼蒙皮（Skinning）
- 基于包围球的视锥剔除，屏幕外实例不会提交给正常绘制
- 共享纹理缓存，同一路径的图片只创建一份 GPU 纹理

## 项目结构

```text
OpenGL-Practice/
├─ assets/
│  ├─ cube.obj              # OBJ 模型
│  ├─ cube.mtl              # OBJ 材质
│  ├─ sunset_*.hdr          # CC0 HDR 环境
│  ├─ concrete_*            # CC0 PBR 贴图组
│  └─ SimpleSkin.gltf       # CC0 glTF 骨骼动画示例
├─ src/
│  ├─ main_clean.cpp        # 程序入口、资源准备和主循环
│  ├─ ShaderProgram.cpp     # Shader 编译、链接和 uniform 查询
│  ├─ Camera.cpp            # 第一人称摄像机输入和 View 矩阵
│  ├─ Mesh.cpp              # VAO/VBO/EBO、实例缓冲区和绘制
│  ├─ Model.cpp             # OBJ/MTL 解析、索引去重和切线生成
│  ├─ Material.cpp          # MTL 漫反射纹理的加载、持有和查找
│  ├─ RenderTarget.cpp      # 场景颜色纹理和深度/模板缓冲
│  ├─ BlurBuffer.cpp        # Bloom 的双 HDR 模糊缓冲区
│  ├─ EnvironmentIBL.cpp    # 环境贴图与 IBL 预计算资源
│  ├─ GltfAnimatedModel.cpp # glTF、动画采样和 GPU Skinning
│  ├─ Frustum.cpp           # 视锥平面与包围球测试
│  ├─ TextureCache.cpp      # 共享文件纹理资源
│  ├─ Scene.cpp             # 场景节点、世界矩阵和动态光源
│  ├─ Renderer.cpp          # 阴影与正常场景的多遍渲染
│  ├─ ShadowMap.cpp         # 二维/立方体深度纹理与 framebuffer
│  ├─ Texture2D.cpp         # 2D 纹理加载和绑定
│  ├─ AssetPaths.cpp        # 跨工作目录查找 assets
│  ├─ Window.cpp            # GLFW 窗口、Context 和 GLAD 初始化
│  └─ main.cpp              # 早期三角形练习代码
├─ include/
│  ├─ ShaderProgram.h
│  ├─ Camera.h
│  ├─ Mesh.h
│  ├─ Texture2D.h
│  └─ AssetPaths.h
├─ shaders/
│  ├─ main.vert / main.frag                 # 正常材质和光照
│  ├─ depth.vert / depth.frag               # 阴影深度 Pass
│  ├─ light_marker.vert / light_marker.frag # 光源标记
│  ├─ debug.vert / debug.frag               # 深度图调试视图
│  ├─ postprocess.vert / postprocess.frag   # HDR 合成和 Tone Mapping
│  ├─ bright_pass.frag                       # HDR 亮区提取
│  ├─ blur.frag                              # 可分离高斯模糊
│  ├─ cubemap_capture.vert                   # 环境 Cubemap 捕获
│  ├─ procedural_environment.frag            # 程序化 HDR 天空
│  ├─ irradiance_convolution.frag            # 漫反射 IBL 卷积
│  ├─ prefilter_environment.frag             # 粗糙度预过滤
│  ├─ brdf_lut.vert / brdf_lut.frag          # BRDF 积分查找表
│  ├─ skybox.vert / skybox.frag              # 天空盒显示
│  └─ skinned.vert / skinned.frag            # 骨骼蒙皮示例
├─ CMakeLists.txt
└─ README.md
```

`main_clean.cpp` 现在只负责准备资源和运行“输入、更新、渲染、显示”主循环。原来的 `main.cpp` 保留作为早期练习记录，CMake 不会编译它。

## 模块职责

```text
main_clean.cpp   → 准备资源并运行主循环
ShaderProgram    → 从 shaders/ 读取 GLSL、编译/链接并查询 uniform
Camera           → 处理 WASD/鼠标，生成 View 矩阵
Mesh             → 管理几何/实例缓冲区并发起普通或实例化绘制
Model            → 解析 OBJ/MTL，生成索引顶点、切线和材质分段
MaterialLibrary  → 加载并管理 MTL 引用的漫反射纹理
PbrMaterial      → 加载和绑定 Albedo/Normal/Roughness/AO 贴图组
TextureCache     → 去重并统一持有文件纹理的 GPU 对象
GltfAnimatedModel→ 读取 glTF Skin/动画并执行 GPU 顶点蒙皮
Frustum          → 从 ViewProjection 提取平面并剔除屏幕外包围球
RenderTarget     → 管理可随窗口尺寸变化的离屏场景 framebuffer
BlurBuffer       → 管理 Bloom 使用的两个 Ping-Pong 浮点纹理
EnvironmentIBL   → 生成环境、Irradiance、Prefilter 和 BRDF LUT
Scene            → 更新节点层级、世界矩阵和动态光源
Renderer         → 执行阴影、正常场景、光源标记和调试 Pass
ShadowMap        → 管理二维和立方体阴影贴图的 GPU 资源
Texture2D        → 加载图片、创建纹理、绑定纹理单元
AssetPaths       → 兼容 Visual Studio 和命令行工作目录的资源查找
Window           → 创建 GLFW 窗口、OpenGL Context 和 GLAD 函数加载器
```

## 环境要求

- Windows
- Visual Studio，包含 MSVC、Windows SDK 和 CMake 工具
- 支持 OpenGL 3.3 的显卡驱动
- 首次配置需要网络，用于获取依赖和 Poly Haven CC0 示例资产

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
- 按住 `F2`：对最终画面应用灰度后处理
- `F3`：开启/关闭 Bloom
- `↑/↓`：提高/降低 HDR 曝光值
- `Z/X`：降低/提高模型金属度
- `C/V`：降低/提高模型粗糙度

## 渲染流程概览

每帧大致执行以下步骤：

```text
更新输入和摄像机
        ↓
计算场景节点的世界矩阵
        ↓
采样 glTF 动画并计算骨骼矩阵
        ↓
视锥剔除并更新可见实例缓冲区
        ↓
聚光灯深度 pass
        ↓
点光源六面阴影 pass
        ↓
Cook–Torrance PBR 直接光照
        ↓
Irradiance + Prefilter + BRDF LUT 环境光照
        ↓
绘制 HDR 天空盒
        ↓
绘制灯光标记和调试视图
        ↓
离屏颜色纹理经过全屏后处理
        ↓
提取亮区并执行 Ping-Pong 高斯模糊
        ↓
合成 Bloom，执行曝光 Tone Mapping 和 Gamma 校正
        ↓
交换前后缓冲区
```

## 依赖说明

- GLFW：窗口、输入和 OpenGL Context
- GLAD：加载现代 OpenGL 函数
- GLM：向量、矩阵和摄像机数学
- stb_image：图像文件加载
- tinyobjloader：OBJ 模型解析
- tinygltf：glTF 2.0、Skin 和 Animation 数据解析

依赖由 CMake `FetchContent` 管理，不需要手动复制到项目目录。
