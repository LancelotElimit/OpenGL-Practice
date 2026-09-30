# 引擎、项目与可编辑对象

## 结构边界

`LancelotEngine` 是独立的 CMake 静态库，包含公共对象、资源加载、模拟、渲染与平台输入采样，不依赖 ImGui。
`LancelotEditor` 单独包含编辑 UI、输入分发、历史记录与编辑器宿主循环；运行生命周期 RuntimeSession 已放在引擎。
`OpenGLPractice` 是很薄的编辑器启动程序，不再创建固定示例对象。
项目是数据目录，通过 `.lancelot` 引用资源根目录与场景 JSON；可放在引擎仓库以外。
项目 C++ 脚本另编译为 `LancelotProjectScripts` 并通过注册表接入，详见
[运行模式](PLAY_MODE.md)。这不是动态脚本模块、插件系统或独立安装包。

```text
OpenGLPractice / editor/src/EditorMain
    ↓ 打开 .lancelot
Project → 资源引用 + scene.json
    ↓
LancelotEditor / EditorApplication
    ├─ EditorWorkspace + 各独立面板
    ├─ EditorInputRouter + PlaySession
    └─ LancelotEngine / EngineApplication
       ├─ Scene：场景数据
       ├─ AssetLibrary：模型导入与缓存
       ├─ SimulationRuntime：模拟与动画更新
       └─ Renderer：只提交绘制
```

## 对象与继承

```text
SceneObject（抽象：名称、ID、可见性、TRS、clone）
├─ RenderableObject
│  ├─ ModelObject（OBJ / glTF / 蒙皮）
│  ├─ PlatformObject（颜色、粗糙度）
│  └─ SimulationObject
│     ├─ ParticleEmitterObject（CPU / GPU）
│     ├─ WaterObject（SPH、水面材质）
│     └─ SmokeObject（二维流体、显示透明度）
├─ LightObject（点光源 / 聚光灯）
├─ CameraObject（视角、预览）
└─ EnvironmentObject（天空、亮度、旋转）
```

采用浅继承区分对象类别，参数使用组合，不把所有对象的参数堆进一个基类。
Scene 持有 `unique_ptr`，复制调用虚拟 `clone()`，防止派生类数据被切掉。
SimulationRuntime 按对象 ID 管理 CPU/GPU 粒子、水与烟雾的独立运行状态。
复制模拟对象会复制参数，但创建新的模拟，而非复制每个动态粒子。
删除实例会回收对应运行时，不删除资源文件。

## 编辑操作

- `Hierarchy → Add object`：创建对象；选中后修改 Inspector。
- `Q / W / E`：移动、旋转、缩放；`F` 聚焦选中对象。
- `Ctrl+D` 复制子树、`Delete` 删除子树；目录中也有按钮和右键菜单。
- `Ctrl+Z` 撤销；`Ctrl+Y` 或 `Ctrl+Shift+Z` 重做；目录拖放或 Inspector 的 Parent 设置层级。
- 资源浏览器 Import / Preview 按类型分流，模型导入创建独立对象；Bind selected model 或拖到 Inspector 只修改选中对象。
  OBJ/glTF 之外的静态模型范围见 [资产格式](ASSET_FORMATS.md)。
- 水：Inspector 开启 Render water，打开 Water controls 编辑对应实例。
- 烟雾：Smoke controls 打开对应模拟画布；场景中显示可变换的 XY 平面。
- 平台：变换会作用于颜色绘制和阴影，可编辑颜色与粗糙度。
- 点光源：移动和颜色、强度有效；聚光灯沿局部 -Z 轴照射，旋转改变方向。
- 相机：修改变换和 FOV 后点击 Preview this camera；自由编辑相机与场景相机分离。
- 环境：旋转改变天空与主 PBR 路径的 IBL；无限远天空忽略位置和缩放。
- `Window → Reset workspace layout` 恢复布局，适合旧布局让场景视图过小的情况。

全局 UI 字体为 20px 微软雅黑，来自 Windows 字体目录 `msyh.ttc`；支持中文名称输入。
没有此字体时回退 ImGui 默认字体，并在 Output 提示。

## 项目格式与运行

```json
{
  "version": 1,
  "name": "My Project",
  "assetRoot": "assets",
  "scene": "scene.json",
  "assets": {
    "model": "model.obj",
    "gltf": "model.glb",
    "animated": "character.gltf",
    "environment": "studio.hdr"
  }
}
```

路径相对项目描述文件；资源引用相对 assetRoot。可选项 `excludeObject` 和
`modelRadius` 控制 OBJ 导入。地面资源键为 `floorColor / floorNormal / floorRoughness / floorAO`。
没有模型或 HDR 的项目仍可用平台、粒子与程序化环境；不会偷用前一个项目的资源根目录。

```powershell
.\out\build\x64-Debug\OpenGLPractice.exe "D:\MyProject\MyProject.lancelot"
```

不提供参数时打开仓库的 Sandbox。编辑状态下 `文件 → 打开项目… / File → Open Project...`
直接弹出 Windows 文件选择窗口，只需选择 `.lancelot`；不是选择模型或 scene.json。
切换会释放旧窗口与 GPU 资源，然后重新创建引擎运行环境。
有未保存修改时可保存并继续、放弃并继续或取消；保存失败不会继续切换。

### 在自选目录新建项目

1. 停止运行，点击 `文件 → 新建项目… / File → New Project...`。
2. 输入项目名称，点击“选择目录…”通过系统窗口选择已经存在的父目录。
3. 界面显示最终目录；点击“创建并打开”，生成同名子目录。

```text
所选父目录/
└─ MyProject/
   ├─ MyProject.lancelot
   ├─ scene.json
   └─ assets/
```

初始场景含平台、点光源、相机与环境，不自动加载 Sandbox 模型或绑定主角脚本。
项目创建、打开和保存支持中文名称/目录；名称不能包含路径分隔符、Windows 非法字符、
保留设备名或末尾空格/点。同名子目录已存在时拒绝创建，不覆盖其中任何内容。
创建后若取消切换，新项目文件仍保留。Play/Pause 期间新建、打开和保存禁用。
`projects/Minimal/Minimal.lancelot` 保留为无 Sandbox 外部模型依赖的最小示例。

`Project::create` 属于引擎的数据创建逻辑；`ProjectDialog` 属于编辑器的 Windows 系统 UI。
系统选择窗口不改变工作目录，也不把项目变成独立安装包。
分发仍需要可执行文件、shaders 和实际引用的资产；C++ 脚本仍须编译注册。

`Ctrl+S` 保存全部对象的 ID、父节点、类型、名称、可见性、局部 TRS、组件参数及独立模型资源引用。
原文件保留为 `.bak`，恢复时关闭编辑器后用备份替换对应文件。
没有保存则重启会放弃内存中的编辑；布局单独保存在本机，与项目场景无关。
当前不保存运行中的水粒子、烟雾纹理、鼠标绘制的障碍或模拟时间。

## 目前的边界

- 烟雾是二维 Stable Fluids 的透明显示面，不是体素或体积光线步进。
- 水模拟与水盆边界在对象局部空间，移动/旋转/缩放整个水对象后边界一起变化。
  尚未与任意场景平台、模型进行物理碰撞。
- 每个水、烟雾、粒子发射器有独立状态；多个水面折射按提交次序处理，透明交叉排序仍有限。
- 多 OBJ/glTF/蒙皮模型可共存；OBJ 和静态 glTF 按路径缓存，蒙皮运行实例独立。空资源引用沿用项目默认模型；缓存当前在项目关闭时释放。
- 主 OBJ/平台路径最多 8 个点光源与 4 个聚光灯；每类只有第一个生成阴影。
  glTF、蒙皮和水当前只取第一个点光源的颜色与强度，不是统一的完整多光源渲染。
- 环境贴图是项目级共享资源；多个环境强度相加，使用首个可见环境的旋转。
- 已有编译式 C++ 脚本绑定与 Play/Pause/Stop；已有撤销/重做、父子层级、暂停单步与独立 Player；尚无脚本热重载、资源热重载和运行状态持久快照。

构建可用 `-DLANCELOT_FETCH_SAMPLE_ASSETS=OFF` 禁止下载 Sandbox 示例资源。
这不关闭引擎依赖库的下载。测试通过 CTest 运行：`SceneEditorTests` 与 `ProjectTests`，
验证变换、拾取、复制、外部项目打开、保存重载和错误场景的事务性加载。
新增 `PlaySessionTests` 验证运行模式、主角控制与脚本生命周期。
`ProjectCreationTests` 检查中文创建、同名保护、非法名称、外部目录保存重开及真实 GL 渲染。
纯引擎构建使用 `-DLANCELOT_BUILD_EDITOR=OFF`；`EngineBoundaryTests` 验证消费者没有编辑器头文件依赖。
详细职责与仍未拆开的部分见 [架构说明](ARCHITECTURE.md)。
新版编辑操作见 [编辑工作流](EDITOR_WORKFLOW.md)。
