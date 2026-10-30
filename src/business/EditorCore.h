#pragma once

// 业务层中枢：由 MainWindow 持有并注入各面板/视口。
// 持有 RuntimeData（不拥有）、SelectionModel、内部剪贴板、UndoStack（阶段4）。
// 可操作对象（演员/屏风/道具/空对象的创建/删除/变换/属性/关键帧/场次标签）的一切
// 数据修改统一走 executeCommand：命令先 apply 到 FileData 副本，再经
// RuntimeData::commitFileData 提交回数据层（渲染层不允许直接改数据）。
// 提交成功的命令入撤销栈；撤销/重做复用同一提交路径（revert/apply 后
// commitFileData），保证渲染层重建与选中剔除行为一致。
// 唯一例外：舞台尺寸（setStage）——舞台只能在设置窗口调整，不属于
// 主视口/Hierarchy/Inspector/时间轴的可操作对象，故不入撤销栈，
// 校验后直接 commitFileData 提交（阶段7 起）。

#include "Commands.h"
#include "KeyframeCommands.h" // ObjectKeyframe（关键帧剪贴板条目）
#include "ResourcePreviewPlayer.h"
#include "SelectionModel.h"
#include "TimelinePlayer.h"
#include "UndoStack.h"
#include "data/DataTypes.h"

#include <QColor>
#include <QObject>
#include <QPointF>
#include <QSet>
#include <QSizeF>

#include <functional>
#include <memory>
#include <optional>

class RuntimeData;

// 双帧段引用（阶段12，运行时状态，不入存档）：时间轴 Ctrl 加选同一对象相邻两帧
// 进入双帧态时由 TimelineCanvas 派生上报，主视口据此绘制两帧幽灵 + 直线路径。
// from < to（秒），帧存在性/相邻性由上报方维护（帧或相邻性消失即清除）。
struct SegmentRef {
    QString objectId;
    double from = 0.0; // 前帧时刻（秒）
    double to = 0.0;   // 后帧时刻（秒）
};

inline bool operator==(const SegmentRef& a, const SegmentRef& b)
{
    return a.objectId == b.objectId && a.from == b.from && a.to == b.to;
}
inline bool operator!=(const SegmentRef& a, const SegmentRef& b) { return !(a == b); }

// 选中路径引用（阶段12 方案A，运行时状态，不入存档）：点选白线设置，
// Inspector 据此切路径页。segment 要求等于当前 activeSegment（白线只在双帧态
// 显示）；pointIndex = 选中路径点下标（-1 = 仅路径选中，无选中点）。
struct ActivePathRef {
    SegmentRef segment;
    int pointIndex = -1;
};

inline bool operator==(const ActivePathRef& a, const ActivePathRef& b)
{
    return a.segment == b.segment && a.pointIndex == b.pointIndex;
}
inline bool operator!=(const ActivePathRef& a, const ActivePathRef& b) { return !(a == b); }

class EditorCore : public QObject {
    Q_OBJECT
public:
    explicit EditorCore(RuntimeData* runtimeData, QObject* parent = nullptr);

    RuntimeData* runtimeData() const { return m_runtimeData; }
    SelectionModel* selection() { return &m_selection; }
    TimelinePlayer* timelinePlayer() { return &m_timelinePlayer; }
    ResourcePreviewPlayer* previewPlayer() { return &m_previewPlayer; }

    // 打帧询问回调：渲染层弹窗实现（含"不再提示"勾选），返回 true=打帧。
    // 业务层只在确实需要询问时调用（未抑制且 T 处无关键帧）。
    using KeyframeConfirmCallback = std::function<bool()>;

    // 统一命令入口。无实际变更（apply 前后 FileData 相等）时丢弃命令、
    // 不提交、不入撤销栈。返回是否产生了真实提交。
    bool executeCommand(std::unique_ptr<Command> command);

    // ---- 撤销 / 重做（阶段4）----
    // 复用命令路径：revert/apply 到 FileData 副本后经 commitFileData 提交；
    // 无可撤销/重做时返回 false 且不动数据。栈随 fileReplaced 清空。
    bool undo();
    bool redo();
    bool canUndo() const { return m_undoStack.canUndo(); }
    bool canRedo() const { return m_undoStack.canRedo(); }

    const ObjectData* findObject(const QString& id) const;

    // 所有对象名称全局唯一；excludeId 用于排除自身
    bool isNameTaken(const QString& name, const QString& excludeId = QString()) const;

    // ---- 创建 / 剪贴板 ----
    bool createObject(ObjectType type, const QPointF& posCm);   // 阶段13 起道具可创建；
                                                                // 空对象忽略 posCm、落在顶层
    void copySelection();                                       // 深拷贝进内部剪贴板（含子树）
    bool pasteClipboard(const std::optional<QPointF>& atCm = std::nullopt); // 无位置 → 偏移 (20,20)
    bool deleteSelection();                                     // 级联删除选中对象的整棵子树
    bool clipboardHasContent() const { return !m_clipboard.isEmpty(); }

    // ---- 层级整理 ----
    // 拖拽落点：newParentId 空串 = 顶层；insertIndex = 移除被移动子树后在
    // newParentId 直系子对象序列中的插入下标。禁止环（拖到自身/后代之下）、
    // 禁止以 Prop 为父对象；返回 false 表示拒绝或无实际变更。
    bool reparent(const QList<QString>& ids, const QString& newParentId, int insertIndex);

    // ---- 变换 / 属性 ----
    // 游标时刻语义（需求第 20 条，阶段5 起所有对象变换修改的唯一入口）：
    // 对每个目标对象——游标 T 处已有关键帧 → upsert 直接更新（不询问）；
    // 无关键帧 → 已抑制则直接打帧，否则经 confirm 询问，选"否"则整个批次
    // 不产生任何数据变更（返回 false，渲染层负责把图元回弹到插值显示位置）。
    // 传入的是"显示基准上的新绝对值"；一批对象合并为一条命令（一步撤销）。
    // 阶段13 修复：位置实际变化且受影响段穿阻挡 → 整条命令拒绝（返回 false）。
    bool setTransformsAtCursor(const QList<ObjectTransform>& updates,
                               const KeyframeConfirmCallback& confirm);
    // 阶段13 修复：视口拖拽预览查询——按 setTransformsAtCursor 同一过滤与命令
    // 路径模拟应用，返回"若提交是否穿阻挡被拒"（不改真实数据）
    bool wouldTransformBlocked(const QList<ObjectTransform>& updates) const;
    bool moveSelectionBy(const QPointF& deltaCm,
                         const KeyframeConfirmCallback& confirm); // 方向键微调，不吸附

    // 打帧按钮：以当前显示值（插值结果）在游标处 upsert，不弹窗。
    // 可用性：单选演员/屏风/道具（多选/空选/空对象禁用，见 canKeyframeSelection）。
    bool keyframeSelectionAtCursor();
    bool canKeyframeSelection() const;

    // 当前时刻的显示值（插值结果）；对象不存在返回 false
    bool displayTransform(const QString& id, Transform& out) const;

    // ---- 关键帧 / 场次标签编辑（时间轴面板入口，全部走 executeCommand）----
    bool upsertKeyframe(const QString& id, const Keyframe& keyframe);
    bool removeKeyframes(const QList<QPair<QString, double>>& refs);
    bool moveKeyframe(const QString& id, double oldT, double newT);
    // 批量改时刻（方向键 ±0.5s 步进）：一条 MoveKeyframesCommand 一步撤销；
    // 逐帧 clamp t≥0，目标时刻已有帧按 upsert 覆盖
    bool moveKeyframes(const QList<QPair<QString, double>>& refs, double deltaT);
    bool addSceneLabel(double t, const QString& name);
    bool renameSceneLabel(int index, const QString& name);
    bool removeSceneLabel(int index);
    bool moveSceneLabel(int index, double newT); // 按下标改 t（无变更不入栈）

    // ---- 关键帧级剪贴板（业务层持有，与对象剪贴板互相独立；运行时状态不入存档）----
    void copySelectedKeyframes(const QList<QPair<QString, double>>& refs); // 深拷贝 (id,t,x,y,rotation)，不存在的帧跳过
    // 复制集最小 t 对齐游标、保持相对间隔 upsert（一条批量命令一步撤销）；
    // 目标对象已删除的条目跳过；空集/无有效目标返回 false
    bool pasteKeyframesAtCursor();
    bool keyframeClipboardHasContent() const { return !m_keyframeClipboard.isEmpty(); }

    bool setObjectName(const QString& id, const QString& name); // 空名/重名拒绝
    bool setObjectColor(const QString& id, const QColor& color);
    bool setScreenSize(const QString& id, const QSizeF& sizeCm);
    // 道具（阶段13）：缩放两值一次提交（xy 绑定语义由 Inspector 组装）；
    // 范围 0.1~100 仅正值，越界拒绝；standable 切换（false = 阻挡路径编辑）
    bool setPropScale(const QString& id, double scaleX, double scaleY);
    bool setPropStandable(const QString& id, bool standable);
    // 道具形状（阶段13 任务2）：形状编辑器确定时一次性替换；写入前绕向归一
    // 逆时针；合围校验在编辑器侧实时做（OK 禁用），此处只校验类型
    bool setPropShape(const QString& id, const PropShape& shape);

    // 舞台设置（阶段7 起直改直提交）：宽/深必须 > 0，非法尺寸拒绝；无变更不提交
    // （返回 false）。不入撤销栈——撤销栈只覆盖主视口/Hierarchy/Inspector/时间轴
    // 中的可操作对象，舞台只能在设置窗口调整，不在其列；校验后直接经
    // commitFileData 提交，dirty 跟踪与 fileDataChanged 刷新路径与命令提交一致。
    bool setStage(const StageData& stage);

    // ---- 台下准备区（阶段10，与 setStage 同例：直改直提交、不入撤销栈） ----
    // 只能在设置窗口的舞台页方块编辑器中调整。校验：id/name 全局唯一非空、宽高 > 0；
    // 无变更不提交（返回 false）。
    bool setPrepAreas(const QList<PrepAreaData>& prepAreas);
    // 新建准备区：自动唯一 id（prep-NNN 递增查空）与唯一名（"准备区N" 按现存最大
    // N+1），默认 400×300，中心落在 (x, y)；内部走 setPrepAreas，返回新 id（失败空串）
    QString addPrepArea(double x, double y);
    bool removePrepArea(const QString& id);
    const PrepAreaData* findPrepArea(const QString& id) const;

    // ---- 双帧段选中态（阶段12，运行时状态，不入存档）----
    // 由 TimelineCanvas 派生上报/清除；StageScene 经 MainWindow 订阅
    // activeSegmentChanged 绘制两帧幽灵 + 直线路径。相同值重复设置不发信号。
    // 段消失/变更时一并清除 activePath。
    std::optional<SegmentRef> activeSegment() const { return m_activeSegment; }
    void setActiveSegment(const std::optional<SegmentRef>& segment);

    // ---- 选中路径（阶段12 方案A，运行时状态，不入存档）----
    // 主视口点选白线设置（pointIndex 可另选路径点）；Inspector 订阅切路径页。
    // 对象选中变化 / 双帧态消失 / 命令提交后路径数据失效 → 清除。
    std::optional<ActivePathRef> activePath() const { return m_activePath; }
    void setActivePath(const std::optional<ActivePathRef>& path);

    // ---- 项目资源库（阶段14，命令入撤销栈；选中态为运行时状态不存档）----
    const ResourceData* findResource(const QString& id) const;
    bool isResourceNameTaken(const QString& name, const QString& excludeId = QString()) const;
    QString generateResourceId() const; // res-<uuid>，全库唯一且不复用
    // "名称"/"名称 (2)"…；reservedNames 供批量导入在提交前预留本批已用别名
    QString uniqueResourceName(const QString& baseName,
                               const QSet<QString>& reservedNames = {}) const;
    // 导入成功项批量提交（一条命令一步撤销）；校验 id/别名唯一、路径安全
    bool addResources(const QList<ResourceData>& resources);
    bool removeResource(const QString& id);                 // 只删清单，不删磁盘文件
    bool renameResource(const QString& id, const QString& name); // 空名/重名拒绝

    QString activeResourceId() const { return m_activeResourceId; }
    // 选中资源：清空对象选择、双帧段与路径选择；传空串清除资源选中态。
    // 对象/路径选中、资源被删除、新建/打开项目时资源选中态自动清除。
    void setActiveResource(const QString& id);

    // ---- 移动路径（阶段12，全部走命令入撤销栈；级联见关键帧/对象命令）----
    const PathData* findPath(const QString& objectId, double from, double to) const;
    bool setPathType(const QString& objectId, double from, double to, PathType type);
    bool setPathLegAngle(const QString& objectId, double from, double to, int legIndex,
                         double angleDeg); // ±350° 钳制
    bool addPathPoint(const QString& objectId, double from, double to,
                      const QPointF& scenePos); // 点吸附到曲线点击处，圆心角保形拆分
    bool movePathPoint(const QString& objectId, double from, double to, int pointIndex,
                       const QPointF& scenePos);
    bool removePathPoint(const QString& objectId, double from, double to, int pointIndex);
    bool setPathPointT(const QString& objectId, double from, double to, int pointIndex,
                       double t); // 沿曲线滑动重定位 + 相邻钳制保序
    bool resetPath(const QString& objectId, double from, double to); // 恢复直线

signals:
    void clipboardChanged();
    void undoStackChanged(); // 入栈/撤销/重做/清空后发出，供菜单可用性刷新
    void activeSegmentChanged(); // 双帧段选中态变化（含清除为 nullopt）
    void activePathChanged();    // 选中路径变化（含清除为 nullopt）
    void activeResourceChanged(); // 资源选中态变化（含清除为空串）

private:
    QString nextFreeId(ObjectType type, QSet<QString>& taken) const; // 占用集合就地更新
    QString generateObjectId(ObjectType type) const;
    QString generateObjectName(ObjectType type) const;               // 角色N / 屏风N / 空对象N
    QString uniqueName(const QString& base, QSet<QString>& takenNames) const; // 粘贴用：重名则数字后缀
    QColor nextPaletteColor();                                       // 调色板轮转
    void pruneSelection();                                           // 剔除已不存在的选中 id
    void pruneActivePath(); // 剔除失效的选中路径（段/路径点不存在即清除，fileDataChanged）
    void pruneActiveResource(); // 剔除已删除资源的选中态（fileDataChanged）

    RuntimeData* m_runtimeData = nullptr; // 不拥有
    TimelinePlayer m_timelinePlayer;      // 游标/插值/播放（运行时状态，不入存档）
    ResourcePreviewPlayer m_previewPlayer; // 音频试听（运行时状态，不入存档）
    SelectionModel m_selection;
    QList<ObjectData> m_clipboard;
    QList<ObjectKeyframe> m_keyframeClipboard; // 关键帧级剪贴板（独立于对象剪贴板）
    UndoStack m_undoStack;
    int m_paletteCursor = 0;
    std::optional<SegmentRef> m_activeSegment; // 双帧段选中态（阶段12，运行时状态）
    std::optional<ActivePathRef> m_activePath; // 选中路径（阶段12，运行时状态）
    QString m_activeResourceId; // 资源选中态（阶段14，运行时状态，不入存档）
};
