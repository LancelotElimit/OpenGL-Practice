# 截图展示与复现实验

[中文 README](../README.md) · [English README](../README.en.md) · [项目说明](PROJECT_GUIDE.md)

本页整理用户提供的四张 Lancelot 编辑器截图。原图完整复制到 `docs/images/`，
不裁剪、不修改像素；英文 UI 是截图时的语言选择，当前程序可以切换中文。
截图不是自动化测试证据，也不是统一硬件/场景下的性能基准。

## 1. 编辑器全景与水对象

![编辑器全景：可停靠面板与水对象调参](images/Engine_Editor_Overview.png)

左侧是层级和性能面板，中间为场景与资源/输出，右侧为 Inspector 和 3D Water Lab。
选中水对象后能调整其变换、可见性、局部模拟和水面设置。
水面来自粒子标量场的连续表面提取；截图本身不代表水与任意模型发生了真实碰撞。

复现步骤：

1. 打开 Sandbox，在 Hierarchy 选择 Water；若已经删除，可从 Add object 添加水对象。
2. 在 Inspector 开启 Render water，打开水体控制窗口；必要时使用 Focus water。
3. Geometry 选择 Surface，观察水面；切换 Particles / Wireframe 对照模拟点与网格。
4. 使用 Pause / Step once、Splash、倒水和网格重建频率，分别观察模拟与重建开销。
5. 单独调节折射、吸收、环境反射、粗糙度与边缘泡沫，避免同时改变所有参数。

水的局部容器边界会随对象变换，但目前不与任意平台或导入模型进行物理碰撞。
材质使用屏幕空间近似折射，无法看见屏幕外物体，也不是光线追踪水体。

## 2. 金属与非金属对比

### 金属、较光滑

![金属低粗糙度：Metallic 1.000，Roughness 0.050](images/PBR_Metallic_Smooth.png)

Inspector 中 Override material 已开启，Metallic 为 1.000，Roughness 为 0.050。
可见较明确的环境反射和明暗细节；低粗糙度不表示表面必须始终更亮。

### 非金属、较粗糙

![非金属高粗糙度：Metallic 0.000，Roughness 1.000](images/PBR_Dielectric_Rough.png)

Metallic 为 0.000，Roughness 为 1.000。材质颜色更直接可见，反射细节更不明显。
这里“Dielectric”指非金属材质响应，并非透明玻璃或折射材质。

| 图片 | Metallic | Roughness | 展示目标 |
| --- | --- | --- | --- |
| PBR_Metallic_Smooth.png | 1.000 | 0.050 | 金属镜面响应和较清晰的环境反射 |
| PBR_Dielectric_Rough.png | 0.000 | 1.000 | 非金属漫反射与粗糙表面响应 |

复现：导入 cube.obj，选中模型，在 Inspector 的 Model component 中开启材质覆盖，
再设置表中数值。可通过相机导航和 F 聚焦，但不同视角会改变反射，未必与截图逐像素一致。
材质覆盖适用于当前静态模型路径；不能推断所有 glTF 材质都由相同覆盖控件驱动。

若要弄清单个参数的作用，请固定环境、视角、曝光与模型，先固定 Metallic 改 Roughness，
再固定 Roughness 改 Metallic。提供的两张图同时改变两个参数，并非单变量实验。
全局 Z/X、C/V 快捷键不等于修改选中对象的材质覆盖值。

## 3. CPU 粒子烟雾

![CPU 粒子发射器：Smoke 预设与实时参数](images/Particle_System_Demo.png)

截图选择 CPU Particle Emitter，使用 Smoke 预设、软交界和发射开关。
可见参数包括约 44.118 个/秒的发射率、2.400 秒寿命、1.000 速度和 0.250 重力。
这些是该次截图的配置，不是系统默认值或推荐性能指标。

复现：在层级选择/添加 CPU 发射器，切换 Smoke 预设并开启 Emit CPU particles；
调整发射率、寿命和大小，用 Q/W/E 改变发射器的局部变换。
已有粒子会跟随对象整体变换，旋转也影响局部重力。Reset this emitter 清空对应运行状态。

请区分以下系统：

- 这里的烟雾：CPU 粒子 + Billboard + 透明混合 + 软交界。
- GPU 发射器：Transform Feedback 更新，使用加法混合；截图未展示它。
- 2D Smoke Lab：速度/密度/压力纹理中的二维流体求解，可在画布注入和绘制障碍。
- 体积烟雾：三维密度与 Ray Marching，目前未实现。

## 4. 原有 Unreal 参考图

以下为此前提供并保留的外部引擎参考，不是 Lancelot 的新截图。

![此前的 Unreal 渲染参考](../assets/comparison-unreal-render.png)

![此前的 Unreal 编辑器参考](../assets/comparison-unreal-editor.png)

相机、光照、模型处理、材质、后处理和截图条件并未统一，不适合作为公平画质或性能测试。
不能仅凭这些截图断言 Unreal 必须依赖纹理才能渲染，或 Lancelot 拥有相同功能覆盖。
这里保留它们只是为了记录视觉目标；本项目更侧重可观察、可调的渲染与模拟数据流。

## 图片文件维护

README 使用 `docs/images/...`，专题文档使用 `images/...`，均为仓库相对路径，
因此不依赖用户电脑上的截图目录。四张新图的来源是用户提供的编辑器截图；
它们并非 AI 生成，也未通过本次文档整理增加任何渲染功能。
截图里的环境与模型素材继续按 [资产来源](../assets/README.md) 记录，
这里不额外声明未知素材或外部引擎截图的许可。
