# 最小可玩运行时

## 立即体验

Sandbox 的 `Player`（当前使用鼠标模型作为角色）已经绑定 `PlayerController`。

1. 顶部点击 **Play**。
2. 点击 **Scene View** 的画面，让它获得键盘焦点。
3. **WASD** 沿世界 XZ 平面移动角色；默认跟随相机保持固定偏移。
4. **Pause** 或 **Esc** 暂停；**Resume** 继续。
5. **Stop** 回到编辑状态，角色位置和编辑相机恢复开始前的状态。

运行/暂停时场景编辑、资源导入、项目切换与保存被禁用，避免误把临时状态写进项目。
暂停仍正常渲染画面、响应 UI，但不更新脚本、骨骼动画、CPU/GPU 粒子、水或烟雾。
编辑状态保留原有模拟预览；开始/停止会重新创建模拟状态，不保留预览中的动态粒子。

## 给另一个对象绑定脚本

停止运行后，选中对象，在 Inspector 的 **C++ Script** 部分：

- **Behaviour** 选择 `PlayerController`；None 表示不绑定。
- **Script enabled** 控制是否执行脚本。
- **Main character** 指定接收玩家输入的对象；勾选时自动取消其他对象的主角标记。
- **Move speed** 设置移动速度（世界单位/秒）。
- **Face movement** 使角色局部 -Z 朝向移动方向。
- **Follow camera / Camera offset** 设置跟随与世界空间相机偏移。

Ctrl+S 保存绑定和参数。复制对象会复制脚本参数，但不会复制主角资格。
目前每个对象只绑定一个脚本，只有主角接收移动输入；其他启用的脚本仍执行生命周期。
可以不指定主角，单独运行模拟；未注册脚本或多个主角会阻止开始，并在 Output 提示。

## 结构与执行顺序

```text
编辑场景（保存到项目）
    │ Play：深复制对象数据，创建脚本实例
    ▼
运行场景 → OnStart
    │ 每帧输入 → OnUpdate(deltaTime)
    │ 更新变换 → 模拟/动画 → 渲染
    ├─ Pause：停逻辑与模拟，继续渲染/UI
    ├─ Resume：恢复更新，不补算暂停时长
    └─ Stop：OnStop → 丢弃运行场景 → 恢复编辑相机
```

`PlaySession` 持有运行场景，编辑场景本身不被角色脚本改写。
所有工具栏命令在下一帧边界应用，避免绘制 UI 期间替换场景造成悬空引用。
脚本调用发生错误时暂停运行并报告；这里的处理针对普通 C++ 异常，不能隔离内存破坏或崩溃。

## C++ 脚本：项目代码与引擎分离

- `include/ScriptBehaviour.h`：引擎提供 `OnStart / OnUpdate / OnStop`、所属对象、输入、相机和时间步。
- `include/PlaySession.h`、`src/PlaySession.cpp`：引擎管理状态、场景副本与生命周期。
- `projects/Sandbox/Scripts/PlayerController.cpp`：项目行为，独立编译为 `LancelotProjectScripts`。
- `src/EditorMain.cpp`：注册编译好的项目脚本，再把注册表传给引擎。

增加脚本时，继承 `ScriptBehaviour`，在 `registerProjectScripts` 中注册工厂，
把源文件加入项目脚本库并重新编译，然后在 Inspector 中选择它。
本版本是静态编译脚本注册，不会从资源浏览器动态编译任意 cpp；切换项目也不会动态加载新的脚本模块。
当前编辑器中编译的项目脚本注册表可供所打开的数据项目使用。

角色移动只是 `position += velocity * deltaTime`，斜向输入会归一化，长帧时间步限制为 50ms。
尚无碰撞、重力、跳跃、导航、角色动画状态机、固定物理步或脚本热重载。
输入需要 Scene View 焦点，点击菜单或文本框后不再控制角色；运行时 Q/W/E 编辑工具禁用。

## 验证

CTest 包含 `PlaySessionTests`，检查生命周期、斜向速度、暂停、恢复、相机还原、
再次开始、主角唯一性、未注册脚本和脚本异常；`ProjectTests` 检查绑定参数保存重载。
