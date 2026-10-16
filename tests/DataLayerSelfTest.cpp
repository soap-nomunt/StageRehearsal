// 数据层自测程序（独立可执行目标，不影响主程序）。
// 覆盖：存档往返逐字段比对、损坏/版本不符/缺失/结构非法文件的错误枚举、
// AppSettings 的 QSettings 跨进程持久化、RuntimeData 基本行为；
// 业务层命令 apply/revert（含层级整理 reparent、级联删除）、EditorCore 集成、
// parent/group 存档兼容（旧存档无 parent 字段视为顶层）；
// 阶段4：UndoStack 入栈/撤销/重做/redo 区清空/文件替换清栈、
// 20 步确定性操作序列逐级撤销回初始再逐级重做一致。
// 阶段5：TimelinePlayer 插值（直线匀速/最短弧/首前末后保持/单帧/乱序防御）、
// 时间轴命令往返（upsert/删除/改时刻/标签/时长自动延长）、打帧流程
// （已有帧直接更新/无帧询问/选否零变更/抑制直打/打帧按钮显示值/空对象跳过）、
// 撤销/重做关键帧命令后游标处插值立即刷新、播放 tick 到末尾自动停止。
// 时间轴交互完善追加：批量改时刻（多对象平移/覆盖/clamp 0/同批后者覆盖前者/
// 时长延长/redo 幂等）、场次标签移动（重排/名不变/时长延长）、EditorCore
// 关键帧级剪贴板（游标对齐粘贴/相对间隔/upsert 覆盖/撤销/目标删除跳过）。
// 阶段6 追加：RuntimeData dirty 跟踪（快照比较、dirtyChanged 只在翻转时发、
// 撤销回已存内容 = 干净）。
// 阶段7：EditorCore::setStage 直改直提交语义——合法修改生效 / 宽深非法拒绝 /
// 无变更不提交 / 不产生撤销条目（canUndo 不变）/ undo 不影响舞台值。
// 阶段8：AppSettings 最近使用列表（MRU：追加置顶 / 大小写不敏感去重 / 超 10 条
// 截断 / removeRecentFile / 跨实例读回）；RuntimeData::openFile/saveFile 成功
// 入 MRU、openFile 失败不入。
// 阶段10：存档 v2——台下准备区 PrepAreaData（往返逐字段一致 / v1 存档迁移为
// 空列表 / version 受理区间 [1,2] / 非法字段拒载）；EditorCore::setPrepAreas /
// addPrepArea / removePrepArea 直改直提交（校验拒绝 / 自动命名递增 / 不入
// 撤销栈 / undo 不影响准备区）。
// 阶段11：存档 v3——PrepAreaData 新增 color（显式色往返 / invalid = 派生态 /
// v2 存档 prepAreas 无 color 键宽容迁移为派生态 / 受理区间 [1,3]，version=4 才拒）；
// setPrepAreas 颜色变更生效（operator== 含 color，dirty 判定不失效）。
// 阶段12：存档 v4——FileData.paths 移动路径（往返逐字段 / v3 无 paths 节点宽容
// 迁移为空 / 受理区间 [1,4]，version=5 才拒）；EditorCore 路径命令（类型切换/
// 圆心角钳制/加点保形/拖点/删点合并/t 滑动保序/恢复直线）与关键帧级联（改时刻
// 重绑定、段内打帧丢弃、删帧相邻两段丢弃、删对象级联）及撤销恢复；插值弧长匀速
// 数值断言（等 t 步进 = 等弧长步进，无路径段与旧行为逐位一致）。
#include "business/Commands.h"
#include "business/EditorCore.h"
#include "business/KeyframeCommands.h"
#include "business/PathGeometry.h"
#include "business/SelectionModel.h"
#include "business/TimelinePlayer.h"
#include "business/UndoStack.h"
#include "data/AppSettings.h"
#include "data/DataTypes.h"
#include "data/JsonStore.h"
#include "data/RuntimeData.h"
#include "render/RenderUtil.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineF>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

namespace {

int g_failures = 0;

QTextStream& out()
{
    static QTextStream stream(stdout);
    return stream;
}

#define CHECK(condition, message)                                                        \
    do {                                                                                 \
        if (condition) {                                                                 \
            out() << "[PASS] " << message << Qt::endl;                                   \
        } else {                                                                         \
            out() << "[FAIL] " << message << " (" << __FILE__ << ":" << __LINE__ << ")"  \
                  << Qt::endl;                                                           \
            ++g_failures;                                                                \
        }                                                                                \
    } while (0)

// 本进程内使用的独立设置区，避免污染正式软件的设置
const char kTestOrgName[] = "StageRehearsal";
const char kTestAppName[] = "StageRehearsalSelfTest";

bool writeTextFile(const QString& path, const QByteArray& content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(content) == content.size();
}

// 构造一份带自定义数值的数据：舞台 / 演员（3 关键帧）/ 屏风（size + 2 关键帧）/
// 2 个准备区（阶段10；阶段11 起 prep1 显式色、prep2 派生态，覆盖两条颜色路径）/
// 2 个场次标签。
// 数值均取二进制可精确表示的“整齐”值，保证 double 严格相等。
FileData makeSampleData()
{
    FileData data;
    data.stage = { 1500.0, 900.0, 50.0 };

    ObjectData actor;
    actor.id = QStringLiteral("actor-001");
    actor.type = ObjectType::Actor;
    actor.name = QStringLiteral("张三");
    actor.color = QColor(QStringLiteral("#e74c3c"));
    actor.transform = { 300.0, 200.0, 45.0 };
    actor.keyframes = {
        { 2.0, 300.0, 200.0, 45.0 },
        { 5.5, 400.0, 250.0, 90.0 },
        { 9.0, 350.0, 300.0, 135.0 },
    };
    data.objects.append(actor);

    ObjectData screen;
    screen.id = QStringLiteral("screen-001");
    screen.type = ObjectType::Screen;
    screen.name = QStringLiteral("屏风1");
    screen.color = QColor(QStringLiteral("#888888"));
    screen.size = QSizeF(100.0, 10.0);
    screen.transform = { -150.0, 100.0, 90.0 };
    screen.keyframes = {
        { 1.0, -150.0, 100.0, 90.0 },
        { 7.25, -50.0, 150.0, 0.0 },
    };
    data.objects.append(screen);

    // 台下准备区（阶段10）：舞台左外侧 / 台下各一
    PrepAreaData prep1;
    prep1.id = QStringLiteral("prep-001");
    prep1.name = QStringLiteral("准备区1");
    prep1.x = -1050.0;
    prep1.y = 450.0;
    prep1.width = 400.0;
    prep1.height = 300.0;
    prep1.color = QColor(QStringLiteral("#d35400")); // 阶段11：显式独立色
    data.prepAreas.append(prep1);

    PrepAreaData prep2;
    prep2.id = QStringLiteral("prep-002");
    prep2.name = QStringLiteral("准备区2");
    prep2.x = 0.0;
    prep2.y = 1150.0;
    prep2.width = 600.0;
    prep2.height = 400.0;
    // color 保持 invalid（阶段11：派生态路径）
    data.prepAreas.append(prep2);

    // 移动路径（阶段12，v4）：actor-001 的 2.0→5.5 段配一条弧线（1 路径点 + 2 段圆心角）
    PathData path;
    path.objectId = QStringLiteral("actor-001");
    path.from = 2.0;
    path.to = 5.5;
    path.type = PathType::Arc;
    path.points = { PathPoint { 500.0, 100.0 } };
    path.legAngles = { 20.0, -15.0 };
    data.paths.append(path);

    data.timeline.duration = 90.5;
    data.timeline.labels = {
        { 0.0, QStringLiteral("第一幕") },
        { 45.25, QStringLiteral("第二幕") },
    };
    return data;
}

void testTypeMapping()
{
    ObjectType type = ObjectType::Actor;
    CHECK(objectTypeToString(ObjectType::Actor) == QStringLiteral("actor"), "ObjectType::Actor -> \"actor\"");
    CHECK(objectTypeToString(ObjectType::Screen) == QStringLiteral("screen"), "ObjectType::Screen -> \"screen\"");
    CHECK(objectTypeToString(ObjectType::Prop) == QStringLiteral("prop"), "ObjectType::Prop -> \"prop\"（预留道具）");
    CHECK(objectTypeFromString(QStringLiteral("prop"), type) && type == ObjectType::Prop, "\"prop\" -> ObjectType::Prop");
    CHECK(objectTypeToString(ObjectType::Group) == QStringLiteral("group"), "ObjectType::Group -> \"group\"");
    CHECK(objectTypeFromString(QStringLiteral("group"), type) && type == ObjectType::Group, "\"group\" -> ObjectType::Group");
    CHECK(!objectTypeFromString(QStringLiteral("camera"), type), "未知类型字符串被拒绝");
}

// 阶段2：角度制式换算（RenderUtil.h，不依赖 Widgets）。
// 需求制式：0°=+y（朝下）、逆时针增大；验收约定 rotation=90 朝左（−x）。
void testAngleConversion()
{
    // 需求角度 → Qt setRotation 角度：恒等映射（推导见 RenderUtil.h 注释）
    CHECK(RenderUtil::toQtRotation(0.0) == 0.0, "toQtRotation(0) = 0");
    CHECK(RenderUtil::toQtRotation(90.0) == 90.0, "toQtRotation(90) = 90");
    CHECK(RenderUtil::toQtRotation(-45.0) == -45.0, "toQtRotation(-45) = -45（负角直传）");

    // 朝向单位向量（场景坐标 x 右、y 下）
    const QPointF down = RenderUtil::directionVector(0.0);
    CHECK(qAbs(down.x()) < 1e-9 && qAbs(down.y() - 1.0) < 1e-9,
          "rotation=0 → 朝向 +y（朝下）");
    const QPointF left = RenderUtil::directionVector(90.0);
    CHECK(qAbs(left.x() + 1.0) < 1e-9 && qAbs(left.y()) < 1e-9,
          "rotation=90 → 朝向 −x（朝左）");
    const QPointF up = RenderUtil::directionVector(180.0);
    CHECK(qAbs(up.x()) < 1e-9 && qAbs(up.y() + 1.0) < 1e-9,
          "rotation=180 → 朝向 −y（朝上）");
    const QPointF right = RenderUtil::directionVector(270.0);
    CHECK(qAbs(right.x() - 1.0) < 1e-9 && qAbs(right.y()) < 1e-9,
          "rotation=270 → 朝向 +x（朝右）");
}

// 阶段3：rotationFromVector 反向换算（归一化到 [0,360)，与 directionVector 往返一致）
void testRotationFromVector()
{
    // 基准方向向量 → 需求角度
    CHECK(qAbs(RenderUtil::rotationFromVector(QPointF(0.0, 1.0))) < 1e-9,
          "向量(0,1) → 0°");
    CHECK(qAbs(RenderUtil::rotationFromVector(QPointF(-1.0, 0.0)) - 90.0) < 1e-9,
          "向量(-1,0) → 90°");
    CHECK(qAbs(RenderUtil::rotationFromVector(QPointF(0.0, -1.0)) - 180.0) < 1e-9,
          "向量(0,-1) → 180°");
    CHECK(qAbs(RenderUtil::rotationFromVector(QPointF(1.0, 0.0)) - 270.0) < 1e-9,
          "向量(1,0) → 270°");

    // 零向量无朝向 → 0；非单位向量按方向归一
    CHECK(RenderUtil::rotationFromVector(QPointF(0.0, 0.0)) == 0.0, "零向量 → 0°");
    CHECK(qAbs(RenderUtil::rotationFromVector(QPointF(0.0, 5.0))) < 1e-9,
          "非单位向量(0,5) → 0°");
    CHECK(qAbs(RenderUtil::rotationFromVector(QPointF(-2.0, -2.0)) - 135.0) < 1e-9,
          "向量(-2,-2) → 135°");

    // 与 directionVector 往返一致（含 [0,360) 归一化边界）
    for (double degrees : { 0.0, 1.0, 30.0, 45.0, 89.9, 90.0, 135.0, 179.9,
                            180.0, 225.0, 270.0, 315.0, 359.9 }) {
        const double roundTrip =
            RenderUtil::rotationFromVector(RenderUtil::directionVector(degrees));
        CHECK(qAbs(roundTrip - degrees) < 1e-9,
              QStringLiteral("directionVector→rotationFromVector 往返一致 θ=%1").arg(degrees));
    }
}

// 阶段3：命令 apply/revert 往返（移动/创建/删除各一，revert 后 FileData 逐字段还原）
void testCommandRoundTrip()
{
    const FileData original = makeSampleData();

    // 移动（SetTransformsCommand 同时覆盖拖拽移动/方向键微调/Inspector 位置朝向）
    {
        FileData data = original;
        SetTransformsCommand move({ { QStringLiteral("actor-001"), { 500.0, 600.0, 45.0 } } });
        move.apply(data);
        CHECK(data.objects.at(0).transform == Transform({ 500.0, 600.0, 45.0 }),
              "移动命令 apply 生效");
        CHECK(data != original, "移动命令 apply 后与原数据不同");
        move.revert(data);
        CHECK(data == original, "移动命令 revert 后 FileData 逐字段还原");
    }

    // 旋转（同一命令类，改朝向）
    {
        FileData data = original;
        SetTransformsCommand rotate({ { QStringLiteral("screen-001"),
                                        { -150.0, 100.0, 270.0 } } });
        rotate.apply(data);
        CHECK(qAbs(data.objects.at(1).transform.rotation - 270.0) < 1e-12,
              "旋转命令 apply 生效");
        rotate.revert(data);
        CHECK(data == original, "旋转命令 revert 后 FileData 逐字段还原");
    }

    // 创建（粘贴同用 AddObjectsCommand）
    {
        FileData data = original;
        ObjectData created;
        created.id = QStringLiteral("actor-002");
        created.type = ObjectType::Actor;
        created.name = QStringLiteral("角色1");
        created.color = QColor(QStringLiteral("#3498db"));
        created.transform = { 0.0, 0.0, 0.0 };
        AddObjectsCommand create(QList<ObjectData> { created });
        create.apply(data);
        CHECK(data.objects.size() == 3 && data.objects.at(2) == created,
              "创建命令 apply 生效");
        create.revert(data);
        CHECK(data == original, "创建命令 revert 后 FileData 逐字段还原");
    }

    // 删除（含原位还原：删第二个对象，revert 后顺序不变）
    {
        FileData data = original;
        RemoveObjectsCommand remove(QList<QString> { QStringLiteral("screen-001") });
        remove.apply(data);
        CHECK(data.objects.size() == 1 && data.objects.at(0).id == QStringLiteral("actor-001"),
              "删除命令 apply 生效");
        remove.revert(data);
        CHECK(data == original, "删除命令 revert 后 FileData 逐字段还原（原位插回）");
    }
}

// 阶段3：EditorCore 集成（命令提交回数据层 / 唯一名 / 剪贴板 / 选中剔除）
// 注意：每次命令提交都整体替换 FileData，故只跨操作持有 id 副本，数据随用随取。
void testEditorCore()
{
    RuntimeData runtime;
    EditorCore core(&runtime);

    int emissions = 0;
    QObject::connect(&runtime, &RuntimeData::fileDataChanged, &runtime, [&emissions] { ++emissions; });

    // 创建演员：自动 id/唯一名/调色板色，落在指定位置并自动选中
    CHECK(core.createObject(ObjectType::Actor, QPointF(100.0, 200.0)), "创建演员成功");
    CHECK(runtime.fileData().objects.size() == 1, "创建经命令提交回数据层");
    CHECK(emissions == 1, "commitFileData 发出 fileDataChanged");
    {
        const ObjectData actor = runtime.fileData().objects.at(0);
        CHECK(actor.id == QStringLiteral("actor-001") && actor.name == QStringLiteral("角色1"),
              "创建自动生成 id 与唯一名（角色1）");
        CHECK(actor.transform.x == 100.0 && actor.transform.y == 200.0, "创建落在指定位置");
        CHECK(actor.color == AppSettings::defaultPalette().first(), "创建取调色板第一个颜色");
        CHECK(core.selection()->singleSelectedId() == actor.id, "创建后自动选中");
    }
    const QString actorId = runtime.fileData().objects.at(0).id;

    // 创建屏风：默认尺寸、调色板轮转、名字不冲突
    CHECK(core.createObject(ObjectType::Screen, QPointF(0.0, 0.0)), "创建屏风成功");
    const QString screenId = runtime.fileData().objects.at(1).id;
    {
        const ObjectData screen = runtime.fileData().objects.at(1);
        CHECK(screen.name == QStringLiteral("屏风1"), "屏风自动命名（屏风1）");
        CHECK(screen.size && *screen.size == QSizeF(AppSettings::screenDefaultWidthCm,
                                                    AppSettings::screenDefaultHeightCm),
              "屏风默认尺寸取 AppSettings 常量");
        CHECK(screen.color == AppSettings::defaultPalette().at(1), "调色板轮转取色");
    }

    // 道具占位：拒绝创建
    CHECK(!core.createObject(ObjectType::Prop, QPointF(0.0, 0.0)), "道具创建被拒绝（占位禁用）");
    CHECK(runtime.fileData().objects.size() == 2, "道具未进入数据");

    // 名字唯一性（演员与屏风统一校验，排除自身）
    CHECK(core.isNameTaken(QStringLiteral("角色1")), "重名检测命中");
    CHECK(!core.isNameTaken(QStringLiteral("角色1"), actorId), "重名检测排除自身");
    CHECK(!core.setObjectName(screenId, QStringLiteral("角色1")), "重名拒绝");
    CHECK(!core.setObjectName(actorId, QStringLiteral("  ")), "空名拒绝");
    CHECK(core.setObjectName(screenId, QStringLiteral("背景墙")), "合法改名生效");
    CHECK(core.findObject(screenId)->name == QStringLiteral("背景墙"), "改名提交回数据层");

    // 复制/粘贴：默认偏移 (x+20, y+20)、新 id、唯一名、粘贴后选中
    core.selection()->select(actorId);
    core.copySelection();
    CHECK(core.clipboardHasContent(), "复制进内部剪贴板");
    CHECK(core.pasteClipboard(), "粘贴成功（默认偏移）");
    CHECK(runtime.fileData().objects.size() == 3, "粘贴经命令提交回数据层");
    {
        const ObjectData pasted = runtime.fileData().objects.at(2);
        CHECK(pasted.id != actorId, "粘贴生成新 id");
        CHECK(pasted.transform.x == 120.0 && pasted.transform.y == 220.0,
              "粘贴默认偏移 (x+20, y+20)");
        CHECK(pasted.name == QStringLiteral("角色2"), "粘贴生成唯一名（角色2）");
        CHECK(core.selection()->singleSelectedId() == pasted.id, "粘贴后选中副本");
    }

    // 右键位置粘贴：落在指定点
    CHECK(core.pasteClipboard(QPointF(-50.0, -60.0)), "右键位置粘贴成功");
    {
        const ObjectData pastedAt = runtime.fileData().objects.at(3);
        CHECK(pastedAt.transform.x == -50.0 && pastedAt.transform.y == -60.0,
              "右键粘贴落在鼠标位置");
    }

    // 方向键微调（moveSelectionBy：游标时刻语义——基准为当前时刻显示值，
    // 游标处无关键帧 → 确认回调选"是"后打帧；10cm 语义由视口传入，这里验证命令路径）
    core.selection()->select(actorId);
    const int before = emissions;
    CHECK(core.moveSelectionBy(QPointF(10.0, -10.0), [] { return true; }), "微调打帧提交");
    const ObjectData* moved = core.findObject(actorId);
    CHECK(moved->transform.x == 100.0 && moved->transform.y == 200.0,
          "游标时刻语义：静态 transform 不变（打帧不改静态值）");
    CHECK(moved->keyframes.size() == 1 && moved->keyframes.first().t == 0.0
              && moved->keyframes.first().x == 110.0 && moved->keyframes.first().y == 190.0,
          "微调结果落在游标（t=0）处的新关键帧（+10, -10）");
    Transform movedDisplay;
    CHECK(core.displayTransform(actorId, movedDisplay)
              && movedDisplay.x == 110.0 && movedDisplay.y == 190.0,
          "当前时刻显示值 = 新关键帧值");
    CHECK(!core.moveSelectionBy(QPointF(0.0, 0.0), [] { return true; }),
          "零位移（值等于显示值）判定为无变更，不提交");
    CHECK(emissions == before + 1, "零位移不触发 fileDataChanged");

    // 删除：选中态随 fileDataChanged 自动剔除
    CHECK(core.deleteSelection(), "删除选中对象");
    CHECK(runtime.fileData().objects.size() == 3 && core.findObject(actorId) == nullptr,
          "删除提交回数据层");
    CHECK(core.selection()->isEmpty(), "删除后选中态自动剔除");
}

// 阶段3.5：含分组嵌套的存档往返 + 旧存档（无 parent 字段）加载兼容
void testParentGroupJson(QTemporaryDir& dir)
{
    // 分组 + 子对象 + 孙对象 + 顶层屏风，parentId 各不相同
    FileData nested;
    ObjectData group;
    group.id = QStringLiteral("group-001");
    group.type = ObjectType::Group;
    group.name = QStringLiteral("第一幕组");
    nested.objects.append(group);

    ObjectData actor;
    actor.id = QStringLiteral("actor-001");
    actor.type = ObjectType::Actor;
    actor.name = QStringLiteral("张三");
    actor.parentId = QStringLiteral("group-001");
    actor.color = QColor(QStringLiteral("#e74c3c"));
    actor.transform = { 300.0, 200.0, 45.0 };
    nested.objects.append(actor);

    ObjectData screen;
    screen.id = QStringLiteral("screen-001");
    screen.type = ObjectType::Screen;
    screen.name = QStringLiteral("屏风1");
    screen.parentId = QStringLiteral("actor-001"); // 孙对象（挂在演员下）
    screen.color = QColor(QStringLiteral("#888888"));
    screen.size = QSizeF(100.0, 10.0);
    screen.transform = { -150.0, 100.0, 90.0 };
    nested.objects.append(screen);

    const QString path = dir.filePath(QStringLiteral("nested.json"));
    const SaveResult saveResult = JsonStore::save(path, nested);
    CHECK(saveResult.ok, "保存含分组嵌套的存档：" + saveResult.message);
    const LoadResult loadResult = JsonStore::load(path);
    CHECK(loadResult.ok(), "读回含分组嵌套的存档：" + loadResult.message);
    if (loadResult.ok()) {
        CHECK(loadResult.data.objects == nested.objects,
              "含 parent/group 的存档往返逐字段一致");
        CHECK(loadResult.data == nested, "FileData 整体一致（含分组）");
    }

    // 写出始终包含 parent 字段与 group 类型字符串
    const QJsonObject root = JsonStore::toJson(nested);
    const QJsonArray writtenObjects = root.value(QStringLiteral("objects")).toArray();
    CHECK(!writtenObjects.isEmpty()
              && writtenObjects.first().toObject().contains(QStringLiteral("parent")),
          "写出始终包含 parent 字段");
    CHECK(writtenObjects.first().toObject().value(QStringLiteral("type"))
                  .toString()
              == QStringLiteral("group"),
          "group 类型序列化为 \"group\"");

    // 旧存档兼容：去掉所有 parent 字段后加载，全部视为顶层
    QJsonArray legacyObjects = writtenObjects;
    for (int i = 0; i < legacyObjects.size(); ++i) {
        QJsonObject object = legacyObjects.at(i).toObject();
        object.remove(QStringLiteral("parent"));
        legacyObjects[i] = object;
    }
    QJsonObject legacy = root;
    legacy[QStringLiteral("objects")] = legacyObjects;
    const QString legacyPath = dir.filePath(QStringLiteral("legacy.json"));
    writeTextFile(legacyPath, QJsonDocument(legacy).toJson());
    const LoadResult legacyResult = JsonStore::load(legacyPath);
    CHECK(legacyResult.ok(), "旧存档（无 parent 字段）加载兼容：" + legacyResult.message);
    if (legacyResult.ok()) {
        bool allTopLevel = true;
        for (const ObjectData& object : legacyResult.data.objects)
            allTopLevel = allTopLevel && object.parentId.isEmpty();
        CHECK(allTopLevel, "parent 缺省视为顶层");
        CHECK(legacyResult.data.objects.size() == nested.objects.size(),
              "旧存档对象数量一致");
    }

    // parent 字段类型非法 → 严格校验报错而非崩溃
    QJsonArray badObjects = writtenObjects;
    QJsonObject badObject = badObjects.first().toObject();
    badObject[QStringLiteral("parent")] = 42;
    badObjects[0] = badObject;
    QJsonObject bad = root;
    bad[QStringLiteral("objects")] = badObjects;
    const QString badPath = dir.filePath(QStringLiteral("badparent.json"));
    writeTextFile(badPath, QJsonDocument(bad).toJson());
    const LoadResult badResult = JsonStore::load(badPath);
    CHECK(!badResult.ok() && badResult.error == LoadError::InvalidData,
          "parent 字段类型非法返回 InvalidData");
}

// 阶段3.5：ReparentObjectsCommand apply/revert 往返（含排序变化）
void testReparentCommand()
{
    // DFS 顺序：甲, 乙, 组1(丙, 大屏), 丁
    FileData original;
    ObjectData a1; a1.id = QStringLiteral("actor-001"); a1.type = ObjectType::Actor;
    a1.name = QStringLiteral("甲"); a1.transform = { 100.0, 200.0, 0.0 };
    ObjectData a2; a2.id = QStringLiteral("actor-002"); a2.type = ObjectType::Actor;
    a2.name = QStringLiteral("乙"); a2.transform = { -100.0, 300.0, 0.0 };
    ObjectData g; g.id = QStringLiteral("group-001"); g.type = ObjectType::Group;
    g.name = QStringLiteral("组1");
    ObjectData a3; a3.id = QStringLiteral("actor-003"); a3.type = ObjectType::Actor;
    a3.name = QStringLiteral("丙"); a3.parentId = QStringLiteral("group-001");
    a3.transform = { 50.0, 60.0, 0.0 };
    ObjectData s; s.id = QStringLiteral("screen-001"); s.type = ObjectType::Screen;
    s.name = QStringLiteral("大屏"); s.parentId = QStringLiteral("actor-003");
    s.size = QSizeF(100.0, 10.0);
    ObjectData a4; a4.id = QStringLiteral("actor-004"); a4.type = ObjectType::Actor;
    a4.name = QStringLiteral("丁"); a4.transform = { 500.0, 500.0, 0.0 };
    original.objects = { a1, a2, g, a3, s, a4 };

    const auto idOrder = [](const FileData& data) {
        QList<QString> order;
        for (const ObjectData& object : data.objects)
            order.append(object.id);
        return order;
    };

    // 落到行上：甲 成为 组1 的子对象，排到子树末尾
    {
        FileData data = original;
        ReparentObjectsCommand cmd({ QStringLiteral("actor-001") },
                                   QStringLiteral("group-001"), 1);
        cmd.apply(data);
        CHECK(idOrder(data)
                  == QList<QString>({ QStringLiteral("actor-002"), QStringLiteral("group-001"),
                                      QStringLiteral("actor-003"), QStringLiteral("screen-001"),
                                      QStringLiteral("actor-001"), QStringLiteral("actor-004") }),
              "reparent 到行上：子对象排到该行子树末尾（DFS 顺序）");
        CHECK(data.objects.at(4).parentId == QStringLiteral("group-001"),
              "reparent 改写 parentId");
        cmd.revert(data);
        CHECK(data == original, "reparent revert 后 FileData 逐字段还原");
        cmd.apply(data); // redo 幂等
        CHECK(idOrder(data).at(4) == QStringLiteral("actor-001")
                  && data.objects.at(4).parentId == QStringLiteral("group-001"),
              "reparent 再次 apply（redo）结果一致");
    }

    // 顶层排序：丁 插到 乙 之前（Above）
    {
        FileData data = original;
        ReparentObjectsCommand cmd({ QStringLiteral("actor-004") }, QString(), 1);
        cmd.apply(data);
        CHECK(idOrder(data)
                  == QList<QString>({ QStringLiteral("actor-001"), QStringLiteral("actor-004"),
                                      QStringLiteral("actor-002"), QStringLiteral("group-001"),
                                      QStringLiteral("actor-003"), QStringLiteral("screen-001") }),
              "reparent 到顶层行间：同级插入排序");
        CHECK(data.objects.at(1).parentId.isEmpty(), "行间插入保持顶层 parentId 为空");
        cmd.revert(data);
        CHECK(data == original, "顶层排序 revert 还原");
    }

    // 整棵子树移出分组到顶层末尾（子树结构保持）
    {
        FileData data = original;
        ReparentObjectsCommand cmd({ QStringLiteral("actor-003") }, QString(), 4);
        cmd.apply(data);
        CHECK(idOrder(data)
                  == QList<QString>({ QStringLiteral("actor-001"), QStringLiteral("actor-002"),
                                      QStringLiteral("group-001"), QStringLiteral("actor-004"),
                                      QStringLiteral("actor-003"), QStringLiteral("screen-001") }),
              "整棵子树移出分组到顶层末尾");
        CHECK(data.objects.at(4).parentId.isEmpty(), "子树根改挂顶层");
        CHECK(data.objects.at(5).parentId == QStringLiteral("actor-003"),
              "子树内部结构保持（孙对象仍挂原父）");
        cmd.revert(data);
        CHECK(data == original, "子树移出 revert 还原");
    }
}

// 阶段3.5：RemoveObjectsCommand 级联删除整棵子树，revert 原位还原
void testCascadeRemove()
{
    FileData original;
    ObjectData a1; a1.id = QStringLiteral("actor-001"); a1.type = ObjectType::Actor;
    a1.name = QStringLiteral("甲");
    ObjectData g; g.id = QStringLiteral("group-001"); g.type = ObjectType::Group;
    g.name = QStringLiteral("组1");
    ObjectData a2; a2.id = QStringLiteral("actor-002"); a2.type = ObjectType::Actor;
    a2.name = QStringLiteral("乙"); a2.parentId = QStringLiteral("group-001");
    ObjectData a3; a3.id = QStringLiteral("actor-003"); a3.type = ObjectType::Actor;
    a3.name = QStringLiteral("丙"); a3.parentId = QStringLiteral("actor-002");
    ObjectData a4; a4.id = QStringLiteral("actor-004"); a4.type = ObjectType::Actor;
    a4.name = QStringLiteral("丁");
    original.objects = { a1, g, a2, a3, a4 };

    // 删除分组：级联删整棵子树（组1 + 乙 + 丙）
    {
        FileData data = original;
        RemoveObjectsCommand remove({ QStringLiteral("group-001") });
        remove.apply(data);
        CHECK(data.objects.size() == 2
                  && data.objects.at(0).id == QStringLiteral("actor-001")
                  && data.objects.at(1).id == QStringLiteral("actor-004"),
              "删除分组级联删除整棵子树");
        remove.revert(data);
        CHECK(data == original, "级联删除 revert 整棵子树还原（原位插回）");
    }

    // 删除中间节点：乙 + 其子 丙 级联
    {
        FileData data = original;
        RemoveObjectsCommand remove({ QStringLiteral("actor-002") });
        remove.apply(data);
        CHECK(data.objects.size() == 3
                  && !data.objects.at(1).id.contains(QStringLiteral("actor-002")),
              "删除中间节点级联其子对象");
        remove.revert(data);
        CHECK(data == original, "中间节点级联删除 revert 还原");
    }

    // 多选含祖先与后代：只删一次不重复
    {
        FileData data = original;
        RemoveObjectsCommand remove(
            { QStringLiteral("group-001"), QStringLiteral("actor-003") });
        remove.apply(data);
        CHECK(data.objects.size() == 2, "多选含祖先后代时级联去重");
        remove.revert(data);
        CHECK(data == original, "多选级联 revert 还原");
    }
}

// 阶段3.5：EditorCore 空对象创建 / reparent 校验 / 子树复制粘贴
void testEditorCoreGroupReparent()
{
    RuntimeData runtime;
    EditorCore core(&runtime);

    // ---- 空对象创建：自动命名/唯一性/顶层/无颜色大小/忽略位置 ----
    CHECK(core.createObject(ObjectType::Group, QPointF(123.0, 456.0)), "创建空对象成功");
    const QString group1 = runtime.fileData().objects.at(0).id;
    {
        const ObjectData object = runtime.fileData().objects.at(0);
        CHECK(object.id == QStringLiteral("group-001") && object.name == QStringLiteral("空对象1"),
              "空对象自动 id 与命名（空对象1）");
        CHECK(object.parentId.isEmpty(), "空对象创建落在顶层");
        CHECK(!object.color.isValid() && !object.size, "空对象无颜色/大小属性");
        CHECK(object.transform == Transform({ 0.0, 0.0, 0.0 }),
              "空对象忽略传入位置，transform 保留默认值");
        CHECK(core.selection()->singleSelectedId() == object.id, "创建空对象后自动选中");
    }
    CHECK(core.createObject(ObjectType::Group, QPointF(0.0, 0.0)), "创建第二个空对象");
    CHECK(runtime.fileData().objects.at(1).name == QStringLiteral("空对象2")
              && runtime.fileData().objects.at(1).id == QStringLiteral("group-002"),
          "空对象命名唯一性（空对象2）");
    const QString group2 = runtime.fileData().objects.at(1).id;

    // 空对象改名走同一路径（重名拒绝照旧）
    CHECK(!core.setObjectName(group1, QStringLiteral("空对象2")), "空对象重名拒绝");
    CHECK(core.setObjectName(group1, QStringLiteral("第一幕布景组")), "空对象改名生效");

    // ---- reparent：拖入分组 / 环校验 ----
    CHECK(core.createObject(ObjectType::Actor, QPointF(100.0, 100.0)), "创建演员");
    const QString actor1 = runtime.fileData().objects.at(2).id;
    CHECK(core.reparent({ actor1 }, group1, 0), "拖入分组成功");
    CHECK(core.findObject(actor1)->parentId == group1, "reparent 落数据 parentId");
    {
        // DFS 不变式：子对象紧跟父的子树区间 → [组1, 角色1, 空对象2]
        const QList<QString> order = { group1, actor1, group2 };
        QList<QString> actual;
        for (const ObjectData& object : runtime.fileData().objects)
            actual.append(object.id);
        CHECK(actual == order, "reparent 后列表顺序 = DFS 显示顺序");
    }
    CHECK(!core.reparent({ group1 }, actor1, 0), "环拖拽被拒绝（拖到自身后代之下）");
    CHECK(!core.reparent({ group1 }, group1, 0), "环拖拽被拒绝（拖到自身）");
    CHECK(!core.reparent({ group1 }, QStringLiteral("ghost-001"), 0), "父不存在拒绝");
    CHECK(!core.reparent({ QStringLiteral("ghost-001") }, group1, 0), "移动对象不存在拒绝");
    CHECK(core.findObject(group1)->parentId.isEmpty()
              && core.findObject(actor1)->parentId == group1,
          "拒绝的 reparent 不产生任何变更");

    // Prop 不出现：注入一个道具对象，双向拒绝
    {
        FileData injected = runtime.fileData();
        ObjectData prop;
        prop.id = QStringLiteral("prop-001");
        prop.type = ObjectType::Prop;
        prop.name = QStringLiteral("道具1");
        injected.objects.append(prop);
        runtime.commitFileData(injected);
        CHECK(!core.reparent({ QStringLiteral("prop-001") }, group1, 0), "Prop 拖动被拒绝");
        CHECK(!core.reparent({ actor1 }, QStringLiteral("prop-001"), 0), "拖到 Prop 之下被拒绝");
        FileData cleaned = runtime.fileData();
        cleaned.objects.removeLast();
        runtime.commitFileData(cleaned);
    }

    // ---- 子树复制/粘贴：整棵子树、新 id、名字唯一化、结构保持、粘贴到顶层 ----
    core.selection()->select(group1); // 组1 含 角色1
    core.copySelection();
    CHECK(core.pasteClipboard(), "粘贴分组子树成功");
    CHECK(runtime.fileData().objects.size() == 5, "整棵子树一起粘贴");
    {
        const ObjectData& pastedGroup = runtime.fileData().objects.at(3);
        const ObjectData& pastedActor = runtime.fileData().objects.at(4);
        CHECK(pastedGroup.type == ObjectType::Group
                  && pastedGroup.id != group1
                  && pastedGroup.parentId.isEmpty(),
              "粘贴的分组生成新 id 且落在顶层");
        CHECK(pastedActor.parentId == pastedGroup.id, "粘贴保持子树结构");
        CHECK(pastedActor.transform.x == 100.0 && pastedActor.transform.y == 100.0,
              "子对象保持绝对场景坐标（无偏移）");
        CHECK(pastedGroup.name == QStringLiteral("第一幕布景组2")
                  && pastedActor.name == QStringLiteral("角色2"),
              "粘贴名字唯一化");
        CHECK(core.selection()->selectedIds().contains(pastedGroup.id)
                  && core.selection()->selectedIds().contains(pastedActor.id),
              "粘贴后选中整棵副本");
    }

    // 单独复制子对象：粘贴后落顶层并偏移
    core.selection()->select(actor1);
    core.copySelection();
    CHECK(core.pasteClipboard(), "单独粘贴子对象成功");
    {
        const ObjectData& pasted = runtime.fileData().objects.last();
        CHECK(pasted.parentId.isEmpty(), "单独复制的子对象粘贴到顶层");
        CHECK(pasted.transform.x == 120.0 && pasted.transform.y == 120.0,
              "顶层粘贴偏移 (x+20, y+20)");
    }

    // ---- 删除分组级联（走 EditorCore 命令路径）----
    core.selection()->select(group1);
    CHECK(core.deleteSelection(), "删除分组");
    CHECK(core.findObject(group1) == nullptr && core.findObject(actor1) == nullptr,
          "删除分组级联删除子对象（数据层）");
    CHECK(core.selection()->isEmpty(), "级联删除后选中态剔除");
}

// 阶段4：UndoStack 基础行为（入栈/撤销/重做往返、无变更不入栈、
// 新命令清空 redo 区、新建/打开文件清栈、fileReplaced 信号时机）
void testUndoStack(QTemporaryDir& dir)
{
    RuntimeData runtime;
    EditorCore core(&runtime);

    int stackSignals = 0;
    QObject::connect(&core, &EditorCore::undoStackChanged, &core, [&stackSignals] { ++stackSignals; });
    int replaced = 0;
    QObject::connect(&runtime, &RuntimeData::fileReplaced, &runtime, [&replaced] { ++replaced; });

    // ---- 基础往返 ----
    CHECK(!core.canUndo() && !core.canRedo(), "撤销栈初始为空");
    CHECK(!core.undo() && !core.redo(), "空栈 undo/redo 返回 false");
    CHECK(core.createObject(ObjectType::Actor, QPointF(10.0, 20.0)), "创建演员（入栈）");
    const FileData afterCreate = runtime.fileData();
    CHECK(core.canUndo() && !core.canRedo(), "入栈后可撤销、不可重做");
    CHECK(stackSignals == 1, "入栈发出一次 undoStackChanged");

    CHECK(core.undo(), "撤销创建成功");
    CHECK(runtime.fileData().objects.isEmpty(), "撤销后对象移除");
    CHECK(!core.canUndo() && core.canRedo(), "撤销到底后不可撤销、可重做");
    CHECK(core.redo(), "重做创建成功");
    CHECK(runtime.fileData() == afterCreate, "重做后 FileData 逐字段还原");
    CHECK(core.canUndo() && !core.canRedo(), "重做后可再次撤销");

    // ---- 无变更命令不入栈 ----
    core.selection()->select(QStringLiteral("actor-001"));
    const int signalsBefore = stackSignals;
    CHECK(!core.moveSelectionBy(QPointF(0.0, 0.0), [] { return true; }), "零位移判定为无变更");
    CHECK(stackSignals == signalsBefore, "无变更命令不发栈信号");
    core.undo(); // 撤销掉创建：undo 区空、redo 区有"创建"
    CHECK(!core.canUndo() && core.canRedo(), "无变更命令不入栈（栈状态不受影响）");

    // ---- 新命令清空 redo 区 ----
    CHECK(core.createObject(ObjectType::Actor, QPointF(0.0, 0.0)), "新命令入栈（redo 区应清空）");
    CHECK(!core.canRedo(), "新命令清空 redo 区");
    CHECK(core.canUndo(), "新命令后可撤销");

    // ---- 新建文件清空栈 ----
    CHECK(replaced == 0, "此前无 fileReplaced（commit 不触发）");
    runtime.newFile();
    CHECK(replaced == 1, "newFile 发出 fileReplaced");
    CHECK(!core.canUndo() && !core.canRedo(), "newFile 清空撤销栈");
    CHECK(stackSignals > signalsBefore, "清栈发出 undoStackChanged");

    // ---- 打开文件清空栈（成功才发信号；saveFile 不发）----
    CHECK(core.createObject(ObjectType::Screen, QPointF(0.0, 0.0)), "打开前再造一条命令");
    CHECK(core.canUndo(), "打开前栈非空");
    const QString path = dir.filePath(QStringLiteral("undo-open.json"));
    CHECK(JsonStore::save(path, makeSampleData()).ok, "UndoStack 测试准备：写入示例存档");
    const LoadResult openResult = runtime.openFile(path);
    CHECK(openResult.ok(), "openFile 成功");
    CHECK(replaced == 2, "openFile 成功发出 fileReplaced");
    CHECK(!core.canUndo() && !core.canRedo(), "openFile 清空撤销栈");

    CHECK(core.createObject(ObjectType::Actor, QPointF(1.0, 1.0)), "打开后再造一条命令");
    const QString missingPath = dir.filePath(QStringLiteral("undo-missing.json"));
    CHECK(!runtime.openFile(missingPath).ok(), "openFile 失败");
    CHECK(replaced == 2, "openFile 失败不发 fileReplaced");
    CHECK(core.canUndo(), "openFile 失败不清空撤销栈");

    const SaveResult saveResult = runtime.saveFile(dir.filePath(QStringLiteral("undo-save.json")),
                                                   runtime.fileData());
    CHECK(saveResult.ok, "saveFile 成功");
    CHECK(replaced == 2, "saveFile 不发 fileReplaced");
    CHECK(core.canUndo(), "saveFile 不清空撤销栈");
}

// 阶段4 验收硬性要求：20 步确定性操作序列（覆盖移动/旋转/创建/删除/粘贴/改名/
// reparent/改颜色/改屏风大小），每步记录 FileData 快照；随后逐级撤销，每级断言
// FileData 与上一快照逐字段相等，直到回到初始状态；再逐级重做，每级与快照一致。
void testUndoRedoSequence()
{
    RuntimeData runtime;
    EditorCore core(&runtime);

    QList<FileData> snapshots;
    snapshots.append(runtime.fileData()); // 快照0：初始状态（新建默认空舞台）

    const auto step = [&runtime, &snapshots](bool ok, const QString& name) {
        CHECK(ok, QStringLiteral("序列 %1：命令提交成功").arg(name));
        snapshots.append(runtime.fileData());
    };
    // 游标时刻语义下取"当前显示值"作为旋转等新绝对值的基准（阶段5 起 transform 为静态值）
    const auto displayOf = [&core](const QString& id) {
        Transform value;
        core.displayTransform(id, value);
        return value;
    };
    const auto confirmYes = [] { return true; };

    // 1~3：创建 演员/屏风/演员
    step(core.createObject(ObjectType::Actor, QPointF(100.0, 100.0)), QStringLiteral("1 创建演员"));
    const QString actor1 = QStringLiteral("actor-001");
    step(core.createObject(ObjectType::Screen, QPointF(-300.0, 0.0)), QStringLiteral("2 创建屏风"));
    const QString screen1 = QStringLiteral("screen-001");
    step(core.createObject(ObjectType::Actor, QPointF(200.0, -100.0)), QStringLiteral("3 创建演员2"));
    const QString actor2 = QStringLiteral("actor-002");

    // 4~5：移动 / 旋转
    core.selection()->select(actor1);
    step(core.moveSelectionBy(QPointF(30.0, -20.0), confirmYes), QStringLiteral("4 移动"));
    step(core.setTransformsAtCursor(
             { { actor2, { displayOf(actor2).x, displayOf(actor2).y, 90.0 } } }, confirmYes),
         QStringLiteral("5 旋转"));

    // 6~8：改名 / 改颜色 / 改屏风大小
    step(core.setObjectName(actor1, QStringLiteral("主角")), QStringLiteral("6 改名"));
    step(core.setObjectColor(actor2, QColor(QStringLiteral("#123456"))), QStringLiteral("7 改颜色"));
    step(core.setScreenSize(screen1, QSizeF(250.0, 12.0)), QStringLiteral("8 改屏风大小"));

    // 9~10：复制粘贴 / 创建空对象
    core.selection()->select(actor1);
    core.copySelection();
    step(core.pasteClipboard(), QStringLiteral("9 粘贴"));
    step(core.createObject(ObjectType::Group, QPointF(0.0, 0.0)), QStringLiteral("10 创建空对象"));
    const QString group1 = QStringLiteral("group-001");

    // 11~12：reparent 拖入分组
    step(core.reparent({ actor1 }, group1, 0), QStringLiteral("11 reparent 入组"));
    step(core.reparent({ actor2 }, group1, 1), QStringLiteral("12 reparent 入组2"));

    // 13~15：删除副本 / 移动屏风 / 旋转屏风
    core.selection()->select(QStringLiteral("actor-003"));
    step(core.deleteSelection(), QStringLiteral("13 删除粘贴副本"));
    core.selection()->select(screen1);
    step(core.moveSelectionBy(QPointF(0.0, 40.0), confirmYes), QStringLiteral("14 移动屏风"));
    step(core.setTransformsAtCursor(
             { { screen1, { displayOf(screen1).x, displayOf(screen1).y, 45.0 } } }, confirmYes),
         QStringLiteral("15 旋转屏风"));

    // 16~18：再次粘贴 / 屏风改名 / reparent 移出分组到顶层
    step(core.pasteClipboard(), QStringLiteral("16 再次粘贴"));
    step(core.setObjectName(screen1, QStringLiteral("背景墙")), QStringLiteral("17 屏风改名"));
    step(core.reparent({ actor2 }, QString(), 0), QStringLiteral("18 reparent 移出分组"));

    // 19~20：删除分组级联 / 移动收尾
    core.selection()->select(group1);
    step(core.deleteSelection(), QStringLiteral("19 删除分组级联"));
    core.selection()->select(screen1);
    step(core.moveSelectionBy(QPointF(-15.0, 5.0), confirmYes), QStringLiteral("20 移动收尾"));

    CHECK(snapshots.size() == 21, "20 步序列共 21 份快照（含初始）");

    // 逐级撤销：第 i 步撤销后必须逐字段等于快照 i-1
    for (int i = 20; i >= 1; --i) {
        CHECK(core.canUndo(), QStringLiteral("撤销第 %1 步前可撤销").arg(i));
        CHECK(core.undo(), QStringLiteral("撤销第 %1 步返回 true").arg(i));
        CHECK(runtime.fileData() == snapshots.at(i - 1),
              QStringLiteral("撤销第 %1 步后 FileData 与快照 %2 逐字段一致").arg(i).arg(i - 1));
    }
    CHECK(!core.canUndo() && core.canRedo(), "撤销到底：回到初始状态， redo 区完整");
    CHECK(!core.undo(), "撤销到底后继续撤销返回 false");

    // 逐级重做：第 i 步重做后必须逐字段等于快照 i
    for (int i = 1; i <= 20; ++i) {
        CHECK(core.canRedo(), QStringLiteral("重做第 %1 步前可重做").arg(i));
        CHECK(core.redo(), QStringLiteral("重做第 %1 步返回 true").arg(i));
        CHECK(runtime.fileData() == snapshots.at(i),
              QStringLiteral("重做第 %1 步后 FileData 与快照一致").arg(i));
    }
    CHECK(!core.canRedo() && core.canUndo(), "重做到底：redo 区清空，撤销栈完整");
    CHECK(!core.redo(), "重做到底后继续重做返回 false");
}

// 阶段5：关键帧插值纯函数（位置直线匀速、朝向最短弧匀速、首前/末后保持、
// 无关键帧用静态值、单帧任意时刻等于该帧、乱序输入防御）
void testInterpolation()
{
    ObjectData object;
    object.id = QStringLiteral("actor-x");
    object.type = ObjectType::Actor;
    object.transform = { 7.0, 8.0, 9.0 };

    // 无关键帧 → 静态 transform
    CHECK(TimelinePlayer::interpolate(object, 3.0) == Transform({ 7.0, 8.0, 9.0 }),
          "插值：无关键帧用静态值");

    // 两帧：中点位置直线匀速、朝向线性
    object.keyframes = { { 0.0, 0.0, 0.0, 0.0 }, { 10.0, 100.0, 200.0, 40.0 } };
    CHECK(TimelinePlayer::interpolate(object, 5.0) == Transform({ 50.0, 100.0, 20.0 }),
          "插值：两帧中点位置直线匀速");
    CHECK(TimelinePlayer::interpolate(object, -1.0) == Transform({ 0.0, 0.0, 0.0 }),
          "插值：首帧前保持首帧值");
    CHECK(TimelinePlayer::interpolate(object, 11.0) == Transform({ 100.0, 200.0, 40.0 }),
          "插值：末帧后保持末帧值");
    CHECK(TimelinePlayer::interpolate(object, 10.0) == Transform({ 100.0, 200.0, 40.0 }),
          "插值：时刻恰在关键帧上取该帧");

    // 最短弧：350°→10° 走 +20°（而非 -340°）
    object.keyframes = { { 0.0, 0.0, 0.0, 350.0 }, { 10.0, 0.0, 0.0, 10.0 } };
    CHECK(TimelinePlayer::interpolate(object, 2.5).rotation == 355.0,
          "插值：最短弧 350°→10° 经 355°");
    CHECK(TimelinePlayer::interpolate(object, 5.0).rotation == 0.0,
          "插值：最短弧中点跨过 360° 归一化为 0°");
    // 反向最短弧：100°→340° 走 -120°
    object.keyframes = { { 0.0, 0.0, 0.0, 100.0 }, { 10.0, 0.0, 0.0, 340.0 } };
    CHECK(TimelinePlayer::interpolate(object, 5.0).rotation == 40.0,
          "插值：最短弧 100°→340° 中点 40°（走 -120°）");

    // 单帧任意时刻等于该帧
    object.keyframes = { { 3.0, 50.0, 60.0, 30.0 } };
    CHECK(TimelinePlayer::interpolate(object, 0.0) == Transform({ 50.0, 60.0, 30.0 })
              && TimelinePlayer::interpolate(object, 99.0) == Transform({ 50.0, 60.0, 30.0 }),
          "插值：单帧任意时刻等于该帧");

    // 乱序输入防御（手工编辑的存档）：排序后插值
    object.keyframes = { { 10.0, 100.0, 200.0, 40.0 }, { 0.0, 0.0, 0.0, 0.0 } };
    CHECK(TimelinePlayer::interpolate(object, 5.0) == Transform({ 50.0, 100.0, 20.0 }),
          "插值：乱序关键帧列表防御性排序");
}

// 阶段5：时间轴命令 apply/revert 往返（upsert 插入/更新/批量、删除、改时刻、
// 标签添加/改名/删除、时长自动延长及撤销还原）
void testKeyframeCommands()
{
    const FileData original = makeSampleData(); // actor-001 帧 @2/5.5/9，时长 90.5，2 标签

    // upsert 插入：保持按 t 有序
    {
        FileData data = original;
        UpsertKeyframesCommand cmd({ { QStringLiteral("actor-001"), { 4.0, 1.0, 2.0, 3.0 } } });
        cmd.apply(data);
        const QList<Keyframe>& keys = data.objects.at(0).keyframes;
        CHECK(keys.size() == 4 && keys.at(1).t == 4.0 && keys.at(1).x == 1.0,
              "upsert 插入并保持按 t 有序");
        CHECK(data.timeline.duration == 90.5, "落点在时长内不延长");
        cmd.revert(data);
        CHECK(data == original, "upsert 插入 revert 还原");
        cmd.apply(data); // redo 幂等
        CHECK(data.objects.at(0).keyframes.size() == 4, "upsert redo 幂等");
    }

    // upsert 更新：T 处已有帧 → 更新不新增
    {
        FileData data = original;
        UpsertKeyframesCommand cmd({ { QStringLiteral("actor-001"), { 2.0, 9.0, 9.0, 9.0 } } });
        cmd.apply(data);
        const QList<Keyframe>& keys = data.objects.at(0).keyframes;
        CHECK(keys.size() == 3 && keys.at(0) == Keyframe({ 2.0, 9.0, 9.0, 9.0 }),
              "upsert 更新 T 处已有帧（不新增）");
        cmd.revert(data);
        CHECK(data == original, "upsert 更新 revert 还原");
    }

    // 批量 upsert 一条命令（多对象 = 一步撤销）
    {
        FileData data = original;
        UpsertKeyframesCommand cmd(
            { { QStringLiteral("actor-001"), { 2.0, 1.0, 1.0, 1.0 } },
              { QStringLiteral("screen-001"), { 1.0, 2.0, 2.0, 2.0 } } });
        cmd.apply(data);
        CHECK(data.objects.at(0).keyframes.at(0).x == 1.0
                  && data.objects.at(1).keyframes.at(0).x == 2.0,
              "批量 upsert 同时作用于多对象");
        cmd.revert(data);
        CHECK(data == original, "批量 upsert revert 一并还原");
    }

    // 删除（含 revert 还原）
    {
        FileData data = original;
        RemoveKeyframesCommand cmd({ { QStringLiteral("actor-001"), 5.5 } });
        cmd.apply(data);
        const QList<Keyframe>& keys = data.objects.at(0).keyframes;
        CHECK(keys.size() == 2 && keys.at(0).t == 2.0 && keys.at(1).t == 9.0,
              "删除关键帧 apply 生效");
        CHECK(data.timeline.duration == 90.5, "删除不收缩时长");
        cmd.revert(data);
        CHECK(data == original, "删除关键帧 revert 还原");
    }

    // 改时刻：t 变化值不变；目标时刻已有帧被覆盖
    {
        FileData data = original;
        MoveKeyframeCommand cmd(QStringLiteral("actor-001"), 5.5, 4.0);
        cmd.apply(data);
        const QList<Keyframe>& keys = data.objects.at(0).keyframes;
        CHECK(keys.size() == 3 && keys.at(1).t == 4.0
                  && keys.at(1) == Keyframe({ 4.0, 400.0, 250.0, 90.0 }),
              "改时刻 apply：t 变化值不变且保持有序");
        cmd.revert(data);
        CHECK(data == original, "改时刻 revert 还原");
    }
    {
        FileData data = original;
        MoveKeyframeCommand cmd(QStringLiteral("actor-001"), 5.5, 2.0);
        cmd.apply(data);
        const QList<Keyframe>& keys = data.objects.at(0).keyframes;
        CHECK(keys.size() == 2 && keys.at(0) == Keyframe({ 2.0, 400.0, 250.0, 90.0 }),
              "改时刻覆盖目标时刻已有帧");
        cmd.revert(data);
        CHECK(data == original, "改时刻覆盖 revert 还原双方");
    }

    // 时长自动延长及撤销还原
    {
        FileData data = original;
        UpsertKeyframesCommand cmd({ { QStringLiteral("actor-001"), { 95.0, 0.0, 0.0, 0.0 } } });
        cmd.apply(data);
        CHECK(data.timeline.duration == 95.0, "关键帧超出时长自动延长");
        cmd.revert(data);
        CHECK(data == original, "时长延长随命令 revert 一并还原");
    }

    // 场次标签：添加（有序）/改名/删除往返
    {
        FileData data = original;
        AddSceneLabelCommand add(SceneLabel { 20.0, QStringLiteral("间幕") });
        add.apply(data);
        CHECK(data.timeline.labels.size() == 3
                  && data.timeline.labels.at(1).name == QStringLiteral("间幕"),
              "添加场次标签并按 t 有序插入");
        add.revert(data);
        CHECK(data == original, "添加标签 revert 还原");

        AddSceneLabelCommand addBeyond(SceneLabel { 100.0, QStringLiteral("加幕") });
        addBeyond.apply(data);
        CHECK(data.timeline.duration == 100.0, "标签超出时长自动延长");
        addBeyond.revert(data);
        CHECK(data == original, "标签延长 revert 还原");

        RenameSceneLabelCommand rename(0, QStringLiteral("序幕"));
        rename.apply(data);
        CHECK(data.timeline.labels.at(0).name == QStringLiteral("序幕"), "标签改名 apply");
        rename.revert(data);
        CHECK(data == original, "标签改名 revert 还原");

        RemoveSceneLabelCommand remove(1);
        remove.apply(data);
        CHECK(data.timeline.labels.size() == 1
                  && data.timeline.labels.at(0).name == QStringLiteral("第一幕"),
              "删除标签 apply");
        remove.revert(data);
        CHECK(data == original, "删除标签 revert 还原");
    }
}

// 时间轴交互完善：批量改时刻（MoveKeyframesCommand）、标签移动
// （MoveSceneLabelCommand）、EditorCore 关键帧级剪贴板往返
void testTimelineElementEditing()
{
    const FileData original = makeSampleData(); // actor-001 帧 @2/5.5/9，screen-001 帧 @1/7.25，时长 90.5，标签 @0/@45.25

    // 批量改时刻：多对象统一平移，一条命令一步撤销
    {
        FileData data = original;
        MoveKeyframesCommand cmd({ { QStringLiteral("actor-001"), 2.0 },
                                   { QStringLiteral("screen-001"), 1.0 } },
                                 1.5);
        cmd.apply(data);
        CHECK(data.objects.at(0).keyframes.at(0).t == 3.5
                  && data.objects.at(1).keyframes.at(0).t == 2.5,
              "批量改时刻：多对象统一平移");
        cmd.revert(data);
        CHECK(data == original, "批量改时刻 revert 还原");
        cmd.apply(data); // redo 幂等
        CHECK(data.objects.at(0).keyframes.at(0).t == 3.5, "批量改时刻 redo 幂等");
        cmd.revert(data);
    }

    // 目标时刻已有帧按 upsert 覆盖（5.5 → 2.0 覆盖原 @2 帧）
    {
        FileData data = original;
        MoveKeyframesCommand cmd({ { QStringLiteral("actor-001"), 5.5 } }, -3.5);
        cmd.apply(data);
        const QList<Keyframe>& keys = data.objects.at(0).keyframes;
        CHECK(keys.size() == 2 && keys.at(0) == Keyframe({ 2.0, 400.0, 250.0, 90.0 })
                  && keys.at(1).t == 9.0,
              "批量改时刻：覆盖目标时刻已有帧");
        cmd.revert(data);
        CHECK(data == original, "批量改时刻覆盖 revert 还原双方");
    }

    // 逐帧 clamp t≥0：大负 delta 钳到 0
    {
        FileData data = original;
        MoveKeyframesCommand cmd({ { QStringLiteral("actor-001"), 2.0 },
                                   { QStringLiteral("screen-001"), 1.0 } },
                                 -10.0);
        cmd.apply(data);
        CHECK(data.objects.at(0).keyframes.first().t == 0.0
                  && data.objects.at(1).keyframes.first().t == 0.0,
              "批量改时刻：逐帧 clamp t≥0");
        cmd.revert(data);
        CHECK(data == original, "clamp revert 还原");
    }

    // 同批两帧钳到同一时刻：按列表顺序后者覆盖前者（@5.5 的帧覆盖 @2 的帧）
    {
        FileData data = original;
        MoveKeyframesCommand cmd({ { QStringLiteral("actor-001"), 2.0 },
                                   { QStringLiteral("actor-001"), 5.5 } },
                                 -10.0);
        cmd.apply(data);
        const QList<Keyframe>& keys = data.objects.at(0).keyframes;
        CHECK(keys.size() == 2 && keys.at(0) == Keyframe({ 0.0, 400.0, 250.0, 90.0 })
                  && keys.at(1).t == 9.0,
              "批量改时刻：同批钳到同一时刻后者覆盖前者");
        cmd.revert(data);
        CHECK(data == original, "同批覆盖 revert 还原");
    }

    // 时长自动延长（沿用命令内部 ensureDuration）及撤销还原
    {
        FileData data = original;
        MoveKeyframesCommand cmd({ { QStringLiteral("actor-001"), 9.0 } }, 95.0);
        cmd.apply(data);
        CHECK(data.objects.at(0).keyframes.last().t == 104.0
                  && data.timeline.duration == 104.0,
              "批量改时刻：时长自动延长");
        cmd.revert(data);
        CHECK(data == original, "时长延长 revert 一并还原");
    }

    // 标签移动：按下标改 t、名不变、按 t 重排；时长自动延长
    {
        FileData data = original;
        MoveSceneLabelCommand cmd(0, 50.0);
        cmd.apply(data);
        CHECK(data.timeline.labels.size() == 2
                  && data.timeline.labels.at(0).name == QStringLiteral("第二幕")
                  && data.timeline.labels.at(1)
                         == SceneLabel({ 50.0, QStringLiteral("第一幕") }),
              "标签移动：改时刻后按 t 重排且名不变");
        cmd.revert(data);
        CHECK(data == original, "标签移动 revert 还原");
        cmd.apply(data); // redo 幂等（下标在 apply/revert 间稳定）
        CHECK(data.timeline.labels.at(1).t == 50.0, "标签移动 redo 幂等");
        cmd.revert(data);
    }
    {
        FileData data = original;
        MoveSceneLabelCommand cmd(1, 95.5);
        cmd.apply(data);
        CHECK(data.timeline.duration == 95.5, "标签移出时长自动延长");
        cmd.revert(data);
        CHECK(data == original, "标签移动时长 revert 还原");
    }

    // EditorCore 入口：无变更不入栈 / 空集拒绝
    {
        RuntimeData runtime;
        EditorCore core(&runtime);
        runtime.commitFileData(original);
        CHECK(!core.moveSceneLabel(0, 0.0), "moveSceneLabel 无变更不入栈");
        CHECK(core.moveSceneLabel(0, 10.0), "moveSceneLabel 生效");
        CHECK(runtime.fileData().timeline.labels.at(0)
                  == SceneLabel({ 10.0, QStringLiteral("第一幕") }),
              "moveSceneLabel 改时刻且重排");
        CHECK(core.undo() && runtime.fileData() == original, "moveSceneLabel 撤销还原");
        CHECK(!core.moveKeyframes({}, 1.0), "moveKeyframes 空集拒绝");
    }

    // 关键帧级剪贴板（EditorCore 持有，独立于对象剪贴板）
    {
        RuntimeData runtime;
        EditorCore core(&runtime);
        CHECK(core.createObject(ObjectType::Actor, QPointF(0.0, 0.0)), "关键帧剪贴板：创建演员");
        const QString id = QStringLiteral("actor-001");
        CHECK(core.upsertKeyframe(id, { 1.0, 10.0, 10.0, 0.0 }), "关键帧剪贴板：打帧 @1");
        CHECK(core.upsertKeyframe(id, { 3.0, 30.0, 30.0, 90.0 }), "关键帧剪贴板：打帧 @3");

        CHECK(!core.keyframeClipboardHasContent(), "关键帧剪贴板初始为空");
        core.copySelectedKeyframes({ { id, 1.0 }, { id, 3.0 } });
        CHECK(core.keyframeClipboardHasContent(), "复制选中关键帧进关键帧剪贴板");
        CHECK(!core.clipboardHasContent(), "关键帧复制不触碰对象剪贴板");

        // 游标处粘贴：复制集最小 t(=1) 对齐游标 5 → 落 @5/@7，保持相对间隔
        core.timelinePlayer()->seekTo(5.0);
        CHECK(core.pasteKeyframesAtCursor(), "游标处粘贴关键帧");
        const QList<Keyframe>& keys = core.findObject(id)->keyframes;
        CHECK(keys.size() == 4 && keys.at(2) == Keyframe({ 5.0, 10.0, 10.0, 0.0 })
                  && keys.at(3) == Keyframe({ 7.0, 30.0, 30.0, 90.0 }),
              "粘贴保持相对间隔（最小 t 对齐游标）");
        CHECK(core.undo() && core.findObject(id)->keyframes.size() == 2, "撤销粘贴还原");
        CHECK(core.redo() && core.findObject(id)->keyframes.size() == 4, "重做粘贴");

        // upsert 覆盖：游标 3 → delta=2 → @3 被覆盖为复制值
        core.timelinePlayer()->seekTo(3.0);
        CHECK(core.pasteKeyframesAtCursor(), "再次粘贴（目标时刻已有帧）");
        const QList<Keyframe>& keys2 = core.findObject(id)->keyframes;
        CHECK(keys2.size() == 4 && keys2.at(1) == Keyframe({ 3.0, 10.0, 10.0, 0.0 }),
              "粘贴 upsert 覆盖目标时刻已有帧");

        // 复制不存在的关键帧 → 剪贴板清空，粘贴返回 false
        core.copySelectedKeyframes({ { id, 99.0 } });
        CHECK(!core.keyframeClipboardHasContent() && !core.pasteKeyframesAtCursor(),
              "复制不存在的帧 → 剪贴板空、粘贴 false");

        // 目标对象已删除：条目跳过，无有效目标返回 false
        core.copySelectedKeyframes({ { id, 1.0 } });
        core.selection()->select(id);
        CHECK(core.deleteSelection(), "关键帧剪贴板：删除对象");
        CHECK(!core.pasteKeyframesAtCursor(), "目标对象已删除跳过 → 无有效目标返回 false");
    }
}

// 阶段5：打帧流程（需求第 20 条，业务层主导，确认回调模拟弹窗决策）
void testKeyframeWorkflow()
{
    RuntimeData runtime;
    EditorCore core(&runtime);
    runtime.settings().setKeyframePromptSuppressed(false); // 已知起点：未抑制

    CHECK(core.createObject(ObjectType::Actor, QPointF(100.0, 200.0)), "打帧流程：创建演员");
    const QString id = QStringLiteral("actor-001");
    TimelinePlayer* player = core.timelinePlayer();
    player->seekTo(2.0);

    // 无关键帧 + 未抑制：触发询问；选"否" → 零变更（不产生任何数据修改）
    bool confirmCalled = false;
    const FileData beforePrompt = runtime.fileData();
    CHECK(!core.setTransformsAtCursor({ { id, { 300.0, 400.0, 0.0 } } },
                                      [&confirmCalled] {
                                          confirmCalled = true;
                                          return false;
                                      }),
          "打帧流程：选\"否\"返回 false");
    CHECK(confirmCalled, "打帧流程：无关键帧时触发询问回调");
    CHECK(runtime.fileData() == beforePrompt, "打帧流程：选\"否\"零变更");

    // 选"是" → 在游标处生成关键帧；静态 transform 不变
    CHECK(core.setTransformsAtCursor({ { id, { 300.0, 400.0, 45.0 } } },
                                     [] { return true; }),
          "打帧流程：选\"是\"打帧");
    {
        const ObjectData* object = core.findObject(id);
        CHECK(object->keyframes.size() == 1
                  && object->keyframes.first() == Keyframe({ 2.0, 300.0, 400.0, 45.0 }),
              "打帧流程：关键帧落游标处");
        CHECK(object->transform == Transform({ 100.0, 200.0, 0.0 }), "打帧不改静态 transform");
        Transform shown;
        CHECK(core.displayTransform(id, shown) && shown == Transform({ 300.0, 400.0, 45.0 }),
              "打帧流程：当前时刻显示值 = 新帧");
    }

    // T 处已有帧 → 直接更新，不询问
    confirmCalled = false;
    CHECK(core.setTransformsAtCursor({ { id, { 350.0, 450.0, 90.0 } } },
                                     [&confirmCalled] {
                                         confirmCalled = true;
                                         return false;
                                     }),
          "打帧流程：已有帧直接更新");
    CHECK(!confirmCalled, "打帧流程：已有帧不询问");
    CHECK(core.findObject(id)->keyframes.size() == 1
              && core.findObject(id)->keyframes.first().x == 350.0,
          "打帧流程：已有帧更新不新增");

    // 其它时刻无帧 → 再次询问
    player->seekTo(5.0);
    confirmCalled = false;
    CHECK(core.setTransformsAtCursor({ { id, { 0.0, 0.0, 0.0 } } },
                                     [&confirmCalled] {
                                         confirmCalled = true;
                                         return true;
                                     }),
          "打帧流程：新时刻选\"是\"");
    CHECK(confirmCalled, "打帧流程：新时刻无帧触发询问");
    CHECK(core.findObject(id)->keyframes.size() == 2
              && core.findObject(id)->keyframes.at(1).t == 5.0,
          "打帧流程：新帧按 t 有序插入");

    // 已抑制 → 直接打帧不询问
    runtime.settings().setKeyframePromptSuppressed(true);
    player->seekTo(7.0);
    confirmCalled = false;
    CHECK(core.setTransformsAtCursor({ { id, { 1.0, 2.0, 3.0 } } },
                                     [&confirmCalled] {
                                         confirmCalled = true;
                                         return false;
                                     }),
          "打帧流程：抑制后直接打帧");
    CHECK(!confirmCalled, "打帧流程：抑制后不再询问");
    CHECK(core.findObject(id)->keyframes.size() == 3, "打帧流程：抑制后生成关键帧");
    runtime.settings().setKeyframePromptSuppressed(false);

    // 值等于当前显示值 → 无变更不提交（零位移拖拽语义）
    player->seekTo(7.0); // 显示值 = 刚打的帧 (1,2,3)
    CHECK(!core.setTransformsAtCursor({ { id, { 1.0, 2.0, 3.0 } } }, [] { return true; }),
          "打帧流程：值同显示值判定无变更");
    CHECK(core.findObject(id)->keyframes.size() == 3, "无变更不新增帧");

    // 打帧按钮（keyframeSelectionAtCursor）：以当前显示值 upsert，不弹窗
    core.selection()->select(id);
    CHECK(core.canKeyframeSelection(), "打帧按钮：单选演员可用");
    player->seekTo(4.0);
    Transform shown4;
    CHECK(core.displayTransform(id, shown4), "打帧按钮：取显示值");
    CHECK(core.keyframeSelectionAtCursor(), "打帧按钮：以显示值打帧");
    {
        const QList<Keyframe>& keys = core.findObject(id)->keyframes;
        CHECK(keys.size() == 4, "打帧按钮：新增一帧");
        bool matched = false;
        for (const Keyframe& key : keys) {
            if (key.t == 4.0)
                matched = Transform({ key.x, key.y, key.rotation }) == shown4;
        }
        CHECK(matched, "打帧按钮：帧值 = 当前时刻插值显示值");
    }
    CHECK(!core.keyframeSelectionAtCursor(), "打帧按钮：重复打同值判定无变更不入栈");

    // 打帧按钮可用性：空选/多选/空对象禁用
    core.selection()->clear();
    CHECK(!core.canKeyframeSelection(), "打帧按钮：空选禁用");
    CHECK(core.createObject(ObjectType::Group, QPointF(0.0, 0.0)), "打帧流程：创建空对象");
    const QString groupId = runtime.fileData().objects.last().id;
    CHECK(!core.canKeyframeSelection(), "打帧按钮：单选空对象禁用");
    core.selection()->setSelection({ id, groupId });
    CHECK(!core.canKeyframeSelection(), "打帧按钮：多选禁用");
    // 空对象无关键帧语义：变换入口跳过（批次为空 → false）
    CHECK(!core.setTransformsAtCursor({ { groupId, { 0.0, 0.0, 0.0 } } }, [] { return true; }),
          "打帧流程：空对象跳过打帧");

    // 方向键微调走同一流程（含询问回调）
    core.selection()->select(id);
    player->seekTo(9.0); // 无帧（末帧 @7）
    confirmCalled = false;
    CHECK(core.moveSelectionBy(QPointF(10.0, 0.0),
                               [&confirmCalled] {
                                   confirmCalled = true;
                                   return true;
                               }),
          "打帧流程：方向键微调打帧");
    CHECK(confirmCalled, "打帧流程：方向键微调触发询问");
    {
        const QList<Keyframe>& keys = core.findObject(id)->keyframes;
        // 显示基准 = 末帧 @7 (1,2,3)，+10x → (11,2,3)
        CHECK(keys.last() == Keyframe({ 9.0, 11.0, 2.0, 3.0 }),
              "打帧流程：微调以当前显示值为基准");
    }

    // 粘贴：关键帧位置随副本整体偏移 +20/+20（时刻/朝向不变）
    core.selection()->select(id);
    core.copySelection();
    CHECK(core.pasteClipboard(), "打帧流程：粘贴含关键帧对象");
    {
        const ObjectData& pasted = runtime.fileData().objects.last();
        CHECK(pasted.keyframes.size() == core.findObject(id)->keyframes.size(),
              "粘贴复制全部关键帧");
        bool offsetOk = true;
        const QList<Keyframe>& source = core.findObject(id)->keyframes;
        for (int i = 0; i < source.size(); ++i) {
            offsetOk = offsetOk && pasted.keyframes.at(i).t == source.at(i).t
                && pasted.keyframes.at(i).x == source.at(i).x + 20.0
                && pasted.keyframes.at(i).y == source.at(i).y + 20.0
                && pasted.keyframes.at(i).rotation == source.at(i).rotation;
        }
        CHECK(offsetOk, "粘贴：关键帧位置随副本偏移 +20/+20");
    }
}

// 阶段5 验收硬点（实施计划风险2）：撤销/重做关键帧命令后，游标处插值立即刷新；
// 播放 tick 驱动游标推进，到末尾自动停止
void testTimelineUndoRefresh()
{
    RuntimeData runtime;
    EditorCore core(&runtime);
    runtime.settings().setKeyframePromptSuppressed(true);

    CHECK(core.createObject(ObjectType::Actor, QPointF(10.0, 20.0)), "刷新：创建演员");
    const QString id = QStringLiteral("actor-001");
    TimelinePlayer* player = core.timelinePlayer();
    player->seekTo(5.0);
    CHECK(core.setTransformsAtCursor({ { id, { 110.0, 120.0, 30.0 } } }, [] { return true; }),
          "刷新：游标处打帧");

    int timeSignals = 0;
    QObject::connect(player, &TimelinePlayer::currentTimeChanged, player,
                     [&timeSignals](double) { ++timeSignals; });

    Transform value;
    CHECK(core.undo(), "刷新：撤销打帧");
    CHECK(timeSignals == 1, "刷新：undo 后 player 重算并发 currentTimeChanged");
    CHECK(core.displayTransform(id, value) && value == Transform({ 10.0, 20.0, 0.0 }),
          "刷新：撤销后游标处立即回到静态值");
    CHECK(core.redo(), "刷新：重做打帧");
    CHECK(timeSignals == 2, "刷新：redo 后 player 重算并发信号");
    CHECK(core.displayTransform(id, value) && value == Transform({ 110.0, 120.0, 30.0 }),
          "刷新：重做后游标处立即取插值结果");

    // 撤销"自动延长"后时长回退：游标越界被钳制
    player->seekTo(50.0);
    CHECK(core.setTransformsAtCursor({ { id, { 0.0, 0.0, 0.0 } } }, [] { return true; }),
          "刷新：t=50 打帧");
    CHECK(runtime.fileData().timeline.duration == 60.0, "刷新：t=50 未超出默认 60s");
    player->seekTo(60.0);
    // t=60 处显示值为末帧 @50 (0,0,0)，改 (5,5,5) 属真实变更
    CHECK(core.setTransformsAtCursor({ { id, { 5.0, 5.0, 5.0 } } }, [] { return true; }),
          "刷新：t=60 打帧（末后保持基准上的真实变更）");
    CHECK(runtime.fileData().timeline.duration == 60.0, "刷新：t=60 不延长时长");
    // 拖到 70s 延长时长
    CHECK(core.moveKeyframe(id, 60.0, 70.0), "刷新：关键帧拖到 70s");
    CHECK(runtime.fileData().timeline.duration == 70.0, "刷新：时长自动延长到 70s");
    player->seekTo(70.0);
    CHECK(core.undo(), "刷新：撤销改时刻（时长应一并还原）");
    CHECK(runtime.fileData().timeline.duration == 60.0, "刷新：撤销后时长还原 60s");
    CHECK(player->currentTime() == 60.0, "刷新：时长回退后游标钳制到 60s");

    // 播放 tick：时长调短到 0.2s，播放后自动停在末帧（真实事件循环驱动）
    FileData shortData = runtime.fileData();
    shortData.timeline.duration = 0.2;
    runtime.commitFileData(shortData);
    player->seekTo(0.0);
    player->play();
    CHECK(player->isPlaying(), "播放：开始播放");
    QEventLoop loop;
    QTimer::singleShot(500, &loop, &QEventLoop::quit);
    loop.exec();
    CHECK(!player->isPlaying(), "播放：到末尾自动停止");
    CHECK(qAbs(player->currentTime() - 0.2) < 1e-9, "播放：游标停在末帧");
    // 末尾再播放：从头开始
    player->play();
    CHECK(player->isPlaying() && player->currentTime() < 0.2, "播放：末尾再播放从头开始");
    player->pause();
}

void testRoundTrip(QTemporaryDir& dir)
{
    const FileData original = makeSampleData();
    const QString path = dir.filePath(QStringLiteral("roundtrip.json"));

    const SaveResult saveResult = JsonStore::save(path, original);
    CHECK(saveResult.ok, "保存示例存档：" + saveResult.message);

    const LoadResult loadResult = JsonStore::load(path);
    CHECK(loadResult.ok(), "读回示例存档：" + loadResult.message);
    if (!loadResult.ok())
        return;

    const FileData& loaded = loadResult.data;
    CHECK(loaded.version == kFileDataVersion, "version 字段一致");
    CHECK(loaded.stage == original.stage, "StageData 逐字段一致");
    CHECK(loaded.objects.size() == 2, "对象数量一致（演员+屏风）");
    CHECK(loaded.objects == original.objects, "ObjectData（含关键帧、屏风 size）逐字段一致");
    CHECK(loaded.prepAreas.size() == 2, "准备区数量一致");
    CHECK(loaded.prepAreas == original.prepAreas, "PrepAreaData（id/name/x/y/宽高/color）逐字段一致");
    CHECK(loaded.prepAreas.first().color == QColor(QStringLiteral("#d35400")),
          "准备区 color 往返一致（显式色 → \"#rrggbb\"）");
    CHECK(!loaded.prepAreas.at(1).color.isValid(),
          "准备区 color 往返一致（派生态 → 空串 → invalid）");
    CHECK(loaded.paths.size() == 1, "路径数量一致");
    CHECK(loaded.paths == original.paths,
          "PathData（object/from/to/type/points/legAngles）逐字段一致");
    CHECK(loaded.timeline == original.timeline, "TimelineData（含场次标签）逐字段一致");
    CHECK(loaded == original, "FileData 整体一致");
}

void testCorruptedFile(QTemporaryDir& dir)
{
    const QString path = dir.filePath(QStringLiteral("corrupted.json"));
    CHECK(writeTextFile(path, "{ this is not valid json !!!"), "写入损坏测试文件");

    const LoadResult result = JsonStore::load(path);
    CHECK(!result.ok() && result.error == LoadError::Parse,
          "损坏 JSON 返回 LoadError::Parse 而非崩溃（message：" + result.message + "）");

    // 根节点不是对象
    const QString arrayPath = dir.filePath(QStringLiteral("array.json"));
    writeTextFile(arrayPath, "[1, 2, 3]");
    const LoadResult arrayResult = JsonStore::load(arrayPath);
    CHECK(!arrayResult.ok() && arrayResult.error == LoadError::Parse, "根节点为数组返回 LoadError::Parse");
}

// 阶段12 改写：版本受理区间 [1, 4]——v5（未来版本）拒载；v1 存档（无 prepAreas
// 节点）宽容迁移为空列表；v2 存档（prepAreas 元素无 color 键）宽容迁移为派生态；
// v3 存档（无 paths 节点）宽容迁移为空路径列表（仿 testParentGroupJson 的剥字段
// 兼容模式）
void testVersionMismatch(QTemporaryDir& dir)
{
    QJsonObject root = JsonStore::toJson(FileData::createDefault());

    root[QStringLiteral("version")] = 5;
    const QString futurePath = dir.filePath(QStringLiteral("v5.json"));
    writeTextFile(futurePath, QJsonDocument(root).toJson());
    const LoadResult futureResult = JsonStore::load(futurePath);
    CHECK(!futureResult.ok() && futureResult.error == LoadError::UnsupportedVersion,
          "version=5（高于当前）返回 LoadError::UnsupportedVersion");

    root[QStringLiteral("version")] = 0;
    const QString lowPath = dir.filePath(QStringLiteral("v0.json"));
    writeTextFile(lowPath, QJsonDocument(root).toJson());
    const LoadResult lowResult = JsonStore::load(lowPath);
    CHECK(!lowResult.ok() && lowResult.error == LoadError::UnsupportedVersion,
          "version=0（低于受理区间）返回 LoadError::UnsupportedVersion");

    root.remove(QStringLiteral("version"));
    const QString missingPath = dir.filePath(QStringLiteral("noversion.json"));
    writeTextFile(missingPath, QJsonDocument(root).toJson());
    const LoadResult missingResult = JsonStore::load(missingPath);
    CHECK(!missingResult.ok() && missingResult.error == LoadError::UnsupportedVersion,
          "version 缺失返回 LoadError::UnsupportedVersion");

    // v1 存档迁移：真实构造一份无 prepAreas/paths 节点的 JSON（toJson 恒写这两个
    // 节点，须剥掉模拟 v1 存档）
    QJsonObject v1 = JsonStore::toJson(makeSampleData());
    v1[QStringLiteral("version")] = 1;
    v1.remove(QStringLiteral("prepAreas"));
    v1.remove(QStringLiteral("paths"));
    const QString v1Path = dir.filePath(QStringLiteral("v1-legacy.json"));
    writeTextFile(v1Path, QJsonDocument(v1).toJson());
    const LoadResult v1Result = JsonStore::load(v1Path);
    CHECK(v1Result.ok(), "v1 存档（无 prepAreas/paths 节点）加载成功：" + v1Result.message);
    if (v1Result.ok()) {
        CHECK(v1Result.data.version == kFileDataVersion, "v1 迁移后版本号升到当前版本");
        CHECK(v1Result.data.prepAreas.isEmpty(), "v1 迁移：prepAreas 缺省为空列表");
        CHECK(v1Result.data.paths.isEmpty(), "v1 迁移：paths 缺省为空列表");
        CHECK(v1Result.data.objects == makeSampleData().objects,
              "v1 迁移：对象数据不受影响");
    }

    // v2 存档迁移：prepAreas 元素无 color 键（剥字段模式），加载成功且颜色均为
    // invalid（派生态，视觉零变化）；无 paths 节点 → 空列表
    QJsonObject v2 = JsonStore::toJson(makeSampleData());
    v2[QStringLiteral("version")] = 2;
    v2.remove(QStringLiteral("paths"));
    QJsonArray v2Areas = v2.value(QStringLiteral("prepAreas")).toArray();
    for (int i = 0; i < v2Areas.size(); ++i) {
        QJsonObject area = v2Areas.at(i).toObject();
        area.remove(QStringLiteral("color"));
        v2Areas[i] = area;
    }
    v2[QStringLiteral("prepAreas")] = v2Areas;
    const QString v2Path = dir.filePath(QStringLiteral("v2-legacy.json"));
    writeTextFile(v2Path, QJsonDocument(v2).toJson());
    const LoadResult v2Result = JsonStore::load(v2Path);
    CHECK(v2Result.ok(), "v2 存档（prepAreas 无 color 键）加载成功：" + v2Result.message);
    if (v2Result.ok()) {
        CHECK(v2Result.data.version == kFileDataVersion, "v2 迁移后版本号升到当前版本");
        CHECK(v2Result.data.prepAreas.size() == 2, "v2 迁移：准备区数量不变");
        bool allDerived = true;
        for (const PrepAreaData& area : v2Result.data.prepAreas)
            allDerived = allDerived && !area.color.isValid();
        CHECK(allDerived, "v2 迁移：准备区 color 缺省为 invalid（派生态）");
        CHECK(v2Result.data.paths.isEmpty(), "v2 迁移：paths 缺省为空列表");
        const QList<PrepAreaData> sampleAreas = makeSampleData().prepAreas;
        CHECK(v2Result.data.prepAreas.first().x == sampleAreas.first().x
                  && v2Result.data.prepAreas.first().width == sampleAreas.first().width,
              "v2 迁移：准备区其余字段不受影响");
    }

    // v3 存档迁移：无 paths 节点（剥字段模式）→ 加载成功、空路径列表、其余不变
    QJsonObject v3 = JsonStore::toJson(makeSampleData());
    v3[QStringLiteral("version")] = 3;
    v3.remove(QStringLiteral("paths"));
    const QString v3Path = dir.filePath(QStringLiteral("v3-legacy.json"));
    writeTextFile(v3Path, QJsonDocument(v3).toJson());
    const LoadResult v3Result = JsonStore::load(v3Path);
    CHECK(v3Result.ok(), "v3 存档（无 paths 节点）加载成功：" + v3Result.message);
    if (v3Result.ok()) {
        CHECK(v3Result.data.version == kFileDataVersion, "v3 迁移后版本号升到当前版本");
        CHECK(v3Result.data.paths.isEmpty(), "v3 迁移：paths 缺省为空列表（全部直线）");
        CHECK(v3Result.data.prepAreas == makeSampleData().prepAreas,
              "v3 迁移：准备区数据不受影响");
    }

    // 显式空串 color 同样宽容为 invalid（恒写空串的派生态存档读回路径）
    QJsonObject emptyColor = JsonStore::toJson(makeSampleData());
    QJsonArray emptyAreas = emptyColor.value(QStringLiteral("prepAreas")).toArray();
    for (int i = 0; i < emptyAreas.size(); ++i) {
        QJsonObject area = emptyAreas.at(i).toObject();
        area[QStringLiteral("color")] = QString();
        emptyAreas[i] = area;
    }
    emptyColor[QStringLiteral("prepAreas")] = emptyAreas;
    const QString emptyPath = dir.filePath(QStringLiteral("emptycolor.json"));
    writeTextFile(emptyPath, QJsonDocument(emptyColor).toJson());
    const LoadResult emptyResult = JsonStore::load(emptyPath);
    CHECK(emptyResult.ok(), "显式空串 color 的存档加载成功：" + emptyResult.message);
    if (emptyResult.ok()) {
        bool allDerived = true;
        for (const PrepAreaData& area : emptyResult.data.prepAreas)
            allDerived = allDerived && !area.color.isValid();
        CHECK(allDerived, "空串 color 解析为 invalid（派生态）");
    }

    // 非法颜色字符串宽松得 invalid，不报错（沿 ObjectData 颜色先例）
    QJsonObject badColor = JsonStore::toJson(makeSampleData());
    QJsonArray badColorAreas = badColor.value(QStringLiteral("prepAreas")).toArray();
    {
        QJsonObject area = badColorAreas.first().toObject();
        area[QStringLiteral("color")] = QStringLiteral("not-a-color");
        badColorAreas[0] = area;
    }
    badColor[QStringLiteral("prepAreas")] = badColorAreas;
    const QString badColorPath = dir.filePath(QStringLiteral("badcolor.json"));
    writeTextFile(badColorPath, QJsonDocument(badColor).toJson());
    const LoadResult badColorResult = JsonStore::load(badColorPath);
    CHECK(badColorResult.ok()
              && !badColorResult.data.prepAreas.first().color.isValid(),
          "非法颜色字符串宽松得 invalid（不报错）");

    // prepAreas 字段非法 → 严格校验报错而非崩溃
    QJsonObject badPrep = JsonStore::toJson(FileData::createDefault());
    QJsonArray badAreas;
    QJsonObject badArea;
    badArea[QStringLiteral("id")] = QStringLiteral("prep-001");
    badArea[QStringLiteral("name")] = QStringLiteral("准备区1");
    badArea[QStringLiteral("x")] = 0.0;
    badArea[QStringLiteral("y")] = 0.0;
    badArea[QStringLiteral("width")] = -100.0; // 宽 ≤ 0 非法
    badArea[QStringLiteral("height")] = 300.0;
    badAreas.append(badArea);
    badPrep[QStringLiteral("prepAreas")] = badAreas;
    const QString badPath = dir.filePath(QStringLiteral("badprep.json"));
    writeTextFile(badPath, QJsonDocument(badPrep).toJson());
    const LoadResult badResult = JsonStore::load(badPath);
    CHECK(!badResult.ok() && badResult.error == LoadError::InvalidData,
          "准备区宽 ≤ 0 返回 LoadError::InvalidData");

    QJsonObject notArray = JsonStore::toJson(FileData::createDefault());
    notArray[QStringLiteral("prepAreas")] = 42;
    const QString notArrayPath = dir.filePath(QStringLiteral("notarray.json"));
    writeTextFile(notArrayPath, QJsonDocument(notArray).toJson());
    const LoadResult notArrayResult = JsonStore::load(notArrayPath);
    CHECK(!notArrayResult.ok() && notArrayResult.error == LoadError::InvalidData,
          "prepAreas 非数组返回 LoadError::InvalidData");
}

void testMissingFile(QTemporaryDir& dir)
{
    const LoadResult result = JsonStore::load(dir.filePath(QStringLiteral("does-not-exist.json")));
    CHECK(!result.ok() && result.error == LoadError::Io, "文件不存在返回 LoadError::Io");
}

void testInvalidStructure(QTemporaryDir& dir)
{
    QJsonObject root = JsonStore::toJson(FileData::createDefault());
    root.remove(QStringLiteral("stage"));
    const QString path = dir.filePath(QStringLiteral("nostage.json"));
    writeTextFile(path, QJsonDocument(root).toJson());

    const LoadResult result = JsonStore::load(path);
    CHECK(!result.ok() && result.error == LoadError::InvalidData,
          "缺少 stage 节点返回 LoadError::InvalidData（message：" + result.message + "）");
}

void testSettingsPersistence(QCoreApplication& app)
{
    // 从已知状态开始
    {
        QSettings settings;
        settings.clear();
    }

    // 默认值
    {
        AppSettings settings;
        CHECK(settings.gridSnapEnabled() == AppSettings::defaultGridSnapEnabled,
              "网格吸附默认取独立常量（开）");
        CHECK(settings.gridVisible() == AppSettings::defaultGridVisible,
              "网格可见性默认取独立常量（开）");
        CHECK(settings.keyframePromptSuppressed() == AppSettings::defaultKeyframePromptSuppressed,
              "“打帧提示不再提示”默认取独立常量（关）");
        CHECK(settings.stageColor() == AppSettings::defaultStageColor(),
              "舞台颜色默认取独立常量（比网格深、比背景浅）");
        CHECK(settings.gridSizeCm() == AppSettings::defaultGridSizeCm,
              "网格间距默认取独立常量（100cm）");
        CHECK(settings.snapRangeCm() == AppSettings::defaultSnapRangeCm,
              "网格吸附距离默认取独立常量（20cm）");

        // 非正值防御性拒绝（网格间距参与取模运算，除零保护）
        settings.setGridSizeCm(0.0);
        settings.setSnapRangeCm(-3.0);
        CHECK(settings.gridSizeCm() == AppSettings::defaultGridSizeCm
                  && settings.snapRangeCm() == AppSettings::defaultSnapRangeCm,
              "网格间距/吸附距离：非正值写入被防御性拒绝");
    }

    // 写入哨兵值
    {
        AppSettings settings;
        settings.setGridSnapEnabled(false);
        settings.setGridVisible(false);
        settings.setKeyframePromptSuppressed(true);
        settings.setWindowGeometry(QByteArray("selftest-geometry-7f3a"));
        settings.setWindowState(QByteArray("selftest-state-9b2c"));
        settings.setStageColor(QColor(QStringLiteral("#123456")));
        settings.setGridSizeCm(55.5);
        settings.setSnapRangeCm(12.5);
    }

    // 同进程新实例读回
    {
        AppSettings settings;
        CHECK(!settings.gridSnapEnabled(), "同进程重建实例读回 gridSnapEnabled=false");
        CHECK(!settings.gridVisible(), "同进程重建实例读回 gridVisible=false");
        CHECK(settings.keyframePromptSuppressed(), "同进程重建实例读回 keyframePromptSuppressed=true");
        CHECK(settings.windowGeometry() == QByteArray("selftest-geometry-7f3a"), "同进程重建实例读回 windowGeometry");
        CHECK(settings.stageColor() == QColor(QStringLiteral("#123456")), "同进程重建实例读回 stageColor");
        CHECK(settings.gridSizeCm() == 55.5, "同进程重建实例读回 gridSizeCm");
        CHECK(settings.snapRangeCm() == 12.5, "同进程重建实例读回 snapRangeCm");
    }

    // 跨进程读回：子进程以 --verify-settings 模式启动自身
    QProcess child;
    child.start(app.applicationFilePath(), { QStringLiteral("--verify-settings") });
    const bool finished = child.waitForFinished(30000);
    CHECK(finished && child.exitStatus() == QProcess::NormalExit && child.exitCode() == 0,
          "QSettings 跨进程持久化（子进程读回哨兵值）");

    // 清理，不留下测试设置
    {
        QSettings settings;
        settings.clear();
    }
}

void testRuntimeData(QTemporaryDir& dir)
{
    RuntimeData runtime;
    CHECK(runtime.fileData() == FileData::createDefault(),
          "RuntimeData 初始为新建默认值（1200x800 / offset 0 / 60s / 空列表）");
    CHECK(runtime.currentFilePath().isEmpty(), "RuntimeData 初始无文件路径");

    int emissions = 0;
    QObject::connect(&runtime, &RuntimeData::fileDataChanged, &runtime, [&emissions] { ++emissions; });

    const FileData sample = makeSampleData();
    const QString path = dir.filePath(QStringLiteral("runtime.json"));
    const SaveResult saveResult = JsonStore::save(path, sample);
    CHECK(saveResult.ok, "RuntimeData 测试准备：写入示例存档");

    const LoadResult openResult = runtime.openFile(path);
    CHECK(openResult.ok() && runtime.fileData() == sample && runtime.currentFilePath() == path,
          "RuntimeData::openFile 成功并替换只读数据");
    CHECK(emissions == 1, "openFile 发出一次 fileDataChanged");

    const QString missingPath = dir.filePath(QStringLiteral("runtime-missing.json"));
    const LoadResult failedOpen = runtime.openFile(missingPath);
    CHECK(!failedOpen.ok() && failedOpen.error == LoadError::Io && runtime.fileData() == sample,
          "openFile 失败时保留原数据");
    CHECK(emissions == 1, "openFile 失败不发信号");

    const QString saveAsPath = dir.filePath(QStringLiteral("runtime-saveas.json"));
    const SaveResult saveAsResult = runtime.saveFile(saveAsPath, sample);
    CHECK(saveAsResult.ok && runtime.currentFilePath() == saveAsPath,
          "RuntimeData::saveFile（业务层显式触发）成功并更新路径");
    CHECK(emissions == 2, "saveFile 成功发出 fileDataChanged");

    runtime.newFile();
    CHECK(runtime.fileData() == FileData::createDefault() && runtime.currentFilePath().isEmpty(),
          "RuntimeData::newFile 重置为默认值");
    CHECK(emissions == 3, "newFile 发出 fileDataChanged");
}

// 阶段6：dirty 跟踪——快照内容比较（撤销回已存内容 = 干净），
// dirtyChanged 只在状态翻转时发；newFile/openFile/saveFile 成功后干净
void testDirtyTracking(QTemporaryDir& dir)
{
    RuntimeData runtime;
    CHECK(!runtime.isDirty(), "dirty：初始（新建默认）不脏");

    int dirtySignals = 0;
    QObject::connect(&runtime, &RuntimeData::dirtyChanged, &runtime,
                     [&dirtySignals] { ++dirtySignals; });

    const FileData sample = makeSampleData();
    runtime.commitFileData(sample);
    CHECK(runtime.isDirty(), "dirty：commit 后变脏");
    CHECK(dirtySignals == 1, "dirty：翻转时发一次 dirtyChanged");

    FileData touched = sample;
    touched.timeline.duration = 120.0;
    runtime.commitFileData(touched);
    CHECK(runtime.isDirty() && dirtySignals == 1, "dirty：持续脏不重复发信号");

    const QString path = dir.filePath(QStringLiteral("dirty.json"));
    CHECK(runtime.saveFile(path, runtime.fileData()).ok, "dirty：saveFile 成功");
    CHECK(!runtime.isDirty() && dirtySignals == 2, "dirty：保存后干净并发信号");

    runtime.commitFileData(sample); // sample ≠ 已存快照（touched）→ 脏
    CHECK(runtime.isDirty() && dirtySignals == 3, "dirty：保存后再编辑变脏");

    runtime.commitFileData(touched); // 内容回到已存快照（模拟撤销回保存点）
    CHECK(!runtime.isDirty() && dirtySignals == 4,
          "dirty：内容回到已存快照即干净（撤销跨保存点按内容比较）");

    runtime.commitFileData(sample); // 再脏（信号 5）
    const LoadResult openResult = runtime.openFile(path); // 快照为 touched
    CHECK(openResult.ok() && !runtime.isDirty() && dirtySignals == 6,
          "dirty：openFile 成功后干净");

    runtime.commitFileData(sample); // 再脏（信号 7）
    runtime.newFile();
    CHECK(!runtime.isDirty() && dirtySignals == 8, "dirty：newFile 后干净");

    // openFile 失败不影响 dirty 状态
    runtime.commitFileData(sample); // 脏（信号 9）
    const LoadResult failed =
        runtime.openFile(dir.filePath(QStringLiteral("dirty-missing.json")));
    CHECK(!failed.ok() && runtime.isDirty() && dirtySignals == 9,
          "dirty：openFile 失败保持脏状态且不发信号");
}

// 阶段7：EditorCore::setStage 直改直提交语义——合法修改生效（commitFileData
// 提交回数据层）、宽/深 ≤ 0 拒绝、无变更不提交、不产生撤销条目、undo 不影响
// 舞台值（SetStageCommand 已随阶段7 移除，舞台不入撤销栈）
void testStageSettings()
{
    const FileData original = makeSampleData(); // stage {1500, 900, 50}

    RuntimeData runtime;
    EditorCore core(&runtime);
    runtime.commitFileData(original);
    const StageData current = runtime.fileData().stage;

    int emissions = 0;
    QObject::connect(&runtime, &RuntimeData::fileDataChanged, &runtime,
                     [&emissions] { ++emissions; });

    // 无变更不提交
    CHECK(!core.setStage(current), "setStage 无变更不提交（返回 false）");
    CHECK(emissions == 0 && !core.canUndo(),
          "setStage 无变更：不发 fileDataChanged、不动撤销栈");

    // 非法尺寸拒绝
    CHECK(!core.setStage(StageData { 0.0, 800.0, 0.0 }), "setStage 拒绝宽 ≤ 0");
    CHECK(!core.setStage(StageData { 1200.0, -5.0, 0.0 }), "setStage 拒绝深 ≤ 0");
    CHECK(runtime.fileData().stage == current && emissions == 0 && !core.canUndo(),
          "setStage 非法尺寸：不改数据、不发信号、不动撤销栈");

    // 合法修改生效：直改直提交，不入撤销栈
    CHECK(core.setStage(StageData { 2400.0, 1800.0, -250.0 }), "setStage 合法修改生效");
    CHECK(runtime.fileData().stage == StageData({ 2400.0, 1800.0, -250.0 }),
          "setStage 提交回数据层");
    CHECK(emissions == 1, "setStage 发出一次 fileDataChanged");
    CHECK(!core.canUndo(), "setStage 不产生撤销条目（canUndo 不变）");

    // centerOffset 任意 double（负值合法）
    CHECK(core.setStage(StageData { 2400.0, 1800.0, -999.5 }), "setStage：centerOffset 负值合法");
    CHECK(!core.canUndo(), "setStage 再次直改仍无撤销条目");

    // undo 不影响舞台值：对象命令入栈 → 改舞台 → undo 只撤对象
    CHECK(core.createObject(ObjectType::Actor, QPointF(0.0, 0.0)), "setStage 测试：创建演员入撤销栈");
    CHECK(core.canUndo(), "setStage 测试：对象命令可撤销");
    CHECK(core.setStage(StageData { 3000.0, 2000.0, 10.0 }), "setStage 测试：入栈后再改舞台");
    CHECK(core.undo(), "setStage 测试：undo 撤掉对象命令");
    CHECK(runtime.fileData().objects.size() == original.objects.size(),
          "setStage 测试：undo 移除新建对象（回到 original 的 2 个对象）");
    CHECK(runtime.fileData().stage == StageData({ 3000.0, 2000.0, 10.0 }),
          "setStage 测试：undo 不影响舞台值");
    CHECK(!core.canUndo(), "setStage 测试：撤销后栈空（无舞台条目残留）");
}

// 阶段10：台下准备区——EditorCore::setPrepAreas/addPrepArea/removePrepArea
// 与 setStage 同例的直改直提交语义：校验拒绝（宽高 ≤ 0、id/name 重复）、
// 自动命名唯一递增、不入撤销栈、dirty 置位、undo 不影响准备区；
// JsonStore 迁移与往返由 testVersionMismatch/testRoundTrip 覆盖
void testPrepAreas()
{
    CHECK(FileData::createDefault().prepAreas.isEmpty(),
          "createDefault：准备区为空列表");

    RuntimeData runtime;
    EditorCore core(&runtime);

    int emissions = 0;
    QObject::connect(&runtime, &RuntimeData::fileDataChanged, &runtime,
                     [&emissions] { ++emissions; });

    // ---- addPrepArea：自动 id/命名递增、默认 400×300、落点正确 ----
    const QString id1 = core.addPrepArea(-1050.0, 450.0);
    CHECK(!id1.isEmpty(), "addPrepArea 返回新 id");
    CHECK(runtime.fileData().prepAreas.size() == 1, "addPrepArea 经直改提交回数据层");
    CHECK(emissions == 1, "addPrepArea 发出一次 fileDataChanged");
    {
        const PrepAreaData& area = runtime.fileData().prepAreas.first();
        CHECK(area.id == id1 && area.id == QStringLiteral("prep-001"),
              "addPrepArea 生成 prep-NNN 格式 id");
        CHECK(area.name == QStringLiteral("准备区1"), "addPrepArea 自动命名（准备区1）");
        CHECK(area.width == 400.0 && area.height == 300.0, "addPrepArea 默认 400×300");
        CHECK(area.x == -1050.0 && area.y == 450.0, "addPrepArea 中心落在指定坐标");
        CHECK(!area.color.isValid(), "addPrepArea 默认 color 为 invalid（派生态，阶段11）");
        CHECK(core.findPrepArea(id1) == &runtime.fileData().prepAreas.first(),
              "findPrepArea 命中");
    }

    const QString id2 = core.addPrepArea(0.0, 1150.0);
    CHECK(runtime.fileData().prepAreas.at(1).name == QStringLiteral("准备区2"),
          "addPrepArea 命名按现存最大 N+1（准备区2）");
    CHECK(runtime.fileData().prepAreas.at(1).id == QStringLiteral("prep-002"),
          "addPrepArea id 递增查空（prep-002）");

    // ---- setPrepAreas 校验拒绝：宽高 ≤ 0、id/name 重复、空 id/名 ----
    QList<PrepAreaData> current = runtime.fileData().prepAreas;
    const int before = emissions;

    QList<PrepAreaData> badSize = current;
    badSize[0].width = 0.0;
    CHECK(!core.setPrepAreas(badSize), "setPrepAreas 拒绝宽 ≤ 0");
    badSize = current;
    badSize[1].height = -10.0;
    CHECK(!core.setPrepAreas(badSize), "setPrepAreas 拒绝高 ≤ 0");
    QList<PrepAreaData> dupId = current;
    dupId[1].id = current.first().id;
    CHECK(!core.setPrepAreas(dupId), "setPrepAreas 拒绝 id 重复");
    QList<PrepAreaData> dupName = current;
    dupName[1].name = current.first().name;
    CHECK(!core.setPrepAreas(dupName), "setPrepAreas 拒绝 name 重复");
    QList<PrepAreaData> emptyId = current;
    emptyId[0].id.clear();
    CHECK(!core.setPrepAreas(emptyId), "setPrepAreas 拒绝空 id");
    QList<PrepAreaData> emptyName = current;
    emptyName[0].name.clear();
    CHECK(!core.setPrepAreas(emptyName), "setPrepAreas 拒绝空 name");
    CHECK(emissions == before && !core.canUndo(),
          "非法 setPrepAreas：不改数据、不发信号、不动撤销栈");

    // ---- setPrepAreas：无变更不提交；合法修改生效（模拟设置页移动/改宽高） ----
    CHECK(!core.setPrepAreas(current), "setPrepAreas 无变更不提交（返回 false）");
    CHECK(emissions == before, "setPrepAreas 无变更不发 fileDataChanged");

    QList<PrepAreaData> moved = current;
    moved[0].x = -1000.0;
    moved[0].width = 500.0;
    CHECK(core.setPrepAreas(moved), "setPrepAreas 合法修改生效");
    CHECK(runtime.fileData().prepAreas.first().x == -1000.0
              && runtime.fileData().prepAreas.first().width == 500.0,
          "setPrepAreas 提交回数据层");
    CHECK(emissions == before + 1, "setPrepAreas 发出一次 fileDataChanged");
    CHECK(!core.canUndo(), "setPrepAreas 不产生撤销条目（canUndo 不变）");
    CHECK(runtime.isDirty(), "准备区修改弄脏文件（dirty 置位）");

    // ---- 颜色变更（阶段11）：仅改 color 也构成真实变更（operator== 含 color） ----
    QList<PrepAreaData> colored = runtime.fileData().prepAreas;
    colored[0].color = QColor(QStringLiteral("#123abc"));
    const int beforeColor = emissions;
    CHECK(core.setPrepAreas(colored), "setPrepAreas 仅改颜色也提交（dirty 判定含 color）");
    CHECK(runtime.fileData().prepAreas.first().color == QColor(QStringLiteral("#123abc")),
          "setPrepAreas 颜色变更生效");
    CHECK(emissions == beforeColor + 1, "颜色变更发出一次 fileDataChanged");
    colored[0].color = QColor(); // 改回派生态
    CHECK(core.setPrepAreas(colored)
              && !runtime.fileData().prepAreas.first().color.isValid(),
          "setPrepAreas 颜色改回 invalid（恢复派生态路径存在）");

    // ---- 往返一致（阶段11）：显式色与派生态经 JSON 序列化/解析逐字段还原 ----
    QList<PrepAreaData> roundTrip = runtime.fileData().prepAreas;
    roundTrip[0].color = QColor(QStringLiteral("#d35400")); // prep-001 显式色
    roundTrip[1].color = QColor();                          // prep-002 派生态
    CHECK(core.setPrepAreas(roundTrip), "setPrepAreas 写入显式/派生两态颜色");
    const LoadResult rr = JsonStore::fromJson(JsonStore::toJson(runtime.fileData()));
    CHECK(rr.ok() && rr.data.prepAreas == runtime.fileData().prepAreas,
          "准备区颜色 JSON 往返一致（显式色 + 派生态）");

    // ---- undo 不影响准备区：对象命令入栈 → 改准备区 → undo 只撤对象 ----
    CHECK(core.createObject(ObjectType::Actor, QPointF(0.0, 0.0)), "准备区测试：创建演员入撤销栈");
    CHECK(core.canUndo(), "准备区测试：对象命令可撤销");
    const QString id3 = core.addPrepArea(500.0, 500.0);
    CHECK(id3 == QStringLiteral("prep-003")
              && runtime.fileData().prepAreas.size() == 3,
          "入栈后新建准备区（prep-003）");
    CHECK(core.undo(), "准备区测试：undo 撤掉对象命令");
    CHECK(runtime.fileData().objects.isEmpty(), "准备区测试：undo 移除新建对象");
    CHECK(runtime.fileData().prepAreas.size() == 3,
          "准备区测试：undo 不影响准备区（仍在列表）");
    CHECK(!core.canUndo(), "准备区测试：撤销后栈空（无准备区条目残留）");

    // ---- 命名递增按现存最大 N：删掉 准备区2 再新建，不与现存重名 ----
    CHECK(core.removePrepArea(id2), "removePrepArea 成功");
    CHECK(runtime.fileData().prepAreas.size() == 2
              && core.findPrepArea(id2) == nullptr,
          "removePrepArea 从数据层移除");
    const QString id4 = core.addPrepArea(0.0, 0.0);
    CHECK(runtime.fileData().prepAreas.last().name == QStringLiteral("准备区4"),
          "删中间项后命名仍按现存最大 N+1（准备区4，不回填）");
    QSet<QString> names;
    bool unique = true;
    for (const PrepAreaData& area : runtime.fileData().prepAreas) {
        unique = unique && !names.contains(area.name);
        names.insert(area.name);
    }
    CHECK(unique, "准备区名全局唯一");
    CHECK(!core.removePrepArea(QStringLiteral("prep-999")), "removePrepArea 不存在 id 拒绝");
    Q_UNUSED(id4);
}

// 阶段12：移动路径——JsonStore 宽容解析（非法 type/legAngles 长度）/ operator==
// 传导（dirty 判定）/ EditorCore 路径命令（类型切换/圆心角钳制/加点保形/拖点/
// 删点合并/t 滑动保序/恢复直线/零存储默认态）/ 级联（改时刻重绑定、段内打帧丢弃、
// 删帧相邻两段丢弃、删对象级联）与撤销恢复 / 插值弧长匀速数值断言
void testPaths()
{
    CHECK(FileData::createDefault().paths.isEmpty(), "createDefault：路径为空列表");

    // ---- JsonStore 宽容解析 ----
    {
        QJsonObject root = JsonStore::toJson(FileData::createDefault());
        QJsonArray paths;
        QJsonObject badType;
        badType[QStringLiteral("object")] = QStringLiteral("actor-001");
        badType[QStringLiteral("from")] = 1.0;
        badType[QStringLiteral("to")] = 2.0;
        badType[QStringLiteral("type")] = QStringLiteral("curve"); // 非法 → 宽容 Line
        QJsonArray onePoint;
        QJsonObject point;
        point[QStringLiteral("x")] = 100.0;
        point[QStringLiteral("y")] = 50.0;
        onePoint.append(point);
        badType[QStringLiteral("points")] = onePoint;
        QJsonArray shortAngles; // 长度 1 ≠ points+1=2 → 补 0
        shortAngles.append(30.0);
        badType[QStringLiteral("legAngles")] = shortAngles;
        paths.append(badType);
        QJsonObject longAngles = badType;
        longAngles[QStringLiteral("type")] = QStringLiteral("arc");
        QJsonArray fourAngles; // 长度 4 → 截断到 2
        for (double a : { 10.0, 20.0, 30.0, 40.0 })
            fourAngles.append(a);
        longAngles[QStringLiteral("legAngles")] = fourAngles;
        paths.append(longAngles);
        root[QStringLiteral("paths")] = paths;
        const LoadResult result = JsonStore::fromJson(root);
        CHECK(result.ok() && result.data.paths.size() == 2, "路径解析成功（宽容字段）");
        if (result.ok() && result.data.paths.size() == 2) {
            CHECK(result.data.paths.at(0).type == PathType::Line,
                  "非法 type 宽容为 Line");
            CHECK(result.data.paths.at(0).legAngles == QList<double>({ 30.0, 0.0 }),
                  "legAngles 长度不足补 0");
            CHECK(result.data.paths.at(1).type == PathType::Arc
                      && result.data.paths.at(1).legAngles == QList<double>({ 10.0, 20.0 }),
                  "legAngles 超长截断到 points+1");
        }
        QJsonObject notArray = root;
        notArray[QStringLiteral("paths")] = 42;
        CHECK(JsonStore::fromJson(notArray).error == LoadError::InvalidData,
              "paths 非数组返回 LoadError::InvalidData");
    }

    // ---- operator== 传导（漏字段 = dirty 跟踪静默失效）----
    {
        FileData a = makeSampleData(); // 含 1 条弧线路径
        FileData b = a;
        CHECK(a == b, "路径：相同数据相等");
        b.paths[0].legAngles[0] += 1.0;
        CHECK(a != b, "路径：legAngles 差异传导到 operator==");
        b = a;
        b.paths[0].points[0].x += 1.0;
        CHECK(a != b, "路径：points 差异传导到 operator==");
    }

    // ---- EditorCore 路径命令 ----
    RuntimeData runtime;
    EditorCore core(&runtime);
    {
        // 直提交搭数据（不入撤销栈，后续断言从干净栈起步）：演员 + 2 帧（段 1s→3s）
        FileData setup = FileData::createDefault();
        ObjectData actorData;
        actorData.id = QStringLiteral("actor-001");
        actorData.type = ObjectType::Actor;
        actorData.name = QStringLiteral("张三");
        actorData.keyframes = { Keyframe { 1.0, 100.0, 100.0, 0.0 },
                                Keyframe { 3.0, 300.0, 100.0, 0.0 } };
        setup.objects.append(actorData);
        runtime.commitFileData(setup);
    }
    const QString actor = QStringLiteral("actor-001");
    CHECK(runtime.fileData().objects.first().keyframes.size() == 2 && !core.canUndo(),
          "路径测试：初始 2 帧段（1s→3s）且撤销栈空");
    CHECK(runtime.fileData().paths.isEmpty(), "路径测试：初始无 PathData（零存储默认态）");

    // setPathType：切弧线 → 建条目（默认 30° 立即可见）；无点切回直线 → 条目移除
    CHECK(core.setPathType(actor, 1.0, 3.0, PathType::Arc), "setPathType 切弧线生效");
    {
        const PathData* path = core.findPath(actor, 1.0, 3.0);
        CHECK(path && path->type == PathType::Arc
                  && path->legAngles == QList<double>({ PathGeometry::kDefaultLegAngleDeg }),
              "setPathType 切弧线建条目（默认圆心角 30°）");
    }
    CHECK(core.setPathType(actor, 1.0, 3.0, PathType::Line), "setPathType 切回直线生效");
    CHECK(runtime.fileData().paths.isEmpty(),
          "setPathType 直线且无路径点 → 条目移除（零存储默认态）");

    // setPathLegAngle：±350° 钳制
    core.setPathType(actor, 1.0, 3.0, PathType::Arc);
    CHECK(core.setPathLegAngle(actor, 1.0, 3.0, 0, 500.0), "setPathLegAngle 生效");
    CHECK(core.findPath(actor, 1.0, 3.0)->legAngles.first() == 350.0,
          "setPathLegAngle 钳制 +350°");
    core.setPathLegAngle(actor, 1.0, 3.0, 0, -500.0);
    CHECK(core.findPath(actor, 1.0, 3.0)->legAngles.first() == -350.0,
          "setPathLegAngle 钳制 -350°");
    CHECK(core.setPathLegAngle(actor, 1.0, 3.0, 0, 60.0), "setPathLegAngle 改回 60°");
    CHECK(core.canUndo(), "路径命令入撤销栈");

    // addPathPoint 保形：弧线上加点瞬间形状不变、圆心角按弧长比例拆分
    const QList<QPointF> before =
        PathGeometry::samplePath(*core.findPath(actor, 1.0, 3.0),
                                 QPointF(100.0, 100.0), QPointF(300.0, 100.0));
    const QList<double> beforeCum = PathGeometry::cumulativeLengths(before);
    const QPointF clickPos = PathGeometry::pointAtRatio(before, beforeCum, 1.0 / 3.0);
    CHECK(core.addPathPoint(actor, 1.0, 3.0, clickPos), "addPathPoint 生效");
    {
        const PathData* path = core.findPath(actor, 1.0, 3.0);
        CHECK(path->points.size() == 1 && path->legAngles.size() == 2,
              "addPathPoint 插入 1 点、圆心角拆成 2 段");
        const double sum = path->legAngles.at(0) + path->legAngles.at(1);
        CHECK(qAbs(sum - 60.0) < 1e-6, "addPathPoint 圆心角拆分：θ₁+θ₂ = θ");
        CHECK(qAbs(path->legAngles.at(0) - 20.0) < 0.5,
              "addPathPoint 圆心角按弧长比例拆分（1/3 处 → θ₁≈20°）");
        // 加点后该点仍在原曲线上（吸附点击处的投影）
        const QList<QPointF> after =
            PathGeometry::samplePath(*path, QPointF(100.0, 100.0), QPointF(300.0, 100.0));
        CHECK(PathGeometry::projectToRatio(after, clickPos) > 0.0, "加点后曲线可投影");
        const double d1 = QLineF(PathGeometry::pointAtRatio(after,
                                 PathGeometry::cumulativeLengths(after), 1.0 / 3.0),
                                 clickPos).length();
        CHECK(d1 < 0.5, "加点瞬间路径形状不变（弧长 1/3 处原位）");
        const double d2 = QLineF(PathGeometry::pointAtRatio(after,
                                 PathGeometry::cumulativeLengths(after), 0.7),
                                 PathGeometry::pointAtRatio(before, beforeCum, 0.7))
                              .length();
        CHECK(d2 < 0.5, "加点瞬间路径形状不变（弧长 0.7 处原位）");
    }

    // movePathPoint + undo 恢复
    CHECK(core.movePathPoint(actor, 1.0, 3.0, 0, QPointF(200.0, 40.0)),
          "movePathPoint 生效");
    CHECK(qAbs(core.findPath(actor, 1.0, 3.0)->points.first().x - 200.0) < 1e-9
              && qAbs(core.findPath(actor, 1.0, 3.0)->points.first().y - 40.0) < 1e-9,
          "movePathPoint 写入新位置");
    CHECK(core.undo(), "movePathPoint 可撤销");
    CHECK(core.findPath(actor, 1.0, 3.0)->points.first().x
              == PathGeometry::pointAtRatio(before, beforeCum, 1.0 / 3.0).x(),
          "undo 恢复路径点原位");

    // setPathPointT：沿曲线滑动重定位（t=0.8 → 点落在弧长 80% 处）；相邻钳制保序
    CHECK(core.setPathPointT(actor, 1.0, 3.0, 0, 0.8), "setPathPointT 生效");
    {
        const PathData* path = core.findPath(actor, 1.0, 3.0);
        const QList<QPointF> polyline =
            PathGeometry::samplePath(*path, QPointF(100.0, 100.0), QPointF(300.0, 100.0));
        const double readBack = PathGeometry::projectToRatio(
            polyline, QPointF(path->points.first().x, path->points.first().y));
        CHECK(qAbs(readBack - 0.8) < 0.01, "setPathPointT 滑动后投影回读 ≈ 0.8");
    }
    // 保序钳制：再加一点（t≈0.5），把前点 t 改到越过它 → 被钳制不超邻点
    {
        const PathData* path = core.findPath(actor, 1.0, 3.0);
        const QList<QPointF> polyline =
            PathGeometry::samplePath(*path, QPointF(100.0, 100.0), QPointF(300.0, 100.0));
        const QPointF mid = PathGeometry::pointAtRatio(
            polyline, PathGeometry::cumulativeLengths(polyline), 0.5);
        CHECK(core.addPathPoint(actor, 1.0, 3.0, mid), "保序测试：再加一点（t≈0.5）");
    }
    CHECK(core.findPath(actor, 1.0, 3.0)->points.size() == 2, "保序测试：2 个路径点");
    core.setPathPointT(actor, 1.0, 3.0, 1, 0.1); // 把第二点 t 拉到第一点（t=0.8）之前
    {
        const PathData* path = core.findPath(actor, 1.0, 3.0);
        const QList<QPointF> polyline =
            PathGeometry::samplePath(*path, QPointF(100.0, 100.0), QPointF(300.0, 100.0));
        const double t0 = PathGeometry::projectToRatio(
            polyline, QPointF(path->points.at(0).x, path->points.at(0).y));
        const double t1 = PathGeometry::projectToRatio(
            polyline, QPointF(path->points.at(1).x, path->points.at(1).y));
        CHECK(t0 < t1, "setPathPointT 相邻点 t 钳制保序（t0 < t1 保持）");
    }

    // removePathPoint：圆心角合并回 ≈60°（拆分逆操作）
    CHECK(core.removePathPoint(actor, 1.0, 3.0, 1), "removePathPoint 生效");
    {
        const PathData* path = core.findPath(actor, 1.0, 3.0);
        CHECK(path->points.size() == 1 && path->legAngles.size() == 2,
              "removePathPoint 删点剩 1 点 2 段");
    }
    CHECK(core.removePathPoint(actor, 1.0, 3.0, 0), "removePathPoint 删空路径点");
    {
        const PathData* path = core.findPath(actor, 1.0, 3.0);
        CHECK(path && path->points.isEmpty() && path->type == PathType::Arc
                  && path->legAngles.size() == 1
                  && qAbs(path->legAngles.first() - 60.0) < 0.5,
              "removePathPoint 删空后圆心角合并 ≈ 60°（弧线单段保留）");
    }

    // resetPath：恢复直线 = 条目移除
    CHECK(core.resetPath(actor, 1.0, 3.0), "resetPath 生效");
    CHECK(runtime.fileData().paths.isEmpty(), "resetPath 移除 PathData（恢复直线）");
    while (core.canUndo())
        core.undo(); // 清栈回到 2 帧直线状态
    CHECK(runtime.fileData().paths.isEmpty()
              && runtime.fileData().objects.first().keyframes.size() == 2,
          "路径测试：清栈回直线状态");

    // ---- 级联：moveKeyframe 段标识重绑定 ----
    core.setPathType(actor, 1.0, 3.0, PathType::Arc);
    CHECK(core.moveKeyframe(actor, 3.0, 4.0), "级联：moveKeyframe 改时刻");
    CHECK(core.findPath(actor, 1.0, 3.0) == nullptr
              && core.findPath(actor, 1.0, 4.0) != nullptr
              && runtime.fileData().paths.first().to == 4.0,
          "级联：路径 to 跟随重绑定 3.0→4.0");
    CHECK(core.undo() && core.findPath(actor, 1.0, 3.0) != nullptr,
          "级联：undo 恢复段标识 3.0");

    // ---- 级联：段内 upsert 新帧 → 该段 PathData 静默丢弃；端点更新保留 ----
    CHECK(core.upsertKeyframe(actor, Keyframe { 2.0, 200.0, 100.0, 0.0 }),
          "级联：段内打帧（t=2）");
    CHECK(runtime.fileData().paths.isEmpty(), "级联：段内打帧丢弃该段 PathData");
    CHECK(core.undo() && core.findPath(actor, 1.0, 3.0) != nullptr,
          "级联：undo 恢复被丢弃的 PathData");
    CHECK(core.upsertKeyframe(actor, Keyframe { 1.0, 120.0, 100.0, 0.0 }),
          "级联：端点帧更新（t=1 改值）");
    CHECK(core.findPath(actor, 1.0, 3.0) != nullptr,
          "级联：端点更新保留 PathData（端点派生跟随）");
    CHECK(core.undo(), "级联：undo 端点更新");

    // ---- 级联：removeKeyframes 删端点帧 → 相邻段 PathData 丢弃 ----
    CHECK(core.removeKeyframes({ qMakePair(actor, 1.0) }), "级联：删除起始帧");
    CHECK(runtime.fileData().paths.isEmpty(), "级联：删端点帧丢弃相邻段 PathData");
    CHECK(core.undo() && core.findPath(actor, 1.0, 3.0) != nullptr
              && runtime.fileData().objects.first().keyframes.size() == 2,
          "级联：undo 恢复帧与 PathData");

    // ---- 级联：deleteSelection → 对象全部 PathData 级联删除 ----
    core.selection()->select(actor);
    CHECK(core.deleteSelection(), "级联：删除对象");
    CHECK(runtime.fileData().objects.isEmpty() && runtime.fileData().paths.isEmpty(),
          "级联：对象删除级联删除其全部 PathData");
    CHECK(core.undo() && runtime.fileData().paths.size() == 1
              && runtime.fileData().objects.size() == 1,
          "级联：undo 恢复对象与 PathData（严格互逆）");
    while (core.canUndo())
        core.undo(); // 回到干净状态（防污染后续用例）

    // ---- 插值：弧长匀速数值断言（弧线上等 t 步进 = 等弧长步进） ----
    {
        // 单段弧 θ=90°，端点 (0,0)→(200,0)，段 1s→3s
        ObjectData arcObject;
        arcObject.id = QStringLiteral("arc-obj");
        arcObject.keyframes = { Keyframe { 1.0, 0.0, 0.0, 0.0 },
                                Keyframe { 3.0, 200.0, 0.0, 0.0 } };
        PathData arc;
        arc.objectId = arcObject.id;
        arc.from = 1.0;
        arc.to = 3.0;
        arc.type = PathType::Arc;
        arc.legAngles = { 90.0 };
        const QList<PathData> paths { arc };
        // 弧半径 R=100/(2·sin45°)... c=200 → R≈141.42，弧长 = R·π/2 ≈ 222.21
        QList<QPointF> positions;
        for (double r : { 0.0, 0.25, 0.5, 0.75, 1.0 }) {
            const Transform v =
                TimelinePlayer::interpolate(arcObject, paths, 1.0 + 2.0 * r);
            positions.append(QPointF(v.x, v.y));
        }
        // 等 t → 等弧长（匀速沿路径）
        bool uniform = true;
        double step = -1.0;
        for (int i = 1; i < positions.size(); ++i) {
            const double dist = QLineF(positions.at(i - 1), positions.at(i)).length();
            if (step < 0.0)
                step = dist;
            uniform = uniform && qAbs(dist - step) < 0.5; // 采样精度容差（每段 32 采样）
        }
        CHECK(uniform, "插值：弧线上等 t 步进 = 等弧长步进（弧长匀速）");
        CHECK(qAbs(step - 222.21 / 4.0) < 1.0, "插值：弧长总量正确（R·π/2）");
        // 弧长中点在弧顶（左鼓出：行进 +x 方向左侧 = y 负向），≠ 直线中点 (100,0)
        CHECK(qAbs(positions.at(2).x() - 100.0) < 1.0
                  && positions.at(2).y() < -40.0,
              "插值：弧线路径中点落在弧顶（非直线中点）");
        // 朝向与路径无关（最短弧匀速，此处两帧同向恒 0）
        CHECK(TimelinePlayer::interpolate(arcObject, paths, 2.0).rotation == 0.0,
              "插值：朝向不受路径影响");
        // 无 PathData 段：带路径重载与旧签名逐位一致（既有行为零回归）
        const Transform legacy = TimelinePlayer::interpolate(arcObject, 2.0);
        const Transform withPaths =
            TimelinePlayer::interpolate(arcObject, QList<PathData>(), 2.0);
        CHECK(legacy == withPaths, "插值：无路径段与旧签名逐位一致");
    }
}

// 阶段8：AppSettings 最近使用列表（MRU）语义 + RuntimeData 记录时机。
// MRU 哨兵在用例开头/结尾清理，避免残留影响下次运行
void testRecentFiles(QTemporaryDir& dir)
{
    // 从已知状态开始：清空（QSettings 跨运行持久化）
    {
        AppSettings settings;
        const QStringList existing = settings.recentFiles();
        for (const QString& path : existing)
            settings.removeRecentFile(path);
        CHECK(settings.recentFiles().isEmpty(), "MRU：用例前清空最近列表");
    }

    // ---- 追加 / 置顶（新→旧） ----
    AppSettings settings;
    settings.addRecentFile(QStringLiteral("C:/Projects/alpha.json"));
    settings.addRecentFile(QStringLiteral("D:/Work/beta.json"));
    CHECK(settings.recentFiles()
              == QStringList({ QStringLiteral("D:/Work/beta.json"),
                               QStringLiteral("C:/Projects/alpha.json") }),
          "MRU：追加后新条目置顶（新→旧）");

    // ---- 去重置顶：Windows 路径大小写不敏感比较 ----
    settings.addRecentFile(QStringLiteral("c:/projects/ALPHA.json"));
    CHECK(settings.recentFiles().size() == 2
              && settings.recentFiles().first() == QStringLiteral("c:/projects/ALPHA.json"),
          "MRU：大小写不敏感去重后置顶（保留本次写入的写法）");
    // 分隔符差异视为同一路径
    settings.addRecentFile(QStringLiteral("D:\\Work\\beta.json"));
    CHECK(settings.recentFiles().size() == 2
              && settings.recentFiles().first() == QStringLiteral("D:\\Work\\beta.json"),
          "MRU：反斜杠分隔符去重后置顶");

    // ---- 超 10 条截断：丢弃最旧 ----
    for (int i = 0; i < 12; ++i)
        settings.addRecentFile(QStringLiteral("E:/mru-%1.json").arg(i));
    CHECK(settings.recentFiles().size() == AppSettings::kMaxRecentFiles,
          "MRU：上限 10 条截断");
    CHECK(settings.recentFiles().first() == QStringLiteral("E:/mru-11.json")
              && settings.recentFiles().last() == QStringLiteral("E:/mru-2.json"),
          "MRU：截断丢弃最旧条目");

    // ---- removeRecentFile：按同一大小写不敏感比较 ----
    settings.removeRecentFile(QStringLiteral("e:/MRU-11.JSON"));
    CHECK(settings.recentFiles().size() == 9
              && !settings.recentFiles().contains(QStringLiteral("E:/mru-11.json")),
          "MRU：removeRecentFile 按同一比较移除");

    // ---- 跨实例读回（同进程新实例 = QSettings 持久化） ----
    {
        AppSettings fresh;
        CHECK(fresh.recentFiles() == settings.recentFiles(), "MRU：跨实例读回一致");
    }

    // ---- RuntimeData 记录时机：openFile/saveFile 成功入 MRU，失败不入 ----
    {
        RuntimeData runtime;
        const QString openPath = dir.filePath(QStringLiteral("mru-open.json"));
        CHECK(JsonStore::save(openPath, makeSampleData()).ok, "MRU：测试准备写入存档");
        CHECK(runtime.openFile(openPath).ok(), "MRU：openFile 成功");
        CHECK(!runtime.settings().recentFiles().isEmpty()
                  && runtime.settings().recentFiles().first() == openPath,
              "MRU：openFile 成功入列表（置顶）");

        const QString savePath = dir.filePath(QStringLiteral("mru-saveas.json"));
        CHECK(runtime.saveFile(savePath, runtime.fileData()).ok, "MRU：saveFile 成功");
        CHECK(runtime.settings().recentFiles().first() == savePath,
              "MRU：saveFile 成功入列表（另存为路径置顶）");

        const QStringList before = runtime.settings().recentFiles();
        const QString missingPath = dir.filePath(QStringLiteral("mru-missing.json"));
        CHECK(!runtime.openFile(missingPath).ok(), "MRU：openFile 失败");
        CHECK(runtime.settings().recentFiles() == before
                  && !runtime.settings().recentFiles().contains(missingPath),
              "MRU：openFile 失败不入列表");
    }

    // 用例结尾清理 MRU 哨兵
    {
        AppSettings cleanup;
        const QStringList existing = cleanup.recentFiles();
        for (const QString& path : existing)
            cleanup.removeRecentFile(path);
        CHECK(cleanup.recentFiles().isEmpty(), "MRU：用例结尾清理哨兵");
    }
}


// 子进程模式：读回哨兵值校验 QSettings 持久化
int verifySettings()
{
    AppSettings settings;
    const bool ok = !settings.gridSnapEnabled()
        && !settings.gridVisible()
        && settings.keyframePromptSuppressed()
        && settings.windowGeometry() == QByteArray("selftest-geometry-7f3a")
        && settings.windowState() == QByteArray("selftest-state-9b2c")
        && settings.stageColor() == QColor(QStringLiteral("#123456"))
        && settings.gridSizeCm() == 55.5
        && settings.snapRangeCm() == 12.5;
    out() << (ok ? "[PASS] 子进程读回设置哨兵值" : "[FAIL] 子进程读回设置哨兵值") << Qt::endl;
    return ok ? 0 : 1;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QLatin1String(kTestOrgName));
    QCoreApplication::setApplicationName(QLatin1String(kTestAppName));

    if (app.arguments().contains(QStringLiteral("--verify-settings")))
        return verifySettings();

    testTypeMapping();
    testAngleConversion();
    testRotationFromVector();
    testCommandRoundTrip();
    testReparentCommand();
    testCascadeRemove();
    testEditorCore();
    testEditorCoreGroupReparent();
    testInterpolation();
    testKeyframeCommands();
    testTimelineElementEditing();
    testKeyframeWorkflow();
    testTimelineUndoRefresh();

    QTemporaryDir dir;
    if (!dir.isValid()) {
        out() << "[FAIL] 无法创建临时目录" << Qt::endl;
        return 1;
    }

    testRoundTrip(dir);
    testParentGroupJson(dir);
    testCorruptedFile(dir);
    testVersionMismatch(dir);
    testMissingFile(dir);
    testInvalidStructure(dir);
    testSettingsPersistence(app);
    testRuntimeData(dir);
    testDirtyTracking(dir);
    testStageSettings();
    testPrepAreas();
    testPaths();
    testUndoStack(dir);
    testUndoRedoSequence();
    testRecentFiles(dir);

    out() << Qt::endl;
    if (g_failures == 0) {
        out() << "全部用例通过" << Qt::endl;
        return 0;
    }
    out() << g_failures << " 个用例失败" << Qt::endl;
    return 1;
}
