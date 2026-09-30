# OpenGL-Practice 项目完整说明

## 1. 项目定位

这是一个使用 C++ 和 OpenGL 3.3 Core Profile 编写的实时渲染练习项目。

项目从最基础的三角形开始，逐步加入摄像机、模型、阴影、实例化、HDR 后处理、PBR、IBL、glTF 骨骼动画和基础性能优化。目前它已经是一个具备现代实时渲染流程的小型教学渲染器，而不只是单个 OpenGL API 的练习。

当前实际编译的文件是：

```text
src/EditorMain.cpp → Project → EngineApplication
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

### 第二阶段：静态 glTF 场景、材质与动画控制

`GltfScene` 遍历当前默认 Scene 的节点层级，分别上传每个三角形 Primitive，并保留其节点变换和材质。它加载 `.gltf`/`.glb`、嵌入或外部图像，使用 glTF 核心 Metallic-Roughness 通道约定：粗糙度取 G、金属度取 B、AO 取 R；Base Color/Emissive 贴图按 sRGB 转线性。OPAQUE、MASK、BLEND 分开处理，BLEND 按 Primitive 中心排序，绘制时关闭深度写入。面板允许加载用户路径并显示加载结果。Khronos `BoxTextured.glb` 是默认演示资产。

`GltfAnimatedModel` 仍负责 SimpleSkin 蒙皮示例，现在读取所有动画片段；面板可以选择片段、暂停或改变速度。片段切换时用约 0.3 秒对当前姿态和新片段姿态做渐变，支持 LINEAR、STEP、CUBICSPLINE 采样。静态场景导入和蒙皮示例尚未统一成完整通用 glTF 运行时：复杂资产可能需要 Morph Target、多 Skin、多 UV、压缩扩展或材质扩展，当前不承诺全部支持。

### 第三阶段：基础粒子系统

`ParticleSystem` 使用固定容量的 CPU 粒子集合，更新年龄、速度、重力和位置；根据生命周期插值大小及透明度。Sparks、Smoke、Snow 是可切换预设。上传每粒子的位置、大小和颜色后，GPU 以一次实例化绘制生成面向摄像机的四边形。透明粒子按摄像机距离排序，深度测试保留、深度写入关闭。场景深度在绘制粒子前复制到只读深度纹理，片段着色器根据粒子与几何的深度差淡化交界，避免明显硬切。面板提供发射率、寿命、速度、重力、大小、软交界开关和存活粒子数。它是点精灵/烟雾/火花的教学基础，不等同于流体模拟或真实流体表面渲染。

### 第四阶段：Transform Feedback GPU 粒子

`GpuParticleSystem` 与 CPU `ParticleSystem` 并存。前者创建两份同布局 GPU Buffer：更新着色器读取当前位置、速度、年龄、寿命和随机种子；OpenGL 3.3 的 Transform Feedback 把新的状态直接写到另一份 Buffer，下一帧交换输入/输出。CPU 只设置 `deltaTime`、发射率、重力等 uniform，不逐粒子修改 VBO 或读回位置。Billboard 渲染从当前粒子 Buffer 读取实例属性，最多 32768 槽位。面板支持暂停、重置、预设、容量、发射率、寿命、重力与尺寸。大量特效粒子当前使用加法混合，无排序；这不是复杂半透明烟雾的正确 Alpha 合成。其更新路径符合 [OpenGL 3.3 Transform Feedback 规范](https://registry.khronos.org/OpenGL/specs/gl/glspec33.core.pdf)，无需 Compute Shader。

### 第五阶段：2D 烟雾流体

`Fluid2D` 使用 192×192 离屏纹理保存 Velocity、Density、Pressure、Divergence 与 Obstacle；速度和密度各有 Ping-Pong 缓冲。一次固定步更新依次执行半拉格朗日平流、自动/鼠标注入、速度散度、Jacobi 压力迭代和压力梯度扣除，再把密度映射到画布。障碍纹理包含边界与可选中心圆盘；右键能涂抹额外障碍，求解和显示都会采样它。可停靠的 `2D Smoke Lab` 可切换五种场，暂停、重置和调参。实现参考 [GPU Gems: Fast Fluid Dynamics Simulation on the GPU](https://developer.nvidia.com/gpugems/gpugems/part-vi-beyond-triangles/chapter-38-fast-fluid-dynamics-simulation-gpu)，采用简化边界处理与固定分辨率，尚无涡量约束、温度/浮力或体积渲染。

### 第六阶段：粒子流体、连续表面与水面材质

`FluidSystem` 是独立于火花/烟雾粒子的三维教学原型。它用 1/120 秒固定步长更新最多 256 个水粒子；密度使用紧支撑核函数，压力取当前密度超出初始参考密度的部分，另加黏性、重力和长方体边界反弹。这是受 [Müller、Charypar、Gross 的 SPH 流体工作](https://diglib.eg.org/items/fb9edf26-94b0-4302-8cfc-52632841cae7)启发的简化弱可压缩模型，包含稳定性钳制，不是完整 Navier–Stokes 求解器。

显示流程：每粒子只对附近体素累积紧支撑标量核及其梯度；`marching tetrahedra` 从等值面生成三角形，梯度提供水面法线。不透明场景先复制到独立 HDR 颜色纹理与深度纹理，水面 Pass 再采样它们，不会一边写场景纹理一边读同一纹理。法线偏移屏幕坐标形成近似折射；深度差用于光吸收和浅水边缘；Fresnel 权重混合环境 Cubemap 反射。水面最终写不透明深度，以正确遮挡后续粒子。

从编辑器的 `Window` 菜单打开 `3D Water Lab`：`Focus water` 聚焦水体，`Surface / Particles / Wireframe` 比较模拟点、网格与拓扑；暂停时可 `Step once`。`Water / Refraction / Fresnel / Normals` 分离观察着色组成，多个材质滑块可实时调节。面板显示粒子、三角形、模拟和网格重建耗时；`Surface rebuilds / second` 可降低 Debug 模式下的 CPU 开销，也可直接暂停 2D 烟雾做单独对比。默认 125 粒子，水体默认隐藏。这里选择了原第六阶段的 SPH 粒子水与水面渲染方向；体积烟火、真实光线追踪折射、表面张力、任意网格障碍碰撞和 GPU 求解仍未实现。

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
11. 绘制 PBR 地面、静态 glTF 的不透明 Primitive 与蒙皮示例
12. 绘制 HDR 天空盒，再绘制透明 glTF Primitive
13. 可选绘制连续流体等值面，或切换为原始点/线框调试
14. 复制场景深度，绘制 CPU 软粒子与 GPU 实例化粒子
15. 如果按住 F1，覆盖显示聚光灯深度图
16. 提取 HDR 亮区并执行十次 Ping-Pong 高斯模糊
17. 合成 Bloom，执行 Tone Mapping、Gamma 校正和可选灰度
18. 绘制调试面板，交换前后缓冲区
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
│  ├─ EditorMain.cpp        # 独立项目入口
│  ├─ EngineApplication.cpp # 通用资源准备、输入和主循环
│  ├─ Project.cpp           # 项目和场景读写
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
| 场景视图内按住右键并移动鼠标 | 旋转摄像机视角 |
| `W` | 向前移动 |
| `S` | 向后移动 |
| `A` | 向左移动 |
| `D` | 向右移动 |
| `ESC` | 退出程序 |
| 按住 `F1` | 显示聚光灯深度图 |
| 按住 `F2` | 显示灰度后处理结果 |
| `F3` | 开启或关闭 Bloom |
| `F4` | 切换编辑器鼠标模式与全窗口自由摄像机模式 |
| `↑` / `↓` | 调整 HDR 曝光值（0.1–5.0） |
| `Z` / `X` | 降低/提高模型 Metallic（0–1） |
| `C` / `V` | 降低/提高模型 Roughness（0.05–1） |

## 10. 当前限制

- `EditorMain.cpp` 打开项目，`EngineApplication.cpp` 执行通用主循环；引擎单独编译为 `LancelotEngine` 库，示例内容来自项目文件。
- 第二个蓝色点光源目前不投射阴影。
- 法线贴图仍是很小的程序内测试纹理，不是从外部 PNG 加载。
- 点光源阴影每帧渲染六个面，性能开销较大。
- 阴影贴图分辨率、投影范围和窗口宽高目前部分使用固定值。
- 当前场景图只是简单的父子节点数组，还不是通用递归场景图。
- 编辑器是可停靠工作区原型：已有模型与 CPU/GPU 粒子发射器目录、点击选择、Q/W/E Gizmo 和复制删除，但尚无撤销/重做、场景序列化、可编辑父子层级或通用 ECS。地面和流体未纳入通用对象操作。
- 当前只对共享同一 Mesh 和材质分段的模型使用 GPU Instancing。
- OBJ 模型的 Metallic、Roughness 和 AO 仍是标量参数；真实 PBR 贴图组当前应用于地面。
- 静态 glTF 场景已覆盖核心 PBR 材质，但目前只支持基础三角形网格和第一套 UV；不处理 Draco/Meshopt、Morph Target、材质扩展和复杂纹理变换。静态 glTF 不驱动 Skin 动画。
- 蒙皮演示仍只读取第一个蒙皮网格 Primitive；动画片段可切换并渐变，但不是多片段权重叠加混合。
- CPU 特效粒子与 Transform Feedback GPU 特效粒子是两条独立实现；GPU 粒子采用加法混合，未做透明排序。2D 烟雾模拟只处理简化平流和压力投影，没有涡量约束、温度或浮力。
- 独立的三维水体原型已有简化 SPH、边界碰撞、等值面网格和屏幕空间近似折射；但尚未实现表面张力、复杂障碍碰撞、物理折射及 GPU 计算，默认隐藏。
- 蒙皮示例当前不写入两类阴影贴图，因此自身暂不投射动态阴影。
- 视锥剔除使用包围球，并以摄像机视锥筛选共享实例；尚未加入遮挡剔除和空间索引。

Dear ImGui docking 工作区默认是左侧 `Hierarchy`、中央 `Scene View`、右侧 `Inspector`，底部 `Resource Browser / Output / Profiler / 2D Smoke Lab`。面板可拖动、停靠、关闭和重开，布局保存在本机。场景先写入离屏纹理，再在 Scene View 上叠加 ImGuizmo 工具。点击静态模型时先做包围球粗检测，再在导入模型坐标中做射线/三角形求交；射线方向变换后不归一化，以保留世界距离并正确比较非均匀缩放实例。蒙皮演示暂用绑定姿势包围球。

`SceneObject` 是可克隆的多态基类，保存运行期稳定 ID、名称、类型、可见性和 TRS；模型、平台、模拟对象、灯光、相机和环境使用各自的派生类。资源导入矩阵负责居中与尺寸归一化，编辑矩阵按 `T * Rz * Ry * Rx * S` 组合，最终模型矩阵为 `editor * import`。点击对象或目录后按 Q 移动、W 旋转、E 缩放；工具与 Inspector 数值互相同步。单轴缩放使用局部轴；移动和旋转可切换世界/局部模式，并设置吸附。Ctrl+D 复制，Delete 删除实例；右键菜单和按钮提供相同操作。复制共享几何资源但具有独立变换，目录修改不会删除磁盘资源。Ctrl+S 将对象变换和组件参数写入当前项目场景；模拟的动态粒子位置和流体纹理不持久化。

Resource Browser 是独立可停靠窗口，支持目录进入/返回/根目录、文件名筛选、路径显示、glTF 加载及添加已加载模型的实例。当前仅保留一份静态 glTF 共享资源，重新加载会替换所有同类实例的几何与材质；不同 glTF 文件不能同时驻留。右键拖动 Scene View + WASD 用于相机导航，此时 W 不切换 Gizmo；F 聚焦选中实例，F4 切换旧式全窗口自由摄像机。Profiler 统计不包含编辑器自身绘制。OBJ 阴影 Pass 保留全部可见实例，颜色 Pass 才使用摄像机视锥剔除。蒙皮和静态 glTF 沿用其原有专用绘制路径。

## 11. 推荐学习路线

粒子发射器也属于 `SceneObject`：数据组件只保存参数，Renderer 以稳定 ID 管理独立 CPU 粒子池或 GPU Transform Feedback 缓冲区。目录复制产生新 ID 与参数副本，下次渲染创建空的模拟状态；目录删除后 Renderer 回收对应运行时资源。粒子模拟以局部原点为发射源，CPU 绘制前变换位置，GPU 顶点着色器使用 `uEmitterTransform`，因此移动不需要重置或回读 GPU 粒子。旋转也作用于局部重力；粒子片仍保持 Billboard 朝向。场景原点标记用于选择发射器，而不是把每个动态粒子当成独立可编辑对象。

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
glTF / 骨骼动画 / 视锥剔除 / 纹理缓存
        ↓
静态 glTF 多材质 / CPU 粒子 / 调试 UI
        ↓
Transform Feedback GPU 粒子 / 2D 烟雾流体  ← 第四、五阶段已实现
        ↓
三维 SPH / 水面重建 / 近似折射与反射  ← 第六阶段选定方向
        ↓
更完整的资产管线 / 编辑器 / GPU 流体  ← 后续扩展
```

这套路线的第四、五阶段已做出可交互原型，但不等于生产级引擎：GPU 粒子仍是加法混合特效，二维流体的边界与物理模型经过简化，复杂资产和完整编辑器仍需继续建设。当前重点是理解并验证 CPU、GPU 与离屏纹理之间的数据流。
