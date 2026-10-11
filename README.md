# StageRehearsal（舞台剧排练预演）

在 PC 上模拟舞台剧排练情况的桌面软件：以俯视角观察整个舞台，安排演员、屏风和舞台道具的走位，并通过时间轴关键帧编排整场演出的调度与场次节奏。

## 功能特性

### 舞台与视口
- 俯视角 2D 舞台，单位为厘米（cm），默认参考比例 1:50，网格间距 100cm × 100cm
- 坐标系：右为 x 轴正、下为 y 轴正；原点在舞台最里侧边中心，可调整中心对称点位置
- 滚轮缩放（以鼠标为中心）、右键拖动平移
- 舞台本身不可在主视口交互，通过编辑菜单中的〈舞台〉设置窗口调整大小与中心点

### 对象系统
- **演员**：半径 30cm 圆形 + 白色朝向三角（拖拽三角可改朝向），不同演员以颜色区分
- **屏风**：默认 100cm × 10cm 矩形，可自定义大小
- **舞台道具**：预留占位，暂不可创建
- **分组**：纯组织节点，用于在 Hierarchy 中整理对象
- 朝向约定：y 轴正方向（下）为 0°，逆时针增大；角色名/屏风名不可重复

### 编辑操作
- 主视口或 Hierarchy 空白处右键 → Create 创建对象
- 鼠标拖拽移动，方向键微调（每次 10cm）；可选网格吸附（范围 20cm）
- 单击选中、Ctrl 多选、左键框选（中心点被框住即选中），多选可整体移动
- 复制 / 粘贴（默认偏移 20cm，右键粘贴到鼠标位置）/ 删除
- 主视口、Hierarchy、Inspector、时间轴中的一切修改均可撤销 / 重做

### 时间轴
- 全局单条时间轴，单位为秒，总时长可延长
- 关键帧记录位置 + 朝向，按最短路线匀速插值；显式打帧按钮
- 播放 / 暂停 / 倍速，游标可拖动，主视口实时联动
- 每个对象一条轨道，同类对象可折叠；关键帧可选中、拖动、Delete 删除
- 场次标签：在时间轴上标注幕/场次（视觉标记）

### 界面布局
- 参考 Unity 的布局：主视口 + Hierarchy 对象列表 + Inspector 属性面板
- Inspector 中的修改实时反映到主视口

### 文件与存档
- JSON 存档（`.json`），根节点含 `version` 字段，便于后续格式迁移；示例见 [examples/stage-demo.json](examples/stage-demo.json)
- 手动保存（Ctrl+S），不自动保存；未保存时文件名后显示 `*`
- 退出 / 新建 / 打开时提示未保存修改；加载损坏文件给出提示；另存为重名时确认覆盖

## 技术栈

- C++17，Qt 6（≥ 6.4，界面用 Widgets，主视口用 QGraphicsView/QGraphicsScene）
- JSON 序列化使用 QJson（QJsonDocument/QJsonObject）
- CMake（≥ 3.20）构建，编译器 MSVC 2022，目标平台 Windows

代码按三层组织（见下文目录结构）：数据层 / 业务层 / 渲染层。

## 构建

前提：已安装 Qt 6.4+ 与 CMake 3.20+，且 `CMAKE_PREFIX_PATH` 指向 Qt 安装目录（或使用 Qt 命令行环境）。

```bash
cmake -B build
cmake --build build --config Debug
```

生成的可执行文件位于 `build/Debug/StageRehearsal.exe`（Release 构建则对应 `build/Release/`）。示例存档 `stage-demo.json` 会在配置时自动拷贝到可执行文件旁。

如需打包分发，可使用 Qt 自带的 `windeployqt` 收集运行库。

## 运行自测

项目提供两个独立自测目标（随上述构建一并生成）：

```bash
# 数据层 / 业务层自测：存档读写、命令、撤销栈、选择模型、时间轴插值等
./build/Debug/DataLayerSelfTest.exe

# 交互自测：QTest 合成事件驱动真实主窗口，覆盖视口/面板交互路径
QT_QPA_PLATFORM=offscreen ./build/Debug/InteractionSelfTest.exe
```

## 目录结构

```
src/
  main.cpp        程序入口
  data/           数据层：存档 Schema、JSON 读写、应用设置、运行时数据
  business/       业务层：命令、撤销栈、选择模型、时间轴播放、关键帧命令
  render/         渲染层：主窗口、视口/场景、各对象图元、Hierarchy/Inspector、时间轴控件
tests/            自测程序（DataLayerSelfTest / InteractionSelfTest）
examples/         示例存档 stage-demo.json
doc/              需求与实施计划文档
```

## 文档

- 需求说明：[doc/需求.md](doc/需求.md)
- 实施计划：[doc/实施计划.md](doc/实施计划.md)

## 许可证

[MIT](LICENSE)
