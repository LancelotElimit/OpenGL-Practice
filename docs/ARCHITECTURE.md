# OpenGL-Practice 架构说明

本次整理的目标是把“学习用的单文件渲染器”变成更容易继续扩展的项目，同时保留原有的模型、纹理、光照、阴影和场景树功能。

## 一帧的责任边界

`main_clean.cpp` 只负责组织顶层流程：

```text
读取输入
  ↓
更新 Camera 和 Scene
  ↓
计算场景节点和骨骼的世界矩阵
  ↓
调用 Renderer
  ├─ Frustum Culling
  ├─ 聚光灯 Shadow Pass
  ├─ 点光源 Cubemap Shadow Pass
  ├─ 正常场景 Pass
  └─ 光源标记和调试视图
  ↓
把后处理结果写入 Scene View 纹理
  ↓
Dear ImGui docking 工作区显示场景与模块面板
```

## 模块职责

### ShaderProgram

`ShaderProgram` 拥有一个已经链接的 GLSL Program，负责：

- 从 `shaders/` 读取 GLSL 文本
- 编译顶点和片元 Shader
- 检查编译/链接错误
- 调用 `glUseProgram`
- 查询 uniform 位置
- 在 OpenGL Context 仍然有效时释放 Program

CMake 会在构建后把 `shaders/` 复制到可执行文件旁边；开发时也可以通过资源路径查找器定位项目根目录中的 Shader 文件。

### Camera

`Camera` 保存摄像机位置、前方向、yaw 和 pitch。它把键盘、鼠标输入转换成：

```cpp
glm::mat4 view = camera.viewMatrix();
```

主循环不需要知道鼠标偏移如何计算。

### Mesh

`Mesh` 管理一组交错顶点数据的 VAO/VBO/EBO，并根据属性描述配置：

```text
location 0 → position
location 1 → UV
location 2 → normal
location 3 → tangent.xyz + handedness
location 4–7 → 每个实例的 model matrix 四列
```

OBJ 模型使用 EBO 复用顶点，并通过独立的 instance VBO 保存每个实例的世界矩阵。
`glVertexAttribDivisor(..., 1)` 让 location 4–7 每绘制一个实例才前进一次，
`glDrawElementsInstanced` 因而可以在一次调用中绘制全部同源模型。地面和调试四边形
仍可使用普通 `glDrawArrays`，它们都使用同一个 Mesh 工具。

### Texture2D

`Texture2D` 负责单张 GPU 纹理；`TextureCache` 统一拥有从文件加载的纹理，
`MaterialLibrary` 和 `PbrMaterial` 只保存指向缓存对象的引用：

- 使用 stb_image 读取 RGBA 像素
- 创建 OpenGL 纹理对象
- 设置过滤和重复模式
- 绑定到指定纹理单元
- 用规范化后的“路径 + 翻转方式”作为缓存键
- 同一资源被多个材质引用时只上传一次 GPU
- 按 MTL 的 `map_Kd` 名称查找漫反射纹理
- 找不到材质纹理时回退到默认纹理

Shader 只需要知道 sampler 对应的纹理单元，不需要知道图片是怎么读取的。

### Window

`Window` 负责 GLFW 生命周期、OpenGL 3.3 Context、GLAD 函数加载和 framebuffer 尺寸。窗口缩放时回调会更新 OpenGL viewport，主渲染 Pass 则使用实时宽高比创建投影矩阵。

### Model

`Model` 负责 tinyobjloader 的 OBJ/MTL 解析，把外部文件转换成：

- 交错的 position / UV / normal / tangent 顶点数组
- 由 position / UV / normal 组合键去重后得到的索引数组
- 按材质分组的 `ModelPart`
- MTL 中引用的漫反射纹理名称

`Model` 会遍历三角形，根据边向量和 UV 差计算 tangent、bitangent，
再把正交化后的 tangent 与 handedness 写入顶点。片元着色器由它们重建 TBN，
不再依赖屏幕空间导数临时估算切线方向。

它不创建 VAO，也不负责绘制；GPU 资源由 `Mesh` 和 `Texture2D` 分别管理。

### Scene

`Scene` 保存每帧变化的场景状态。它负责计算父子节点的世界矩阵以及两个移动点光源的位置，但不调用 OpenGL。

### GltfAnimatedModel

`GltfAnimatedModel` 使用 tinygltf 读取 glTF 2.0 的 `POSITION`、`JOINTS_0`、
`WEIGHTS_0`、索引、节点、Skin、逆绑定矩阵和第一个动画片段。每帧执行：

```text
按时间定位相邻关键帧
→ 平移/缩放线性插值，旋转四元数球面插值
→ 沿节点父子关系计算全局矩阵
→ JointGlobal × InverseBind 得到骨骼矩阵
→ 上传 uBones，由顶点着色器按权重混合顶点
```

这一区分了两项工作：CPU 负责动画时间、关键帧和层级；GPU 负责对大量顶点执行
相同的蒙皮公式。示例使用 Khronos `SimpleSkin.gltf`，包含 2 根关节和 1 条旋转通道。

### Frustum

`Frustum` 从 `Projection × View` 提取左、右、上、下、近、远六个平面。
`Renderer` 将模型局部包围球变换到世界空间，只有与六个平面均相交或位于其内的实例
才写入 instance VBO。该优化减少屏幕外物体的正常场景 Draw 工作；它以较保守的
包围球判断换取低 CPU 成本。

### Renderer

`Renderer` 持有主场景、灯光标记、深度、调试、亮区提取、模糊和最终合成七套 Shader，并安排各个渲染 Pass 的执行顺序。一次 `render()` 调用会完成：

```text
聚光灯深度 Pass
→ 点光源六面深度 Pass
→ Cook–Torrance PBR 与 IBL Pass
→ HDR 天空盒
→ 光源标记
→ 可选的 F1 深度调试视图
→ HDR 亮区提取
→ 十次横向/纵向高斯模糊
→ Bloom 合成、Tone Mapping 和 Gamma 校正
```

主循环因此不再需要管理 framebuffer、uniform 或纹理单元。

### EnvironmentIBL

`EnvironmentIBL` 在启动时读取等距柱状 `.hdr` 文件，将它转换为 Cubemap，并持有完整的环境光照资源：

- `256×256` HDR Environment Cubemap
- `32×32` 漫反射 Irradiance Cubemap
- 带 5 个 roughness mip 的 Specular Prefilter Cubemap
- `256×256 RG16F` BRDF Integration LUT
- 用于场景背景的 HDR Skybox

预计算使用独立 capture framebuffer，从六个方向渲染 Cubemap。主 PBR Shader 使用
split-sum 近似，将 Prefilter Map 与 BRDF LUT 组合成镜面 IBL；Irradiance Map 则提供
低频漫反射环境光。如果外部 HDR 缺失，模块会自动改用程序生成天空，保证项目仍可运行。

### PBR 材质

主片元 Shader 使用 Cook–Torrance BRDF：

```text
NDF       → GGX / Trowbridge-Reitz
Geometry  → Smith + Schlick-GGX
Fresnel   → Schlick approximation
Diffuse   → energy-conserving Lambert
```

OBJ 模型使用 MTL Albedo、测试 Normal 和可调 Metallic/Roughness；地面使用一套真实
Albedo、OpenGL Normal、Roughness 与 AO 贴图。模型和地面共享同一 PBR Shader，
通过 `uUsePbrMaps` 在标量参数和完整贴图组之间切换。

### ShadowMap

`ShadowMap2D` 和 `ShadowCubeMap` 分别拥有深度纹理及其 framebuffer，负责：

- 创建和检查 depth-only framebuffer
- 开始某次二维阴影写入或 Cubemap 某一面的写入
- 自动设置阴影贴图 viewport 并清除深度
- 把阴影纹理绑定到指定纹理单元
- 在 OpenGL Context 有效时释放 GPU 资源

`Renderer` 仍然决定何时绘制哪些物体，但不再知道 framebuffer 和纹理对象编号。

### RenderTarget

`RenderTarget` 管理场景和编辑器视图使用的离屏 framebuffer：

- 一张与目标视图同尺寸的 `RGBA16F` HDR 颜色纹理
- 一张独立的 HDR 颜色复制纹理，供水面折射采样（避免读写反馈）
- 一块 `GL_DEPTH24_STENCIL8` renderbuffer
- 一张可采样的深度副本，供软粒子与水面厚度估计
- 窗口尺寸变化时自动销毁并重建附件
- 把最终颜色纹理绑定给全屏后处理 Shader

正常场景不再直接写入窗口。`Renderer` 先把模型、地面、灯光标记和可选调试视图
画进场景 `RenderTarget`，后处理写入另一份视图目标。编辑器把它作为 `Scene View` 的图像显示，最后 Dear ImGui 才绘制到默认 framebuffer。
浮点附件允许光照值超过 1.0，最终由指数 Tone Mapping 根据 exposure 压回显示范围，
再执行 Gamma 校正。`F2` 灰度作用于完成 Tone Mapping 后的最终颜色。

### BlurBuffer

`BlurBuffer` 持有两个与窗口同尺寸的 `RGBA16F` 颜色纹理及 framebuffer。
亮区提取先写入第一个纹理，随后每个模糊 Pass 在两个纹理之间交替读写：

```text
Bright texture A
→ 横向模糊写入 B
→ 纵向模糊写回 A
→ 重复五轮
→ 与原始 HDR 场景合成
```

二维高斯模糊被拆成横向和纵向两个一维 Pass，可显著减少每个像素需要的采样次数。

### AssetPaths

Visual Studio、命令行和直接运行 exe 时，当前工作目录可能不同。`findAssetPath` 会从工作目录和 exe 所在目录向上查找项目的 `assets/`，避免出现“文件明明存在但找不到”的问题。

## 当前边界

模型、材质纹理、阴影资源、场景更新和多遍渲染已经分离，同源模型已经使用
GPU Instancing、HDR/Bloom、Cook–Torrance PBR、天空盒和 split-sum IBL。
真实 HDR/PBR 资产管线、glTF 骨骼动画、视锥剔除和共享纹理缓存已经接入。
静态 glTF 场景路径支持多节点/Primitive 和核心 Metallic-Roughness 材质；蒙皮路径支持多片段选择与渐变，但仍只读取第一个蒙皮 Primitive。两条路径尚未统一，多 Skin、Morph Target、扩展材质和蒙皮阴影仍是后续扩展。CPU 特效粒子有软交界和深度排序；GPU 特效粒子用 Transform Feedback 双缓冲更新并实例化渲染。`Fluid2D` 以速度/密度/压力/散度/障碍纹理执行平流和压力投影；独立 `FluidSystem` 是简化的三维 SPH 与 CPU 等值面实验，水面通过独立颜色/深度副本做屏幕空间折射、吸收、Fresnel 环境反射和边缘效果。默认关闭三维模拟，避免掩盖二维实验的性能。
