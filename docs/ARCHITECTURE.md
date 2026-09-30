# OpenGL-Practice 架构说明

本次整理的目标是把“学习用的单文件渲染器”变成更容易继续扩展的项目，同时保留原有的模型、纹理、光照、阴影和场景树功能。

## 第一阶段：引擎与编辑器边界（已完成）

第一阶段拆分 Engine/Editor，后续第二、三、四阶段已接入，见下文。本轮不增加光照公式，也不改为完整 ECS。

### 编译依赖

```text
LancelotPlayer ──→ LancelotEngine + LancelotProjectScripts（无编辑器）
OpenGLPractice（编辑器启动程序）
├─ LancelotEditor ──→ LancelotEngine
│  ├─ ImGui / ImGuizmo
│  └─ Windows IME、系统项目选择窗口与媒体预览
└─ LancelotProjectScripts ──→ LancelotEngine
   └─ projects/Sandbox/Scripts

LancelotEngine ──→ GLFW / GLAD / GLM / OpenGL / 资产加载器（含 Assimp）
                 不依赖 ImGui、ImGuizmo 或编辑器头文件
```

引擎和项目脚本不依赖编辑器；编辑器依赖引擎。项目数据通过
`.lancelot` 和 `scene.json` 提供，不把示例对象硬编码进引擎。
项目脚本仍为静态编译注册，不是动态模块或热重载系统。

### 目录与责任

| 位置 | 责任 |
| --- | --- |
| `include/`、`src/` | 引擎接口与实现：场景、相机、资源、模拟、渲染、窗口 |
| `editor/include/`、`editor/src/` | 编辑器宿主、工作区、面板、输入分发与 SceneHistory |
| `projects/Sandbox/Scripts/` | 项目行为脚本 |
| `projects/*/*.lancelot`、`scene.json` | 项目配置与场景数据 |
| `shaders/` | 引擎使用的 GLSL |
| `tests/` | 场景、项目、运行生命周期与依赖边界测试 |

`EngineApplication` 是可复用的引擎会话：初始化资源，拥有窗口、场景、相机和
Renderer，并提供渲染入口。它不创建 UI，也不接管宿主主循环。
`EditorApplication` 才负责每帧组织、编辑/运行切换和最终 UI 呈现。
引擎会话通过 RAII 回收资源，GPU 资源在 OpenGL Context 销毁前释放；
编辑器 UI 与运行会话先于引擎销毁。

### 一帧的宿主流程

```text
FrameClock：真实帧间隔
    ↓
应用上一帧工具栏命令（避免绘制 UI 时替换活动场景）
    ↓
InputSystem：采样 GLFW 键鼠状态
    ↓
EditorInputRouter：处理全局快捷键与编辑/游戏输入分流
    ↓
EditorWorkspace.beginFrame：视图尺寸与输入焦点
    ↓
PlaySession：更新脚本，选择编辑场景或运行场景
    ↓
Scene 世界矩阵 → EngineApplication.update → SimulationRuntime
    ↓
EngineApplication.render → Renderer（只提交绘制）
    ↓
EditorWorkspace：组合各面板 → 交换窗口缓冲 → 处理事件
```

`FrameClock` 区分真实帧间隔和模拟步长上限：FPS 采用真实时间，模拟步长
限制在 0～50ms，避免长帧造成过大的更新。该工具不是固定步物理调度器。

`InputSystem` 只采样平台状态，不决定哪个对象接收输入。
`EditorInputRouter` 根据编辑/运行模式、文本输入和 Scene View 焦点分发输入；
运行时使用 ImGui 键事件补充短按，减少只采样按住状态造成的漏键。
`Camera` 接收移动轴和指针坐标，不再自己查询 GLFW 键盘。

### 面板拆分

`EditorWorkspace` 保留字体、主题、Docking、菜单、Scene View 和共享选择状态，
不再把所有窗口实现放进同一个大文件：

```text
EditorWorkspace
├─ HierarchyPanel：对象目录
├─ InspectorPanel：对象属性与脚本参数
├─ ResourceBrowserPanel：目录、筛选、选择与导入入口
├─ AssetPreviewPanel / MediaPreview：图片和系统音视频预览
├─ OutputPanel：运行日志
├─ ProfilerPanel：统计与帧时间历史
├─ SmokePanel：选定烟雾对象的实验交互
├─ WaterPanel：选定水对象的实验交互
├─ SelectionController：拾取、快捷键与变换工具
└─ ProjectDialog：Windows 项目文件 / 新建父目录选择
```

面板各自保存窗口开关、浏览器目录/筛选、性能历史或模拟目标 ID。
共享选择与 Gizmo 状态仍由工作区协调；这是职责拆分，
并非已经实现独立服务、组件化场景或编辑器插件接口。

### 构建与验证

默认 `LANCELOT_BUILD_EDITOR=ON` 构建完整编辑器。只构建引擎时：

```powershell
cmake -S . -B out/build/engine-only -G Ninja -DCMAKE_BUILD_TYPE=Debug -DLANCELOT_BUILD_EDITOR=OFF -DLANCELOT_FETCH_SAMPLE_ASSETS=OFF
cmake --build out/build/engine-only
ctest --test-dir out/build/engine-only --output-on-failure
```

在已配置 Visual Studio C++ 工具链的终端运行；关闭示例资产下载不关闭基础依赖下载。
纯引擎配置不会创建或下载 ImGui/ImGuizmo 目标，仍构建项目脚本库、引擎测试和 LancelotPlayer；尚无安装包或自动打包流水线。

本轮验证：编辑器完整构建与 7 项测试通过；纯引擎/Player 独立构建与 4 项测试通过。
以上为该架构拆分阶段的历史验证。后续多格式、媒体与项目创建功能加入后，
当前编辑器配置定义 11 项测试，纯引擎配置定义 6 项；最近完整 Debug 验证为 11 项通过。
其中 AssetsRuntimeTests 使用隐藏的真实 OpenGL Context，验证不同 OBJ/glTF 同时导入、
资源复用、独立绑定、实际帧缓冲内容、暂停与重复绘制不推进模拟，以及 Player 呈现无 GL 错误。
桌面冒烟检查通过 Play/WASD/Pause/Step/Stop、位置修改的撤销/重做及拖放改父节点；测试编辑已撤销，未保存场景。资源导入/绑定的鼠标流程仍建议按 [编辑工作流](EDITOR_WORKFLOW.md) 补充检查。

## 第二阶段：组件与资源边界（已接入）

- TransformComponent：局部 TRS；ScriptBinding：脚本参数；ModelComponent：资源引用、动画和 OBJ 材质覆盖。
- 保留浅继承，模拟、灯光、环境等继续组合数据设置；不是任意可插拔 ECS。
- AssetReference 使用相对项目 assetRoot 的路径（可引用外部绝对路径），不是 GPU 编号，也不是自动追踪移动的 GUID。
- AssetLibrary 按规范化路径缓存多个 OBJ、静态 glTF；OBJ 使用引擎会话共享 TextureCache。
  glTF 内部纹理仍由对应 glTF 资源拥有，并未统一为全局去重纹理服务。
- 资源浏览器导入创建新对象，不替换所有同类对象；可绑定选中对象，也可拖入场景或 Inspector。
- 每个蒙皮对象独立保存片段/速度/播放设置，运行实例独立；当前蒙皮加载器仍只支持一个蒙皮 Primitive。
- 缓存保留到项目关闭；删除对象不删除资源文件。文件重载、异步导入、缓存淘汰仍未实现。

## 第三阶段：更新与绘制分离（已接入）

```text
EngineApplication（拥有 Context 和资源生命周期）
├─ Scene：可保存的数据，派生世界矩阵
├─ AssetLibrary：导入与 GPU 模型缓存
├─ SimulationRuntime：ID → 粒子 / 水 / 烟雾 / 蒙皮运行实例
└─ Renderer：借用上述资源，提交阴影、颜色、透明与后处理 Pass

宿主：脚本 → 世界矩阵 → 模拟/动画 → 绘制 → UI/呈现
```

Renderer::render 不更新粒子、流体或动画。EngineApplication::update 显式决定是否推进；
暂停仍同步对象存活状态，但不积分。新/删除的模拟对象在同步阶段创建/回收独立实例。
开始、停止和历史恢复会重置模拟状态，避免已恢复的 ID 误用旧缓冲。
模拟类仍同时含更新与绘图资源，这是 OpenGL 教学实现的内部结构；
并未将每个模拟器继续拆成纯数学求解器与独立 GPU 呈现器。
每个水/烟雾求解器保留自己的固定步逻辑，尚无统一物理调度或多线程任务图。

## 第四阶段：编辑工作流与运行宿主（已接入）

- 父子层级：目录树、拖放/Inspector 改父节点；默认保持世界 TRS。
  拒绝循环和缺失父节点；无法由 TRS 表示的剪切会拒绝，避免偷偷改坏模型。
- 世界 Gizmo 写回父节点下的局部 TRS；父节点的变换/可见性传给子节点。
  复制/删除针对整个子树，复制重映射 ID 并取消主角资格。
- SceneHistory：最多 64 步作者数据快照；连续拖动/输入在结束时合为一条，
  Ctrl+Z 撤销、Ctrl+Y 或 Ctrl+Shift+Z 重做。新编辑清空重做分支。
  不撤销相机导航、窗口布局、全局渲染选项或实时模拟缓冲。
- 持久化：保存稳定 ID、父节点、模型资源与组件设置；兼容不含新字段的 v1 场景。
  每个文件先写临时文件再替换，并保留 .bak；场景和描述文件不是跨文件原子事务。
- RuntimeSession 位于引擎，无 ImGui；编辑器 PlaySession 只是兼容别名。
  暂停工具栏 Step 推进 1/60 秒并保持暂停；Stop 丢弃运行副本，不写回作者场景。
- LancelotPlayer 是独立运行宿主：打开 .lancelot、执行已编译项目脚本、WASD 控制，
  Esc 暂停/恢复，不链接 ImGui 或编辑器。

撤销/重做不是磁盘版本管理；运行中的水粒子、烟雾纹理/障碍、模拟时间不保存。
脚本热重载、碰撞/导航、全局任意组件编辑、多 Skin、统一 glTF 材质/蒙皮路径和体积流体
均不属于这轮已完成项。操作示例见 [编辑工作流](EDITOR_WORKFLOW.md)。

## 渲染帧的责任边界

`editor/src/EditorApplication.cpp` 组织宿主循环；渲染侧流程如下：

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

`Camera` 保存摄像机位置、前方向、yaw 和 pitch。输入由宿主分发；它根据移动轴和指针坐标更新姿态，并生成：

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

`Scene` 保存可编辑对象与派生世界矩阵，更新父子层级与灯光数据，但不调用 OpenGL。
灯光是场景对象，不应把早期两个移动点光源的固定演示当成当前项目结构。

### GltfAnimatedModel

`GltfAnimatedModel` 使用 tinygltf 读取 glTF 2.0 的 `POSITION`、`JOINTS_0`、
`WEIGHTS_0`、索引、节点、Skin、逆绑定矩阵和动画片段。支持片段选择与切换渐变，
但仍只读取一个蒙皮 Primitive。每帧执行：

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

项目打开/创建在编辑器协调：系统窗口返回原生 Unicode 路径，Project 验证或创建数据，
有未保存修改时提示处理，宿主在退出当前循环后重新初始化项目。
创建不覆盖已有目录，也不动态加载项目 C++；场景与描述文件的保存不是跨文件事务。

模型、材质纹理、阴影资源、场景更新和多遍渲染已经分离，同源模型已经使用
GPU Instancing、HDR/Bloom、Cook–Torrance PBR、天空盒和 split-sum IBL。
真实 HDR/PBR 资产管线、glTF 骨骼动画、视锥剔除和共享纹理缓存已经接入。
静态 glTF 场景路径支持多节点/Primitive 和核心 Metallic-Roughness 材质；蒙皮路径支持多片段选择与渐变，但仍只读取第一个蒙皮 Primitive。两条路径尚未统一，多 Skin、Morph Target、扩展材质和蒙皮阴影仍是后续扩展。CPU 特效粒子有软交界和深度排序；GPU 特效粒子用 Transform Feedback 双缓冲更新并实例化渲染。`Fluid2D` 以速度/密度/压力/散度/障碍纹理执行平流和压力投影；独立 `FluidSystem` 是简化的三维 SPH 与 CPU 等值面实验，水面通过独立颜色/深度副本做屏幕空间折射、吸收、Fresnel 环境反射和边缘效果。默认关闭三维模拟，避免掩盖二维实验的性能。
