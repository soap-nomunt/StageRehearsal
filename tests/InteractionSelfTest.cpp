// 交互自测（阶段3 验收辅助，独立目标）：QTest 合成鼠标/键盘事件驱动真实 MainWindow，
// 覆盖需求第 11~14 条的视口交互路径：选择（单选/Ctrl/框选中心点判定/点空白）、
// 拖拽移动（网格吸附 20cm 范围）、多选整体移动、方向键微调 10cm、朝向手柄旋转、
// 复制/粘贴/删除快捷键、Hierarchy 双向同步、Inspector 即时生效与重名拒绝。
// 阶段3.5 追加：空对象创建（业务入口）、Hierarchy 树形拖拽整理（合成 QDropEvent
// 走同一 dropEvent 路径；真实拖拽手势是模态的，列入手测清单）、环拒绝、
// 删除分组级联、重建后展开状态保持、选中子对象自动展开父链。
// 追加：悬停/拖拽中的指针形状反馈（viewport()->cursor().shape() 断言三类悬停位置
// 与 Moving/Rotating 拖拽期间形状；真实鼠标观感列入手测）。
// 右键菜单与 Create 对话框是模态的，其业务路径由 DataLayerSelfTest 的 EditorCore
// 用例覆盖；菜单弹出本身列入人工手测。
// 阶段4 追加：撤销/重做——真实 MainWindow 上合成 Ctrl+Z/Ctrl+Y（菜单快捷键接线），
// 逐级撤销到底再逐级重做一致；菜单项可用性随栈状态；Inspector/视口随重建刷新；
// 新命令清空 redo 区；newFile（fileReplaced）清栈。
// 阶段5 追加：时间轴——打帧按钮、需求第 20 条弹窗询问（选是/选否两路径，
// 模态弹窗用 QTimer 定时点按钮）、游标拖动实时联动视口与 Inspector、
// 关键帧点选/拖动改时刻/Delete 删除 + Ctrl+Z/Ctrl+Y 撤销重做且游标处显示
// 立即刷新、播放/暂停驱动游标推进、场次标签添加/双击改名/右键删除、
// 关键帧拖出时长自动延长及撤销还原。
// 阶段5 起视口/Inspector/方向键的对象变换修改走"游标时刻语义"（打关键帧，
// 不改静态 transform）；既有拖拽/旋转用例的位置断言改为当前时刻显示值
// （displayAt），静态 transform 保持不变另有断言。
// 时间轴交互完善追加：时间轴 Dock 初始高度、点标尺选中游标 + ←/→ 步进 ±0.5s
// （与游标拖拽吸附一致）、标尺拖拽 0.5s 吸附、关键帧 ←/→ 批量步进（撤销重做）、
// 关键帧级 Ctrl+C/V（画布焦点，含对象语义仲裁）、场次标签选中/步进/拖动改时刻/
// Delete 删除、点轨道联动对象选中、Esc 清空画布选择。
// 阶段6 追加：标题栏 * 随编辑/保存/撤销变化、saveToPath/openFromPath 路径明确
// 核心（损坏文件弹提示且数据不变）、askUnsaved/maybeCloseOrContinue 三选一门卫、
// closeEvent 不保存关闭/取消保留、新建菜单过门卫。
// 阶段7 追加：统一设置窗口（VSCode 风格，非模态单例）——菜单栏顶层"设置"项
// 单击直达打开（旧舞台设置/偏好设置两个入口并入）、重复打开聚焦而非新建、
// 分类切换、搜索过滤（命中/搜索中忽略分类分页/清空恢复/无匹配占位）；舞台尺寸
// editingFinished 直改直提交（不入撤销栈、Ctrl+Z 只撤对象编辑）、非法值被控件
// 钳制、新建/打开后控件回显同步；舞台颜色与三个偏好开关改动立即生效并跨实例
// 持久化（QColorDialog 仍是模态子弹窗，沿用 QTimer 驱动）。
// 阶段8 追加：启动页 StartDialog（非模态实例化直查控件，不经 exec）——MRU 列表
// 行数与顺序（去重置顶）、搜索过滤命中/无匹配占位、单击条目即选 OpenPath、
// 新建/空项目按钮、空列表占位；打开存档按钮走原生文件对话框列手测。
// 阶段8 启动适配：MainWindow 构造不再自动加载任何文件（构造即默认空舞台），
// 依赖示例存档的段落（main 初始/w2/w6）构造后显式 openFromPath 再继续断言。
// QFileDialog 原生对话框 offscreen 不可驱动——文件对话框与重名覆盖确认列手测。
// 阶段9 追加：窗口布局持久化（窗口1 改尺寸+浮动 Dock 关闭 → 窗口2 恢复一致；
// 首次运行默认 1440×900 三栏；用例前后清空布局键沿用哨兵模式）、
// 倍速下拉框"倍速"标注（speedLabel）与 ToolTip。
// 关闭项目追加：文件菜单"关闭项目(&C)"——过未保存门卫后回启动页（门卫取消留在
// 原项目且不弹启动页；启动页选空项目则运行时重置为默认空舞台、撤销栈清空、
// 主窗口重新显示；不脏时直弹启动页无门卫弹窗；启动页取消 = 退出程序，列手测）。
// 阶段10 追加：台下准备区——设置页"舞台与准备区"方块编辑器（PrepAreaCanvas）：
// 右键空白新建（QMenu::exec 模态菜单经 QTimer 驱动）/ 合成拖拽松开单次提交
// （50cm 吸附）/ 右键删除 / 选中联动属性行启用态与回显 / editingFinished 提交 /
// 舞台方块不可拖；主视口只读渲染（背景层无图元）与 resetView 含准备区；
// 画布与视口 grab 截图存档人工复核。
// 阶段11 追加：设置页舞台分类重构为双侧布局（左画布 + 右 Inspector 双页栈）——
// 选中切页（stageInspectorStack currentWidget + prepInspectorTitle 标题 + 行可见性）、
// 准备区 X/Y 行 editingFinished 提交且与画布拖拽双向同步、准备区独立颜色
// （prepColorButton 弹 QColorDialog，dirty 置位、不入撤销栈）、搜索"准备区"跨
// Inspector 页命中重挂、清空后挂回原页顺序保持；存档 v3（示例存档 prep-001 显式
// 色 / prep-002 派生态）；主视口按区颜色渲染截图；v2 旧档（prepAreas 无 color 键）
// 真实打开派生色显示、另存回读为 v3 恒写 color。
// 阶段11 追加（右键平移）：PrepAreaCanvas 右键按住拖拽 = 平移内容（同主视口
// StageView：任意位置生效、握拳光标、越阈值才移动、内容随手、零提交零撤销、
// 平移后吞掉 contextMenuEvent 不弹菜单——合成右键拖拽 + 补发合成 contextMenuEvent
// 验证）；画布无滚动条，平移限界为内容包围盒始终与控件相交（疯狂拖拽不丢内容）；
// 右键点击（未越阈值）仍弹菜单（新建/删除回归）。
// 阶段12 追加：双帧选中态 + 幽灵与直线路径可视化（纯渲染，数据层零改动）——
// 时间轴 Ctrl 加选同对象相邻两帧进入双帧态（EditorCore::activeSegment），
// 主视口绘制前帧幽灵（不透明）+ 后帧幽灵（0.4）+ 直线白线（幽灵不参与命中/
// 选中/显示值驱动）；加选不相邻/跨对象帧 = 替换为新单选；清除路径全覆盖
// （点空白/单选他帧/轨道选中变化/播放开始/fileReplaced/命令提交后防御）。
//
// 运行（Git Bash，项目根目录）：
//   QT_QPA_PLATFORM=offscreen ./build/Debug/InteractionSelfTest.exe
// 示例存档在 configure 时已拷入 exe 目录，自测经显式 openFromPath 加载 5 个对象。

#include "business/EditorCore.h"
#include "business/PathGeometry.h"
#include "business/SelectionModel.h"
#include "business/TimelinePlayer.h"
#include "data/RuntimeData.h"
#include "render/MainWindow.h"
#include "render/PrepAreaCanvas.h"
#include "render/SettingsWindow.h"
#include "render/StageObjectItem.h"
#include "render/StageScene.h"
#include "render/StageView.h"
#include "render/StartDialog.h"
#include "render/TimelineWidget.h"
#include "render/dialogs/CreateObjectDialog.h"

#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QDropEvent>
#include <QFile>
#include <QGraphicsPathItem>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <qpa/qplatformdrag.h>        // QPlatformDragQtResponse（handleDrag 返回类型）
#include <qpa/qwindowsysteminterface.h> // QtGui 私有头：平台级拖拽注入（仅测试目标用）

#include <cmath>

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

bool near(double a, double b, double eps = 2.0)
{
    return qAbs(a - b) <= eps;
}

// 示例存档初始坐标（examples/stage-demo.json）
const QPointF kZhangsan(300.0, 200.0);  // actor-001 张三 rot 0
const QPointF kLisi(-350.0, 400.0);     // actor-002 李四 rot 90
const QPointF kScreen1(-150.0, 100.0);  // screen-001 屏风1 rot 90

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("StageRehearsal"));
    QCoreApplication::setApplicationName(QStringLiteral("InteractionSelfTest")); // 独立设置区

    MainWindow window;
    window.resize(1600, 1000);
    window.show();

    // 阶段8：MainWindow 构造不再自动加载任何文件（构造即默认空舞台）；
    // 自测显式 openFromPath 复现原启动加载状态（成功 = 原自动加载的等价状态）。
    // 先用 JsonStore 探针确认可读，避免失败时 openFromPath 弹模态框挂住 offscreen 自测
    const QString demoPath =
        QCoreApplication::applicationDirPath() + QStringLiteral("/stage-demo.json");
    CHECK(JsonStore::load(demoPath).ok(), "启动适配：示例存档可读（configure 已拷入 exe 目录）");
    CHECK(window.openFromPath(demoPath), "启动适配：显式加载示例存档");

    if (!QTest::qWaitForWindowExposed(&window)) {
        out() << "[FAIL] 窗口未能曝光" << Qt::endl;
        return 1;
    }
    window.activateWindow();
    QTest::qWaitForWindowActive(&window);

    auto* view = qobject_cast<StageView*>(window.centralWidget());
    auto* core = window.findChild<EditorCore*>();
    auto* runtime = window.findChild<RuntimeData*>();
    auto* tree = window.findChild<QTreeWidget*>(QStringLiteral("hierarchyTree"));
    auto* stack = window.findChild<QStackedWidget*>(QStringLiteral("inspectorStack"));
    auto* emptyLabel = window.findChild<QLabel*>(QStringLiteral("inspectorEmptyLabel"));
    CHECK(view && core && runtime && tree && stack && emptyLabel, "主窗口组件装配完整");
    auto* scene = qobject_cast<StageScene*>(view->scene());
    CHECK(scene, "中央组件为 StageView/StageScene");

    // 适配舞台视野：保证测试用到的场景坐标（含 (-600,800)）都在视口内
    view->fitInView(scene->stageRect().adjusted(-150.0, -150.0, 150.0, 150.0),
                    Qt::KeepAspectRatio);

    QWidget* vp = view->viewport();
    const auto posOf = [view](const QPointF& scenePos) { return view->mapFromScene(scenePos); };
    const auto objectAt = [core](const QString& id) { return core->findObject(id); };
    // 当前时刻显示值（插值结果）：阶段5 起对象变换修改打关键帧，静态 transform 不变
    const auto displayAt = [core](const QString& id) {
        Transform value;
        core->displayTransform(id, value);
        return value;
    };
    const auto ids = [core]() { return core->selection()->selectedIds(); };

    // 阶段5：对象变换修改默认会触发打帧询问弹窗；既有静态语义用例一律
    // 视为"不再提示"（直接打帧），需求第 20 条弹窗路径在阶段5 段落单独覆盖
    runtime->settings().setKeyframePromptSuppressed(true);
    // 网格间距/吸附距离是用户可调设置（QSettings 跨运行持久化）：复位默认值，
    // 保证拖拽吸附用例的 100cm/20cm 前提不受以往运行残留影响
    runtime->settings().setGridSizeCm(AppSettings::defaultGridSizeCm);
    runtime->settings().setSnapRangeCm(AppSettings::defaultSnapRangeCm);

    // ---- 初始状态：示例存档 5 对象、舞台不进 Hierarchy、无选中 ----
    CHECK(runtime->fileData().objects.size() == 5, "启动加载示例存档（5 个对象）");
    CHECK(tree->topLevelItemCount() == 5, "Hierarchy 平铺 5 个对象（舞台不列出）");
    CHECK(ids().isEmpty(), "初始无选中");
    CHECK(stack->currentIndex() == 0, "无选中时 Inspector 表单清空");

    // ---- 单击选中（点演员中心；白三角已移到圆外，中心不命中旋转手柄） ----
    QTest::mouseClick(vp, Qt::LeftButton, Qt::NoModifier, posOf(kZhangsan));
    CHECK(ids() == QList<QString> { QStringLiteral("actor-001") }, "单击选中张三");
    CHECK(stack->currentIndex() == 1, "单选演员 → Inspector 演员表单");
    CHECK(window.findChild<QLineEdit*>(QStringLiteral("actorNameEdit"))->text()
              == QStringLiteral("张三"),
          "Inspector 显示角色名");
    CHECK(near(window.findChild<QDoubleSpinBox*>(QStringLiteral("actorXSpin"))->value(), 300.0, 0.01),
          "Inspector 显示位置 X");
    CHECK(tree->selectedItems().size() == 1
              && tree->selectedItems().first()->data(0, Qt::UserRole).toString()
                     == QStringLiteral("actor-001"),
          "Hierarchy 随视口选中同步");
    CHECK(scene->itemById(QStringLiteral("actor-001"))->isHighlighted(), "选中图元高亮标记置位");

    // ---- Ctrl+单击多选（点李四中心；Ctrl 按下优先多选语义，不触发旋转） ----
    QTest::mouseClick(vp, Qt::LeftButton, Qt::ControlModifier, posOf(kLisi));
    CHECK(ids().size() == 2 && ids().contains(QStringLiteral("actor-002")),
          "Ctrl+单击追加多选");
    CHECK(stack->currentIndex() == 0 && emptyLabel->text().contains(QStringLiteral("2")),
          "多选时 Inspector 清空表单");

    // ---- Ctrl+单击已选中对象 → 反选 ----
    QTest::mouseClick(vp, Qt::LeftButton, Qt::ControlModifier, posOf(kLisi));
    CHECK(ids() == QList<QString> { QStringLiteral("actor-001") }, "Ctrl+单击已选中对象反选");
    QTest::mouseClick(vp, Qt::LeftButton, Qt::ControlModifier, posOf(kLisi)); // 恢复双选
    CHECK(ids().size() == 2, "再次 Ctrl+单击恢复双选");

    // ---- 框选：中心点判定（矩形盖住张三/李四中心，不盖住王五/屏风1/屏风2） ----
    QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, posOf(QPointF(-600.0, 150.0)));
    QTest::mouseMove(vp, posOf(QPointF(390.0, 450.0)));
    QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, posOf(QPointF(390.0, 450.0)));
    CHECK(ids().size() == 2 && ids().contains(QStringLiteral("actor-001"))
              && ids().contains(QStringLiteral("actor-002")),
          "框选：中心点被框住即选中（张三+李四）");

    // ---- 点空白取消选择（舞台区域是背景，不可交互） ----
    QTest::mouseClick(vp, Qt::LeftButton, Qt::NoModifier, posOf(QPointF(-600.0, 800.0)));
    CHECK(ids().isEmpty(), "点空白取消选择");

    // ---- 拖拽移动（吸附开：默认开；目标 (390,205) 落在网格 (400,200) 的 20cm 范围内） ----
    runtime->settings().setGridSnapEnabled(true);
    QTest::mouseClick(vp, Qt::LeftButton, Qt::NoModifier, posOf(kZhangsan + QPointF(0.0, -25.0)));
    CHECK(ids() == QList<QString> { QStringLiteral("actor-001") }, "拖拽前单选张三（点圆边缘避开白三角）");
    QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, posOf(kZhangsan + QPointF(0.0, -25.0)));
    QTest::mouseMove(vp, posOf(kZhangsan + QPointF(45.0, -10.0)));
    QTest::mouseMove(vp, posOf(QPointF(390.0, 180.0)));
    QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, posOf(QPointF(390.0, 180.0)));
    CHECK(near(displayAt(QStringLiteral("actor-001")).x, 400.0)
              && near(displayAt(QStringLiteral("actor-001")).y, 200.0),
          "拖拽松开一次性提交（打帧 @t=0），吸附到 100cm 网格（390→400, 205→200）");
    CHECK(near(objectAt(QStringLiteral("actor-001"))->transform.x, kZhangsan.x())
              && near(objectAt(QStringLiteral("actor-001"))->transform.y, kZhangsan.y()),
          "游标时刻语义：静态 transform 不变");
    CHECK(objectAt(QStringLiteral("actor-001"))->keyframes.size() == 1
              && near(objectAt(QStringLiteral("actor-001"))->keyframes.first().t, 0.0, 0.001),
          "拖拽在游标处生成关键帧");
    CHECK(near(scene->itemById(QStringLiteral("actor-001"))->pos().x(), 400.0)
              && near(scene->itemById(QStringLiteral("actor-001"))->pos().y(), 200.0),
          "场景重建后图元落点一致");

    // ---- 拖拽移动（吸附关：精确落点） ----
    runtime->settings().setGridSnapEnabled(false);
    QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, posOf(QPointF(400.0, 175.0)));
    QTest::mouseMove(vp, posOf(QPointF(430.0, 195.0)));
    QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, posOf(QPointF(455.0, 210.0)));
    CHECK(near(displayAt(QStringLiteral("actor-001")).x, 455.0)
              && near(displayAt(QStringLiteral("actor-001")).y, 235.0),
          "吸附关闭时拖拽精确落点 (455,235)");
    runtime->settings().setGridSnapEnabled(true);

    // ---- 方向键微调：每次 10cm ----
    {
        const QPointF before(displayAt(QStringLiteral("actor-001")).x,
                             displayAt(QStringLiteral("actor-001")).y);
        QTest::keyClick(view, Qt::Key_Right);
        QTest::keyClick(view, Qt::Key_Down);
        const QPointF after(displayAt(QStringLiteral("actor-001")).x,
                            displayAt(QStringLiteral("actor-001")).y);
        CHECK(near(after.x() - before.x(), 10.0, 0.01) && near(after.y() - before.y(), 10.0, 0.01),
              "方向键微调每次 10cm（Right +10x，Down +10y）");
    }

    // ---- 多选整体移动（吸附开：-310 吸到 -300，相对位置保持） ----
    const QPointF zsBefore(displayAt(QStringLiteral("actor-001")).x,
                           displayAt(QStringLiteral("actor-001")).y); // (465,245)
    const QPointF lsBefore(kLisi);
    QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, posOf(kLisi + QPointF(0.0, -25.0)));
    QTest::mouseMove(vp, posOf(kLisi + QPointF(20.0, -25.0)));
    QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, posOf(kLisi + QPointF(40.0, -25.0)));
    // 注意：上一步未 Ctrl，按下李四即单选李四——这验证"点未选中对象即单选并拖动"
    CHECK(ids() == QList<QString> { QStringLiteral("actor-002") }, "点未选中对象：单选并进入拖拽");
    CHECK(near(displayAt(QStringLiteral("actor-002")).x, -300.0)
              && near(displayAt(QStringLiteral("actor-002")).y, 400.0),
          "单对象拖拽吸附 (-310→-300)");
    // 张三+李四双选后整体拖
    QTest::mouseClick(vp, Qt::LeftButton, Qt::NoModifier, posOf(zsBefore + QPointF(0.0, -25.0)));
    QTest::mouseClick(vp, Qt::LeftButton, Qt::ControlModifier, posOf(QPointF(-300.0, 375.0)));
    CHECK(ids().size() == 2, "双选（张三+李四）");
    QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, posOf(QPointF(-300.0, 375.0)));
    QTest::mouseMove(vp, posOf(QPointF(-250.0, 375.0)));
    QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, posOf(QPointF(-260.0, 375.0)));
    {
        // 主对象李四 (-300,400) → 原始目标 (-260,400)，吸附到 -300 网格？|-260-(-300)|=40>20
        // 不吸 → delta = (+40, 0)
        const QPointF zsAfter(displayAt(QStringLiteral("actor-001")).x,
                              displayAt(QStringLiteral("actor-001")).y);
        const QPointF lsAfter(displayAt(QStringLiteral("actor-002")).x,
                              displayAt(QStringLiteral("actor-002")).y);
        CHECK(near(lsAfter.x() - (-300.0), 40.0) && near(lsAfter.y(), 400.0),
              "多选整体移动：李四 (+40,0)");
        CHECK(near(zsAfter.x() - zsBefore.x(), 40.0) && near(zsAfter.y() - zsBefore.y(), 0.0),
              "多选整体移动：张三同步 (+40,0)，相对位置保持");
    }
    QTest::mouseClick(vp, Qt::LeftButton, Qt::NoModifier, posOf(QPointF(-600.0, 800.0))); // 清空

    // ---- 旋转：拖拽白三角（张三显示朝向 0，三角在圆外 +y 33~48cm 处，按 (0,42)） ----
    {
        const QPointF center(displayAt(QStringLiteral("actor-001")).x,
                             displayAt(QStringLiteral("actor-001")).y); // (505,245)
        QTest::mouseClick(vp, Qt::LeftButton, Qt::NoModifier, posOf(center + QPointF(0.0, -25.0)));
        QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, posOf(center + QPointF(0.0, 42.0)));
        QTest::mouseMove(vp, posOf(center + QPointF(-25.0, 10.0)));
        QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier,
                            posOf(center + QPointF(-50.0, 0.0))); // 向量(-50,0) → 90°
        const double rotation = displayAt(QStringLiteral("actor-001")).rotation;
        CHECK(near(rotation, 90.0, 3.0), "拖白三角旋转 → 朝向 90°（需求角度制）");
        CHECK(near(scene->itemById(QStringLiteral("actor-001"))->rotation(), 90.0, 3.0),
              "图元朝向同步（90° 三角朝左）");
        CHECK(near(window.findChild<QDoubleSpinBox*>(QStringLiteral("actorRotSpin"))->value(),
                   90.0, 3.0),
              "Inspector 朝向值同步为 90°");
        view->grab().save(QDir::temp().absoluteFilePath(QStringLiteral("stage3-interaction-rot90.png")));
    }

    // ---- 旋转吸附：释放在非 15° 整数倍方向 → 吸附到最近倍数 ----
    {
        // 当前显示朝向 90°（朝左），圆外手柄在中心 +x 负向 33~48cm 处
        const QPointF center(displayAt(QStringLiteral("actor-001")).x,
                             displayAt(QStringLiteral("actor-001")).y);
        QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, posOf(center + QPointF(-42.0, 0.0)));
        QTest::mouseMove(vp, posOf(center + QPointF(-40.0, -15.0)));
        // 释放方向向量 (-59.1,-10.4) ≈ 100° → 吸附到 105°
        QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier,
                            posOf(center + QPointF(-59.1, -10.4)));
        const double rotation = displayAt(QStringLiteral("actor-001")).rotation;
        CHECK(near(rotation, 105.0, 1.0), "旋转吸附：~100° 吸附到 105°（15° 整数倍）");
        CHECK(near(scene->itemById(QStringLiteral("actor-001"))->rotation(), 105.0, 1.0),
              "图元朝向同步为吸附值（105°）");
        CHECK(near(window.findChild<QDoubleSpinBox*>(QStringLiteral("actorRotSpin"))->value(),
                   105.0, 1.0),
              "Inspector 朝向值同步为 105°");
    }

    // ---- 悬停/拖拽指针反馈（此刻 actor-001 显示在 (505,245)、朝向 105°，
    //      圆外手柄在 105° 方向约 42cm 处：dir = (-sin105°, cos105°)） ----
    {
        const QPointF center(displayAt(QStringLiteral("actor-001")).x,
                             displayAt(QStringLiteral("actor-001")).y);
        const double rad = 105.0 * 3.14159265358979323846 / 180.0;
        const QPointF handle = center + QPointF(-std::sin(rad), std::cos(rad)) * 42.0;
        const QPointF body = center + QPointF(0.0, -25.0); // 圆边缘，避开手柄
        const QPointF blank(-600.0, 800.0);

        QTest::mouseMove(vp, posOf(handle));
        CHECK(vp->cursor().shape() == Qt::PointingHandCursor, "悬停朝向手柄 → 旋转意图指针");
        QTest::mouseMove(vp, posOf(body));
        CHECK(vp->cursor().shape() == Qt::SizeAllCursor, "悬停对象本体 → 移动意图指针");
        QTest::mouseMove(vp, posOf(blank));
        CHECK(vp->cursor().shape() == Qt::ArrowCursor, "悬停空白 → 恢复默认箭头");

        // Moving 期间保持移动指针；拖回起点松开（位移为零不提交，数据不变）
        runtime->settings().setGridSnapEnabled(false);
        QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, posOf(body));
        QTest::mouseMove(vp, posOf(body + QPointF(40.0, 0.0)));
        CHECK(vp->cursor().shape() == Qt::SizeAllCursor, "拖拽移动中保持移动指针");
        QTest::mouseMove(vp, posOf(body));
        QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, posOf(body));
        runtime->settings().setGridSnapEnabled(true);
        CHECK(near(displayAt(QStringLiteral("actor-001")).x, center.x(), 0.01)
                  && near(displayAt(QStringLiteral("actor-001")).y, center.y(), 0.01),
              "移动指针用例零位移松开：位置不变");

        // Rotating 期间保持旋转指针；回到手柄按下点松开（角度吸附回原值，数据不变）
        QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, posOf(handle));
        QTest::mouseMove(vp, posOf(center + QPointF(40.0, 40.0)));
        CHECK(vp->cursor().shape() == Qt::PointingHandCursor, "拖拽旋转中保持旋转指针");
        QTest::mouseMove(vp, posOf(handle));
        QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, posOf(handle));
        CHECK(near(displayAt(QStringLiteral("actor-001")).rotation, 105.0, 1.0),
              "旋转指针用例原地松开：朝向不变");
        CHECK(vp->cursor().shape() == Qt::PointingHandCursor,
              "手势结束按悬停位置恢复指针（手柄上 → 旋转指针）");
    }

    // ---- Inspector 表单修改立即生效 ----
    auto* xSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("actorXSpin"));
    auto* rotSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("actorRotSpin"));
    xSpin->setValue(123.4);
    CHECK(near(displayAt(QStringLiteral("actor-001")).x, 123.4, 0.01),
          "Inspector 位置 X 修改立即生效（打帧当前时刻）");
    CHECK(near(scene->itemById(QStringLiteral("actor-001"))->pos().x(), 123.4, 0.01),
          "Inspector 修改在主视口体现");
    rotSpin->setValue(45.0);
    CHECK(near(displayAt(QStringLiteral("actor-001")).rotation, 45.0, 0.01),
          "Inspector 朝向修改立即生效");

    // 改名 + Hierarchy 同步
    auto* nameEdit = window.findChild<QLineEdit*>(QStringLiteral("actorNameEdit"));
    nameEdit->setText(QStringLiteral("赵六"));
    QTest::keyClick(nameEdit, Qt::Key_Return);
    CHECK(objectAt(QStringLiteral("actor-001"))->name == QStringLiteral("赵六"),
          "Inspector 改名立即生效");
    bool hierarchyShowsNewName = false;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = tree->topLevelItem(i);
        if (item->data(0, Qt::UserRole).toString() == QStringLiteral("actor-001"))
            hierarchyShowsNewName = item->text(0) == QStringLiteral("赵六");
    }
    CHECK(hierarchyShowsNewName, "Hierarchy 名字与 Inspector 一致");

    // 重名拒绝并提示（模态 QMessageBox 由定时器关闭）
    QTimer::singleShot(300, [] {
        if (QWidget* modal = QApplication::activeModalWidget())
            modal->close();
    });
    nameEdit->setText(QStringLiteral("李四"));
    QTest::keyClick(nameEdit, Qt::Key_Return);
    CHECK(objectAt(QStringLiteral("actor-001"))->name == QStringLiteral("赵六"),
          "重名被拒绝（数据不变）");
    CHECK(nameEdit->text() == QStringLiteral("赵六"), "重名拒绝后输入框恢复原名");

    // ---- 屏风表单：大小字段 + 修改生效 ----
    QTest::mouseClick(vp, Qt::LeftButton, Qt::NoModifier, posOf(kScreen1));
    CHECK(stack->currentIndex() == 2, "单选屏风 → Inspector 屏风表单");
    CHECK(window.findChild<QLineEdit*>(QStringLiteral("screenNameEdit"))->text()
              == QStringLiteral("屏风1"),
          "Inspector 显示屏风名");
    auto* wSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("screenWSpin"));
    CHECK(near(wSpin->value(), 200.0, 0.01), "屏风表单显示大小 宽 200");
    wSpin->setValue(250.0);
    CHECK(near(objectAt(QStringLiteral("screen-001"))->size->width(), 250.0, 0.01),
          "屏风大小修改立即生效");

    // ---- 复制/粘贴/删除（快捷键经菜单接线） ----
    QTest::mouseClick(vp, Qt::LeftButton, Qt::NoModifier,
                      posOf(QPointF(displayAt(QStringLiteral("actor-001")).x,
                                    displayAt(QStringLiteral("actor-001")).y)
                            + QPointF(0.0, -25.0)));
    view->setFocus();
    window.activateWindow(); // 模态框关闭后 offscreen 窗口会失去激活，快捷键需要窗口激活
    QTest::qWaitForWindowActive(&window);
    view->setFocus();
    const qsizetype countBefore = runtime->fileData().objects.size();
    const QPointF pasteSource(objectAt(QStringLiteral("actor-001"))->transform.x,
                              objectAt(QStringLiteral("actor-001"))->transform.y);
    const QList<Keyframe> copySourceKeys =
        objectAt(QStringLiteral("actor-001"))->keyframes; // 含此前打的关键帧
    QTest::keyClick(view, Qt::Key_C, Qt::ControlModifier);
    CHECK(core->clipboardHasContent(), "Ctrl+C 复制进内部剪贴板（编辑菜单接线）");
    QTest::keyClick(view, Qt::Key_V, Qt::ControlModifier);
    CHECK(runtime->fileData().objects.size() == countBefore + 1, "Ctrl+V 粘贴新增对象");
    {
        const ObjectData& pasted = runtime->fileData().objects.last();
        CHECK(near(pasted.transform.x, pasteSource.x() + 20.0, 0.01)
                  && near(pasted.transform.y, pasteSource.y() + 20.0, 0.01),
              "粘贴默认偏移 (x+20, y+20)（静态 transform）");
        bool keysOffset = pasted.keyframes.size() == copySourceKeys.size();
        for (int i = 0; keysOffset && i < copySourceKeys.size(); ++i)
            keysOffset = near(pasted.keyframes.at(i).x, copySourceKeys.at(i).x + 20.0, 0.01)
                && near(pasted.keyframes.at(i).y, copySourceKeys.at(i).y + 20.0, 0.01)
                && near(pasted.keyframes.at(i).t, copySourceKeys.at(i).t, 0.001);
        CHECK(keysOffset, "粘贴：关键帧位置随副本偏移 +20/+20（时刻不变）");
        CHECK(pasted.id != QStringLiteral("actor-001"), "粘贴生成新 id");
        CHECK(pasted.name == QStringLiteral("赵六2"), "粘贴生成唯一名（赵六2）");
        CHECK(ids() == QList<QString> { pasted.id }, "粘贴后选中副本");
    }
    QTest::keyClick(view, Qt::Key_Delete);
    CHECK(runtime->fileData().objects.size() == countBefore, "Del 删除选中对象（编辑菜单接线）");
    CHECK(ids().isEmpty(), "删除后选中清空");

    // ---- Hierarchy 正向同步：点树行 → 视口选中 ----
    QTreeWidgetItem* wangwuItem = nullptr;
    QTreeWidgetItem* screenItem = nullptr;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = tree->topLevelItem(i);
        const QString id = item->data(0, Qt::UserRole).toString();
        if (id == QStringLiteral("actor-003"))
            wangwuItem = item;
        if (id == QStringLiteral("screen-001"))
            screenItem = item;
    }
    CHECK(wangwuItem && screenItem, "Hierarchy 行按 id 可查");
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                      tree->visualItemRect(wangwuItem).center());
    CHECK(ids() == QList<QString> { QStringLiteral("actor-003") }, "Hierarchy 点选同步到视口选中");
    CHECK(window.findChild<QLineEdit*>(QStringLiteral("actorNameEdit"))->text()
              == QStringLiteral("王五"),
          "Inspector 跟随 Hierarchy 选中显示王五");
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::ControlModifier,
                      tree->visualItemRect(screenItem).center());
    CHECK(ids().size() == 2, "Hierarchy Ctrl 多选同步");
    CHECK(stack->currentIndex() == 0, "多选时 Inspector 清空");

    // ---- 选中高亮截图（人工复核用） ----
    QTest::mouseClick(vp, Qt::LeftButton, Qt::NoModifier,
                      posOf(QPointF(displayAt(QStringLiteral("actor-001")).x,
                                    displayAt(QStringLiteral("actor-001")).y)
                            + QPointF(0.0, -25.0)));
    QTest::mouseClick(vp, Qt::LeftButton, Qt::ControlModifier, posOf(QPointF(-260.0, 375.0)));
    CHECK(scene->itemById(QStringLiteral("actor-001"))->isHighlighted()
              && scene->itemById(QStringLiteral("actor-002"))->isHighlighted()
              && !scene->itemById(QStringLiteral("screen-001"))->isHighlighted(),
          "高亮标记与选中一致");
    view->grab().save(QDir::temp().absoluteFilePath(QStringLiteral("stage3-interaction-selected.png")));

    // ---- Hierarchy 树形交互：拖拽整理走命令；空白处框选；点对象即选中 ----
    // 树行按 id 可查（递归遍历，嵌套后子行也能找到）
    const auto rowOf = [tree](const QString& id) -> QTreeWidgetItem* {
        QTreeWidgetItemIterator it(tree);
        while (*it) {
            if ((*it)->data(0, Qt::UserRole).toString() == id)
                return *it;
            ++it;
        }
        return nullptr;
    };
    const auto fileIdOrder = [runtime]() {
        QList<QString> order;
        for (const ObjectData& object : runtime->fileData().objects)
            order.append(object.id);
        return order;
    };

    // 合成 drop 辅助：QApplication 对 DragEnter/Drop 有特殊路由（无活动拖拽时
    // sendEvent 直接丢弃，送不到 dropEvent），Qt 自带 DnD 测试的做法是走平台入口
    // QWindowSystemInterface::handleDrag/handleDrop（offscreen 可用，同步投递）。
    // 真实拖拽手势（长按行拖动、QDrag::exec 模态）本身列入手测清单。
    // zone: 0=行上（成为子对象） 1=行间上方 2=行间下方 3=空白（顶层末尾）
    const auto simulateDrop = [tree](QTreeWidgetItem* target, int zone) {
        QPoint pos;
        if (target && zone != 3) {
            const QRect rect = tree->visualItemRect(target);
            if (zone == 1)
                pos = QPoint(rect.center().x(), rect.top() + 1);
            else if (zone == 2)
                pos = QPoint(rect.center().x(), rect.bottom() - 1);
            else
                pos = rect.center();
        } else {
            int bottom = 0;
            QTreeWidgetItemIterator it(tree);
            while (*it) {
                bottom = qMax(bottom, tree->visualItemRect(*it).bottom());
                ++it;
            }
            pos = QPoint(tree->viewport()->width() / 2,
                         qMin(bottom + 20, tree->viewport()->height() - 2));
        }
        QMimeData* mime = tree->model()->mimeData(tree->selectionModel()->selectedIndexes());
        QWindow* window = tree->window()->windowHandle();
        // handleDrag/handleDrop 的坐标是窗口本地坐标（平台插件在 window 坐标系注入）
        const QPoint local = tree->viewport()->mapTo(tree->window(), pos);
        QWindowSystemInterface::handleDrag(window, mime, local, Qt::MoveAction,
                                           Qt::LeftButton, Qt::NoModifier);
        QWindowSystemInterface::handleDrop(window, mime, local, Qt::MoveAction,
                                           Qt::LeftButton, Qt::NoModifier);
        delete mime;
    };

    // 行上 press+move（未达拖拽阈值）不起框选：选中保持按下时的行
    {
        const QRect from = tree->visualItemRect(rowOf(QStringLiteral("actor-001")));
        QTest::mousePress(tree->viewport(), Qt::LeftButton, Qt::NoModifier, from.center());
        QTest::mouseMove(tree->viewport(), from.center() + QPoint(2, 2));
        QTest::mouseRelease(tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                            from.center() + QPoint(2, 2));
        CHECK(ids() == QList<QString> { QStringLiteral("actor-001") },
              "行上按下小位移：不框选、选中保持按下行");
    }

    // 空白处框选：从末行下方空白向上拖，覆盖 王五/屏风1/屏风2 三行
    {
        const QRect lastRow = tree->visualItemRect(rowOf(QStringLiteral("screen-002")));
        const QPoint empty(tree->viewport()->width() / 2, lastRow.bottom() + 30);
        const QPoint target(10, tree->visualItemRect(rowOf(QStringLiteral("actor-003"))).center().y());
        QTest::mousePress(tree->viewport(), Qt::LeftButton, Qt::NoModifier, empty);
        QTest::mouseMove(tree->viewport(), QPoint(10, lastRow.top() - 10));
        QTest::mouseMove(tree->viewport(), target);
        QTest::mouseRelease(tree->viewport(), Qt::LeftButton, Qt::NoModifier, target);
        CHECK(ids().size() == 3 && ids().contains(QStringLiteral("actor-003"))
                  && ids().contains(QStringLiteral("screen-001"))
                  && ids().contains(QStringLiteral("screen-002")),
              "Hierarchy 空白处框选多行（王五+屏风1+屏风2）");
    }

    // 点对象即选中（单选替换）
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                      tree->visualItemRect(rowOf(QStringLiteral("screen-001"))).center());
    CHECK(ids() == QList<QString> { QStringLiteral("screen-001") }, "Hierarchy 点对象即选中");

    // ---- 空对象（分组）：创建 / 树形拖拽 / 级联删除 / 展开状态 ----
    // 创建经 EditorCore 业务入口（Create 对话框是模态的，列入手测清单）
    CHECK(core->createObject(ObjectType::Group, QPointF(999.0, 999.0)), "创建空对象（业务入口）");
    const QString group1 = runtime->fileData().objects.last().id;
    CHECK(runtime->fileData().objects.last().type == ObjectType::Group
              && runtime->fileData().objects.last().parentId.isEmpty(),
          "空对象落在顶层");
    CHECK(runtime->fileData().objects.last().name == QStringLiteral("空对象1"), "空对象自动命名");
    CHECK(tree->topLevelItemCount() == 6, "Hierarchy 顶层新增空对象行");
    CHECK(scene->itemById(group1) == nullptr, "空对象不进视口（无图元）");
    CHECK(ids() == QList<QString> { group1 }, "创建空对象后自动选中");
    CHECK(stack->currentIndex() == 3, "单选空对象 → Inspector 只有名称表单");
    CHECK(window.findChild<QLineEdit*>(QStringLiteral("groupNameEdit"))->text()
              == QStringLiteral("空对象1"),
          "空对象 Inspector 显示名称");

    // 拖到行上 → 成为子对象（树结构与 parentId 双重验证）
    core->selection()->select(QStringLiteral("actor-001"));
    simulateDrop(rowOf(group1), 0);
    CHECK(objectAt(QStringLiteral("actor-001"))->parentId == group1,
          "拖到行上成为子对象（parentId 落数据）");
    {
        QTreeWidgetItem* groupItem = rowOf(group1);
        CHECK(groupItem && groupItem->childCount() == 1
                  && groupItem->child(0)->data(0, Qt::UserRole).toString()
                         == QStringLiteral("actor-001"),
              "树结构：子对象行挂在分组行下");
        CHECK(fileIdOrder()
                  == QList<QString>({ QStringLiteral("actor-002"), QStringLiteral("actor-003"),
                                      QStringLiteral("screen-001"), QStringLiteral("screen-002"),
                                      group1, QStringLiteral("actor-001") }),
              "FileData 顺序 = DFS 显示顺序（子对象排到该行子树末尾）");
    }

    // 拖到行间 → 同级插入排序（FileData 顺序变化验证）
    core->selection()->select(QStringLiteral("screen-001"));
    simulateDrop(rowOf(QStringLiteral("actor-002")), 1); // Above 李四
    CHECK(objectAt(QStringLiteral("screen-001"))->parentId.isEmpty(),
          "行间插入保持同级（顶层）");
    CHECK(fileIdOrder()
              == QList<QString>({ QStringLiteral("screen-001"), QStringLiteral("actor-002"),
                                  QStringLiteral("actor-003"), QStringLiteral("screen-002"),
                                  group1, QStringLiteral("actor-001") }),
          "拖到行间排序（FileData 顺序变化）");

    // 多选整体移动到同一分组（按当前顺序排入子树末尾）
    core->selection()->setSelection(
        { QStringLiteral("actor-002"), QStringLiteral("actor-003") });
    simulateDrop(rowOf(group1), 0);
    CHECK(objectAt(QStringLiteral("actor-002"))->parentId == group1
              && objectAt(QStringLiteral("actor-003"))->parentId == group1,
          "多选整体移动到同一分组");
    {
        QTreeWidgetItem* groupItem = rowOf(group1);
        CHECK(groupItem->childCount() == 3
                  && groupItem->child(0)->data(0, Qt::UserRole).toString()
                         == QStringLiteral("actor-001")
                  && groupItem->child(1)->data(0, Qt::UserRole).toString()
                         == QStringLiteral("actor-002")
                  && groupItem->child(2)->data(0, Qt::UserRole).toString()
                         == QStringLiteral("actor-003"),
              "多选按当前顺序排入子树末尾");
    }

    // 环拖拽被拒绝：拖到自身后代之下，数据不变
    {
        const QList<QString> before = fileIdOrder();
        core->selection()->select(group1);
        simulateDrop(rowOf(QStringLiteral("actor-002")), 0);
        CHECK(fileIdOrder() == before && objectAt(group1)->parentId.isEmpty(),
              "环拖拽被拒绝（拖到自身后代之下，数据不变）");
    }

    // 分组嵌套：空对象2 挂到 空对象1 下
    CHECK(core->createObject(ObjectType::Group, QPointF(0.0, 0.0)), "创建空对象2");
    const QString group2 = runtime->fileData().objects.last().id;
    core->selection()->select(group2);
    simulateDrop(rowOf(group1), 0);
    CHECK(objectAt(group2)->parentId == group1, "分组可嵌套（空对象2 挂入 空对象1）");
    CHECK(rowOf(group1)->childCount() == 4, "嵌套后分组行子行数正确");

    // 含分组嵌套的 Hierarchy 树（展开/折叠三角可见）与视口（分组无图元）截图
    tree->grab().save(QDir::temp().absoluteFilePath(QStringLiteral("stage4-hierarchy-groups.png")));
    view->grab().save(QDir::temp().absoluteFilePath(QStringLiteral("stage4-viewport-groups.png")));

    // 重建后展开状态保持：折叠分组 → 改名触发重建 → 仍折叠
    core->selection()->clear();
    rowOf(group1)->setExpanded(false);
    CHECK(core->setObjectName(group1, QStringLiteral("第一幕组合")), "改名触发重建");
    {
        QTreeWidgetItem* groupItem = rowOf(group1);
        CHECK(groupItem && !groupItem->isExpanded(), "重建后折叠状态保持");
        CHECK(groupItem->text(0) == QStringLiteral("第一幕组合"), "重建后名字更新");
    }

    // 选中视口子对象 → Hierarchy 自动展开父链并高亮对应行
    // （actor-002 在折叠的 空对象1 下，点击视口图元真实命中）
    QTest::mouseClick(vp, Qt::LeftButton, Qt::NoModifier, posOf(QPointF(-260.0, 400.0)));
    CHECK(ids() == QList<QString> { QStringLiteral("actor-002") }, "视口选中子对象");
    CHECK(rowOf(group1)->isExpanded(), "选中子对象自动展开父链");
    CHECK(rowOf(QStringLiteral("actor-002"))->isSelected(), "Hierarchy 高亮对应行");

    // 拖到空白处 → 移到顶层末尾
    core->selection()->select(QStringLiteral("screen-002"));
    simulateDrop(nullptr, 3);
    CHECK(objectAt(QStringLiteral("screen-002"))->parentId.isEmpty()
              && fileIdOrder().last() == QStringLiteral("screen-002"),
          "拖到空白处移到顶层末尾");

    // 删除分组级联（数据 + 树形验证）：空对象1 含 3 演员 + 空对象2
    core->selection()->select(group1);
    CHECK(core->deleteSelection(), "删除分组（业务入口）");
    CHECK(runtime->fileData().objects.size() == 2, "级联删除整棵子树（数据）");
    CHECK(objectAt(QStringLiteral("actor-001")) == nullptr
              && objectAt(group2) == nullptr,
          "子孙对象一并删除");
    CHECK(tree->topLevelItemCount() == 2, "Hierarchy 树同步（只剩两顶层行）");
    CHECK(ids().isEmpty(), "级联删除后选中清空");

    // ---- Create 对话框：四项（舞台道具禁用），空对象可选 ----
    // （对话框本身非模态实例化即可验证；两处右键菜单的模态弹出走手测）
    {
        CreateObjectDialog dialog;
        const QList<QRadioButton*> radios = dialog.findChildren<QRadioButton*>();
        CHECK(radios.size() == 4, "Create 对话框四项");
        CHECK(radios.at(0)->text() == QStringLiteral("演员")
                  && radios.at(1)->text() == QStringLiteral("屏风")
                  && radios.at(2)->text() == QStringLiteral("空对象")
                  && radios.at(3)->text() == QStringLiteral("舞台道具"),
              "四项文案：演员/屏风/空对象/舞台道具");
        CHECK(!radios.at(3)->isEnabled(), "舞台道具禁用");
        CHECK(dialog.selectedType() == ObjectType::Actor, "默认选中演员");
        radios.at(2)->setChecked(true);
        CHECK(dialog.selectedType() == ObjectType::Group, "空对象可选");
    }

    // ---- 阶段4：撤销/重做（菜单快捷键接线 + 可用性 + 面板联动） ----
    QAction* undoAction = nullptr;
    QAction* redoAction = nullptr;
    for (QAction* action : window.findChildren<QAction*>()) {
        if (action->text() == QStringLiteral("撤销(&U)"))
            undoAction = action;
        else if (action->text() == QStringLiteral("重做(&R)"))
            redoAction = action;
    }
    CHECK(undoAction && redoAction, "编辑菜单撤销/重做 action 存在");
    CHECK(undoAction->shortcut() == QKeySequence::Undo
              && redoAction->shortcut() == QKeySequence::Redo,
          "撤销/重做快捷键为 Ctrl+Z / Ctrl+Y");
    CHECK(core->canUndo() && undoAction->isEnabled()
              && !core->canRedo() && !redoAction->isEnabled(),
          "初始：有历史命令可撤销、无可重做（两项可用性正确）");

    // 合成 Ctrl+Z 逐级撤销到底（走菜单快捷键接线），再 Ctrl+Y 逐级重做回来
    const FileData stateBefore = runtime->fileData();
    view->setFocus();
    int undoSteps = 0;
    while (core->canUndo() && undoSteps < 200) {
        QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier);
        ++undoSteps;
    }
    CHECK(undoSteps > 0 && !core->canUndo(), "Ctrl+Z 逐级撤销到底");
    CHECK(!undoAction->isEnabled() && redoAction->isEnabled(),
          "撤销到底后：撤销项禁用、重做项启用");
    view->setFocus();
    int redoSteps = 0;
    while (core->canRedo() && redoSteps < 200) {
        QTest::keyClick(view, Qt::Key_Y, Qt::ControlModifier);
        ++redoSteps;
    }
    CHECK(redoSteps == undoSteps, "Ctrl+Y 重做步数与撤销一致");
    CHECK(runtime->fileData() == stateBefore, "逐级重做后 FileData 与撤销前逐字段一致");
    CHECK(!redoAction->isEnabled() && undoAction->isEnabled(),
          "重做到底后：重做项禁用、撤销项恢复");

    // 视口/Inspector 随撤销重建刷新：改名 → Ctrl+Z → 数据与表单一并还原
    QTest::mouseClick(vp, Qt::LeftButton, Qt::NoModifier, posOf(kScreen1));
    CHECK(ids() == QList<QString> { QStringLiteral("screen-001") }, "撤销测试：选中屏风1");
    auto* screenNameEdit = window.findChild<QLineEdit*>(QStringLiteral("screenNameEdit"));
    screenNameEdit->setText(QStringLiteral("屏风甲"));
    QTest::keyClick(screenNameEdit, Qt::Key_Return);
    CHECK(objectAt(QStringLiteral("screen-001"))->name == QStringLiteral("屏风甲"),
          "撤销测试：Inspector 改名生效");
    view->setFocus();
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier);
    CHECK(objectAt(QStringLiteral("screen-001"))->name == QStringLiteral("屏风1"),
          "Ctrl+Z 撤销改名（数据层生效）");
    CHECK(window.findChild<QLineEdit*>(QStringLiteral("screenNameEdit"))->text()
              == QStringLiteral("屏风1"),
          "Inspector 随撤销重建刷新");
    CHECK(scene->itemById(QStringLiteral("screen-001")) != nullptr, "视口图元随撤销重建");
    view->setFocus();
    QTest::keyClick(view, Qt::Key_Y, Qt::ControlModifier);
    CHECK(objectAt(QStringLiteral("screen-001"))->name == QStringLiteral("屏风甲"),
          "Ctrl+Y 重做改名");
    CHECK(window.findChild<QLineEdit*>(QStringLiteral("screenNameEdit"))->text()
              == QStringLiteral("屏风甲"),
          "Inspector 随重做重建刷新");

    // 新命令清空 redo 区：撤销一步后方向键微调产生新命令
    view->setFocus();
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier); // 撤销改名 → redo 可用
    CHECK(core->canRedo() && redoAction->isEnabled(), "撤销一步后重做可用");
    QTest::keyClick(view, Qt::Key_Right); // 微调屏风1 +10cm（游标处打帧，不吸附）
    CHECK(near(displayAt(QStringLiteral("screen-001")).x, kScreen1.x() + 10.0, 0.01),
          "方向键微调生效（打帧当前时刻，新命令入栈）");
    CHECK(!core->canRedo() && !redoAction->isEnabled(), "新命令清空 redo 区（重做项禁用）");

    // 文件整体替换清栈：newFile → 撤销/重做均禁用；新命令后撤销恢复
    runtime->newFile();
    CHECK(runtime->fileData().objects.isEmpty(), "newFile 后为默认空舞台");
    CHECK(!core->canUndo() && !core->canRedo(), "newFile 清空撤销栈");
    CHECK(!undoAction->isEnabled() && !redoAction->isEnabled(),
          "newFile 后撤销/重做菜单项均禁用");
    CHECK(core->createObject(ObjectType::Actor, QPointF(0.0, 0.0)), "清栈后创建演员（新命令入栈）");
    CHECK(undoAction->isEnabled(), "新命令后撤销项启用");
    view->setFocus();
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier);
    CHECK(runtime->fileData().objects.isEmpty(), "Ctrl+Z 撤销创建");
    CHECK(ids().isEmpty(), "撤销创建后选中剔除");
    CHECK(stack->currentIndex() == 0, "撤销创建后 Inspector 清空");
    CHECK(!undoAction->isEnabled() && redoAction->isEnabled(),
          "撤销到底后：撤销项禁用、重做项启用");

    // ==================== 阶段5：时间轴 ====================
    auto* timeline = window.findChild<TimelineWidget*>();
    auto* canvas = window.findChild<QWidget*>(QStringLiteral("timelineCanvas"));
    auto* keyButton = window.findChild<QPushButton*>(QStringLiteral("keyframeButton"));
    auto* playButton = window.findChild<QPushButton*>(QStringLiteral("playButton"));
    auto* speedCombo = window.findChild<QComboBox*>(QStringLiteral("speedCombo"));
    auto* timeReadout = window.findChild<QLabel*>(QStringLiteral("timeReadout"));
    auto* addLabelButton = window.findChild<QPushButton*>(QStringLiteral("addLabelButton"));
    CHECK(timeline && canvas && keyButton && playButton && speedCombo && timeReadout
              && addLabelButton,
          "阶段5：时间轴面板组件装配完整");
    TimelinePlayer* player = core->timelinePlayer();
    player->seekTo(0.0);

    // 创建 2 个演员（验收场景：各打多个关键帧）
    CHECK(core->createObject(ObjectType::Actor, QPointF(0.0, 100.0)), "阶段5：创建演员甲");
    const QString actorA = runtime->fileData().objects.last().id;
    CHECK(core->createObject(ObjectType::Actor, QPointF(200.0, 100.0)), "阶段5：创建演员乙");
    const QString actorB = runtime->fileData().objects.last().id;

    // ---- 打帧按钮：可用性 + 以显示值在游标处打帧（不弹窗） ----
    core->selection()->select(actorA);
    player->seekTo(1.0);
    CHECK(keyButton->isEnabled(), "打帧按钮：单选演员可用");
    runtime->settings().setKeyframePromptSuppressed(false); // 按钮语义本身不弹窗
    keyButton->click();
    {
        const QList<Keyframe>& keys = objectAt(actorA)->keyframes;
        CHECK(keys.size() == 1 && near(keys.first().t, 1.0, 0.001)
                  && near(keys.first().x, 0.0, 0.01) && near(keys.first().y, 100.0, 0.01),
              "打帧按钮：以当前显示值在游标处打帧（t=1）");
    }
    CHECK(timeReadout->text().contains(QStringLiteral("1.00")), "时刻读数显示当前时刻");
    core->selection()->setSelection({ actorA, actorB });
    CHECK(!keyButton->isEnabled(), "打帧按钮：多选禁用");
    core->selection()->clear();
    CHECK(!keyButton->isEnabled(), "打帧按钮：空选禁用");

    // ---- 弹窗询问（需求第 20 条）：拖拽后选"是"打帧 ----
    player->seekTo(3.0);
    QTimer::singleShot(300, [] {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
            box->button(QMessageBox::Yes)->click();
    });
    {
        // 演员甲当前显示 = 帧@1 (0,100)；拖到 (50,150)（吸附关闭保证精确落点）
        runtime->settings().setGridSnapEnabled(false);
        QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, posOf(QPointF(0.0, 100.0)));
        QTest::mouseMove(vp, posOf(QPointF(20.0, 120.0)));
        QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, posOf(QPointF(50.0, 150.0)));
        runtime->settings().setGridSnapEnabled(true);
    }
    {
        const QList<Keyframe>& keys = objectAt(actorA)->keyframes;
        CHECK(keys.size() == 2 && near(keys.at(1).t, 3.0, 0.001)
                  && near(keys.at(1).x, 50.0) && near(keys.at(1).y, 150.0),
              "弹窗询问选\"是\"：在游标处打帧");
        CHECK(near(scene->itemById(actorA)->pos().x(), 50.0)
                  && near(scene->itemById(actorA)->pos().y(), 150.0),
              "选\"是\"后图元落新位置");
    }

    // ---- 弹窗询问：选"否"零变更 + 图元回弹到插值显示位置 ----
    player->seekTo(5.0); // 显示 = 末帧 (50,150)
    QTimer::singleShot(300, [] {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
            box->button(QMessageBox::No)->click();
    });
    {
        runtime->settings().setGridSnapEnabled(false);
        QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, posOf(QPointF(50.0, 150.0)));
        QTest::mouseMove(vp, posOf(QPointF(60.0, 160.0)));
        QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, posOf(QPointF(80.0, 180.0)));
        runtime->settings().setGridSnapEnabled(true);
    }
    CHECK(objectAt(actorA)->keyframes.size() == 2, "弹窗询问选\"否\"：零变更");
    CHECK(near(scene->itemById(actorA)->pos().x(), 50.0)
              && near(scene->itemById(actorA)->pos().y(), 150.0),
          "选\"否\"图元回弹到插值显示位置");
    CHECK(!runtime->settings().keyframePromptSuppressed(), "选\"否\"不写入\"不再提示\"");

    // 模态弹窗关闭后 offscreen 窗口失去激活：快捷键路由前重新激活（既有先例）
    const auto reactivate = [&window, view] {
        window.activateWindow();
        QTest::qWaitForWindowActive(&window);
        view->setFocus();
    };
    reactivate();

    // ---- 游标拖动实时联动视口与 Inspector ----
    // 演员乙：@0 (200,100,0)，@2 (300,100,90)
    core->selection()->select(actorB);
    player->seekTo(0.0);
    keyButton->click();
    player->seekTo(2.0);
    runtime->settings().setKeyframePromptSuppressed(true);
    CHECK(core->setTransformsAtCursor({ { actorB, { 300.0, 100.0, 90.0 } } },
                                      [] { return true; }),
          "阶段5：演员乙打第二帧");
    {
        const double px = timeline->pxPerSec();
        const int rulerY = TimelineWidget::kRulerH / 2;
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier,
                          QPoint(TimelineWidget::kNameColW, rulerY));
        QTest::mouseMove(canvas, QPoint(TimelineWidget::kNameColW + int(0.5 * px), rulerY));
        QTest::mouseMove(canvas, QPoint(TimelineWidget::kNameColW + int(1.0 * px), rulerY));
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier,
                            QPoint(TimelineWidget::kNameColW + int(1.0 * px), rulerY));
        CHECK(near(player->currentTime(), 1.0, 0.001), "标尺拖动游标到 1s");
    }
    CHECK(near(scene->itemById(actorB)->pos().x(), 250.0, 0.01)
              && near(scene->itemById(actorB)->pos().y(), 100.0, 0.01),
          "游标拖动实时联动视口（中点位置 x=250）");
    CHECK(near(scene->itemById(actorB)->rotation(), 45.0, 0.01),
          "游标拖动联动朝向插值（0°→90° 中点 45°）");
    CHECK(near(window.findChild<QDoubleSpinBox*>(QStringLiteral("actorXSpin"))->value(),
               250.0, 0.01),
          "游标拖动联动 Inspector 数值");

    // ---- 关键帧点选 + Delete 删除 + 撤销/重做（游标处显示立即刷新） ----
    player->seekTo(1.0);
    const QPoint keyPos = timeline->canvasPosFor(actorB, 2.0);
    CHECK(keyPos.x() > TimelineWidget::kNameColW, "自测定位：演员乙轨道行存在");
    QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, keyPos);
    QTest::keyClick(canvas, Qt::Key_Delete);
    CHECK(objectAt(actorB)->keyframes.size() == 1, "Delete 删除选中关键帧");
    CHECK(near(displayAt(actorB).x, 200.0, 0.01),
          "删除后游标处显示立即刷新（只剩 @0，末后保持）");
    reactivate();
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier);
    CHECK(objectAt(actorB)->keyframes.size() == 2, "Ctrl+Z 撤销关键帧删除");
    CHECK(near(displayAt(actorB).x, 250.0, 0.01), "撤销后游标处插值立即刷新");
    QTest::keyClick(view, Qt::Key_Y, Qt::ControlModifier);
    CHECK(objectAt(actorB)->keyframes.size() == 1, "Ctrl+Y 重做关键帧删除");
    CHECK(near(displayAt(actorB).x, 200.0, 0.01), "重做后游标处显示立即刷新");

    // ---- 长按拖动关键帧改时刻（吸附 0.1s）+ 撤销重做 ----
    // 演员甲：@1 (0,100,0)，@3 (50,150,0)；把 @3 拖到 @4
    {
        const QPoint dragFrom = timeline->canvasPosFor(actorA, 3.0);
        const QPoint dragTo = timeline->canvasPosFor(actorA, 4.0);
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, dragFrom);
        QTest::mouseMove(canvas, dragFrom + QPoint(6, 0)); // 越过拖拽阈值
        QTest::mouseMove(canvas, dragTo);
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, dragTo);
        const QList<Keyframe>& keys = objectAt(actorA)->keyframes;
        CHECK(keys.size() == 2 && near(keys.at(1).t, 4.0, 0.001)
                  && near(keys.at(1).x, 50.0), // 拖拽落点经像素量化，eps 用 2cm
              "长按拖动关键帧改时刻（t: 3→4，值不变）");
    }
    reactivate();
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier);
    CHECK(near(objectAt(actorA)->keyframes.at(1).t, 3.0, 0.001), "撤销关键帧拖动");
    QTest::keyClick(view, Qt::Key_Y, Qt::ControlModifier);
    CHECK(near(objectAt(actorA)->keyframes.at(1).t, 4.0, 0.001), "重做关键帧拖动");

    // ---- 关键帧拖出时长自动延长及撤销还原 ----
    CHECK(near(runtime->fileData().timeline.duration, 60.0, 0.001), "阶段5：默认时长 60s");
    {
        const QPoint beyondFrom = timeline->canvasPosFor(actorA, 4.0);
        const QPoint beyondTo(int(TimelineWidget::kNameColW + 61.2 * timeline->pxPerSec()),
                              beyondFrom.y());
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, beyondFrom);
        QTest::mouseMove(canvas, beyondFrom + QPoint(6, 0));
        QTest::mouseMove(canvas, beyondTo);
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, beyondTo);
        CHECK(near(runtime->fileData().timeline.duration, 61.2, 0.001),
              "关键帧拖出时长自动延长（61.2s）");
    }
    reactivate();
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier);
    CHECK(near(runtime->fileData().timeline.duration, 60.0, 0.001)
              && near(objectAt(actorA)->keyframes.at(1).t, 4.0, 0.001),
          "撤销：时长与关键时刻一并还原");
    QTest::keyClick(view, Qt::Key_Y, Qt::ControlModifier);
    CHECK(near(runtime->fileData().timeline.duration, 61.2, 0.001), "重做：时长再延长");

    // ---- 播放 / 暂停 / 倍速 ----
    player->seekTo(0.0);
    playButton->click();
    CHECK(player->isPlaying() && playButton->text() == QStringLiteral("暂停"),
          "播放按钮驱动播放");
    QTest::qWait(250);
    CHECK(player->currentTime() > 0.05, "播放推进游标（tick × 倍速）");
    playButton->click();
    CHECK(!player->isPlaying() && playButton->text() == QStringLiteral("播放"), "暂停按钮");
    const double stoppedAt = player->currentTime();
    QTest::qWait(120);
    CHECK(qAbs(player->currentTime() - stoppedAt) < 1e-9, "暂停后游标不动");
    player->setSpeed(2.0);
    CHECK(speedCombo->currentIndex() == 3, "倍速设置联动下拉框（2×）");
    player->setSpeed(1.0);

    // ---- 场次标签：添加 / 双击改名 / 右键删除 / 撤销 ----
    player->seekTo(6.0);
    addLabelButton->click();
    CHECK(runtime->fileData().timeline.labels.size() == 1
              && near(runtime->fileData().timeline.labels.first().t, 6.0, 0.001)
              && !runtime->fileData().timeline.labels.first().name.isEmpty(),
          "加标签按钮：游标处添加场次标签（自动命名）");
    const QPoint labelPos(int(TimelineWidget::kNameColW + 6.0 * timeline->pxPerSec()),
                          TimelineWidget::kRulerH + TimelineWidget::kLabelRowH / 2);
    QTimer::singleShot(300, [] {
        if (QWidget* modal = QApplication::activeModalWidget()) {
            if (auto* edit = modal->findChild<QLineEdit*>())
                edit->setText(QStringLiteral("高潮幕"));
            if (auto* dialog = qobject_cast<QDialog*>(modal))
                dialog->accept();
        }
    });
    QTest::mouseDClick(canvas, Qt::LeftButton, Qt::NoModifier, labelPos);
    CHECK(runtime->fileData().timeline.labels.first().name == QStringLiteral("高潮幕"),
          "双击标签改名（命令入撤销栈）");
    // 右键标签菜单（重命名/删除）是 QMenu::exec 模态：offscreen 下无法自动关闭，
    // 与 StageView 右键菜单一样列入手测；删除路径这里走业务入口 + 快捷键撤销验证
    CHECK(core->removeSceneLabel(0), "删除场次标签（业务入口，UI 右键菜单手测）");
    CHECK(runtime->fileData().timeline.labels.isEmpty(), "删除场次标签生效");
    reactivate();
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier);
    CHECK(runtime->fileData().timeline.labels.size() == 1
              && runtime->fileData().timeline.labels.first().name == QStringLiteral("高潮幕"),
          "撤销恢复被删标签");

    // ==================== 时间轴交互完善：画布统一选择模型 ====================
    // Dock 初始高度：默认能露出 工具行+标尺+标签行+2 分组标题+5 轨道
    auto* timelineDock = window.findChild<QDockWidget*>(QStringLiteral("timelineDock"));
    CHECK(timelineDock && timelineDock->height() >= 240,
          "时间轴 Dock 初始高度 ≥ 240（默认容纳全部轨道）");

    const double pxT = timeline->pxPerSec(); // 默认 20

    // ---- 点标尺选中游标 + ←/→ 步进 ±0.5s（与游标拖拽吸附一致）----
    player->seekTo(10.0);
    QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier,
                      QPoint(int(TimelineWidget::kNameColW + 12.0 * pxT),
                             TimelineWidget::kRulerH / 2));
    CHECK(near(player->currentTime(), 12.0, 0.001), "点标尺定位游标（选中游标）");
    QTest::keyClick(canvas, Qt::Key_Right);
    CHECK(near(player->currentTime(), 12.5, 0.001), "游标选中后 → 步进 +0.5s");
    QTest::keyClick(canvas, Qt::Key_Left);
    QTest::keyClick(canvas, Qt::Key_Left);
    CHECK(near(player->currentTime(), 11.5, 0.001), "游标选中后 ← 步进 -0.5s");

    // ---- 标尺拖拽 0.5s 吸附：拖到非对齐 x，落点在 0.5 网格 ----
    {
        const int rulerY = TimelineWidget::kRulerH / 2;
        const QPoint from(int(TimelineWidget::kNameColW + 12.0 * pxT), rulerY);
        const QPoint to(int(TimelineWidget::kNameColW + 13.2 * pxT), rulerY);
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, from);
        QTest::mouseMove(canvas, to);
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, to);
        CHECK(near(player->currentTime(), 13.0, 0.001), "标尺拖拽吸附 0.5s（13.2 → 13.0）");
    }

    // ---- 快捷键仲裁：无关键帧选中时 Ctrl+C/V 仍是对象语义 ----
    reactivate();
    core->selection()->select(actorB);
    canvas->setFocus();
    const qsizetype objCountBefore = runtime->fileData().objects.size();
    QTest::keyClick(canvas, Qt::Key_C, Qt::ControlModifier);
    CHECK(core->clipboardHasContent(), "画布焦点无关键帧选中：Ctrl+C 复制对象（对象语义）");
    QTest::keyClick(canvas, Qt::Key_V, Qt::ControlModifier);
    CHECK(runtime->fileData().objects.size() == objCountBefore + 1,
          "关键帧剪贴板为空：Ctrl+V 粘贴对象（对象语义）");
    CHECK(core->undo() && runtime->fileData().objects.size() == objCountBefore,
          "撤销对象粘贴恢复对象数");

    // ---- 关键帧选中 ←/→ ±0.5s 批量步进（选中保持可连按）+ 撤销重做 ----
    // actorA 当前帧 @1 (0,100,0) 与 @61.2
    player->seekTo(20.0); // 游标移开，避免游标线命中抢占轨道点击
    QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier,
                      timeline->canvasPosFor(actorA, 1.0));
    QTest::keyClick(canvas, Qt::Key_Right);
    CHECK(near(objectAt(actorA)->keyframes.first().t, 1.5, 0.001),
          "关键帧选中后 → 步进 +0.5s（批量命令）");
    QTest::keyClick(canvas, Qt::Key_Right);
    CHECK(near(objectAt(actorA)->keyframes.first().t, 2.0, 0.001),
          "选中跟随新时刻：连按 → 再步进 +0.5s");
    reactivate();
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier);
    CHECK(near(objectAt(actorA)->keyframes.first().t, 1.5, 0.001), "撤销一次批量步进");
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier);
    CHECK(near(objectAt(actorA)->keyframes.first().t, 1.0, 0.001), "再撤销回 1.0");
    QTest::keyClick(view, Qt::Key_Y, Qt::ControlModifier);
    CHECK(near(objectAt(actorA)->keyframes.first().t, 1.5, 0.001), "重做批量步进");

    // ---- 关键帧级 Ctrl+C/V（画布焦点，关键帧语义）：复制 @1.5 粘贴到游标 ----
    QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier,
                      timeline->canvasPosFor(actorA, 1.5));
    QTest::keyClick(canvas, Qt::Key_C, Qt::ControlModifier);
    CHECK(core->keyframeClipboardHasContent(), "Ctrl+C 复制选中关键帧（关键帧剪贴板）");
    player->seekTo(25.0);
    QTest::keyClick(canvas, Qt::Key_V, Qt::ControlModifier);
    {
        bool pastedAtCursor = false;
        for (const Keyframe& key : objectAt(actorA)->keyframes)
            pastedAtCursor = pastedAtCursor
                || (near(key.t, 25.0, 0.001) && near(key.x, 0.0, 0.01)
                    && near(key.y, 100.0, 0.01));
        CHECK(pastedAtCursor, "Ctrl+V 在游标处粘贴关键帧（值保持，时刻对齐游标）");
        CHECK(runtime->fileData().objects.size() == objCountBefore, "关键帧粘贴不新增对象");
    }
    reactivate();
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier);
    CHECK(objectAt(actorA)->keyframes.size() == 2, "撤销关键帧粘贴");

    // ---- 场次标签：单击选中 / ←/→ 步进 ±0.5s / 长按拖动改时刻 / Delete 删除 ----
    const QPoint labelPosT(int(TimelineWidget::kNameColW + 6.0 * pxT),
                           TimelineWidget::kRulerH + TimelineWidget::kLabelRowH / 2);
    QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, labelPosT);
    CHECK(runtime->fileData().timeline.labels.size() == 1
              && near(runtime->fileData().timeline.labels.first().t, 6.0, 0.001),
          "单击选中标签（时刻不变）");
    QTest::keyClick(canvas, Qt::Key_Right);
    CHECK(near(runtime->fileData().timeline.labels.first().t, 6.5, 0.001),
          "标签选中后 → 步进 +0.5s");
    QTest::keyClick(canvas, Qt::Key_Left);
    CHECK(near(runtime->fileData().timeline.labels.first().t, 6.0, 0.001),
          "标签选中后 ← 步进 -0.5s");
    {
        const QPoint dragTo(int(TimelineWidget::kNameColW + 8.4 * pxT), labelPosT.y());
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, labelPosT);
        QTest::mouseMove(canvas, labelPosT + QPoint(6, 0)); // 越过拖拽阈值
        QTest::mouseMove(canvas, dragTo);
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, dragTo);
        CHECK(near(runtime->fileData().timeline.labels.first().t, 8.4, 0.001)
                  && runtime->fileData().timeline.labels.first().name
                         == QStringLiteral("高潮幕"),
              "长按拖动标签改时刻（吸附 0.1s，名不变）");
    }
    reactivate();
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier);
    CHECK(near(runtime->fileData().timeline.labels.first().t, 6.0, 0.001), "撤销标签拖动");
    QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, labelPosT); // 重新选中标签
    QTest::keyClick(canvas, Qt::Key_Delete);
    CHECK(runtime->fileData().timeline.labels.isEmpty(), "Delete 删除选中标签（仲裁归画布）");
    reactivate();
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier);
    CHECK(runtime->fileData().timeline.labels.size() == 1, "撤销标签删除");

    // ---- 点轨道联动对象选中（时间区空白 / 冻结名列）----
    player->seekTo(40.0); // 游标移开（游标线命中优先于轨道选中）
    QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier,
                      timeline->canvasPosFor(actorB, 30.0));
    CHECK(ids() == QList<QString> { actorB }, "点轨道行时间区空白 → 选中轨道并联动对象选中");
    core->selection()->clear();
    QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier,
                      QPoint(20, timeline->canvasPosFor(actorB, 0.0).y()));
    CHECK(ids() == QList<QString> { actorB }, "点冻结名列轨道名 → 选中轨道并联动");

    // ---- Esc 清空画布选择（轨道选中联动清空对象选中；之后方向键不再步进游标）----
    QTest::keyClick(canvas, Qt::Key_Escape);
    CHECK(ids().isEmpty(), "Esc 清空画布选择（轨道选中联动清空）");
    QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier,
                      QPoint(int(TimelineWidget::kNameColW + 50.0 * pxT),
                             TimelineWidget::kRulerH / 2));
    QTest::keyClick(canvas, Qt::Key_Escape);
    QTest::keyClick(canvas, Qt::Key_Right);
    CHECK(near(player->currentTime(), 50.0, 0.001), "Esc 后 ←/→ 不再步进游标（交给上层）");

    // ---- 轨道折叠/展开：点名列分组标题切换；折叠状态随重建保持 ----
    {
        const int headerY = TimelineWidget::kRulerH + TimelineWidget::kLabelRowH + 10;
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, headerY));
        CHECK(timeline->canvasPosFor(actorA, 1.0).x() < 0, "折叠演员组：轨道行隐藏");
        core->selection()->select(actorB);
        keyButton->click(); // 触发一次命令 → fileDataChanged → 轨道重建
        CHECK(timeline->canvasPosFor(actorA, 1.0).x() < 0, "折叠状态随重建保持");
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, headerY));
        CHECK(timeline->canvasPosFor(actorA, 1.0).x() > 0, "再点分组标题展开");
    }

    // ==================== 阶段6：文件流程 / 未保存 * ====================
    // 模态弹窗用 QTimer 定时操作；QFileDialog 原生对话框 offscreen 不可驱动，列手测。
    // 路径明确的核心方法（saveToPath/openFromPath/askUnsaved/maybeCloseOrContinue）直调自测。
    QTemporaryDir tempDir;
    CHECK(tempDir.isValid(), "阶段6：临时目录可用");
    const auto writeFile = [](const QString& path, const QByteArray& content) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(content) == content.size();
    };

    // ---- 标题栏 * 与 saveToPath / openFromPath（另起窗口，不动主窗口状态） ----
    {
        MainWindow w2;
        w2.show();
        QTest::qWaitForWindowExposed(&w2);
        // 阶段8：构造即空舞台，标题栏断言依赖示例存档 → 显式加载（探针确认可读，避免弹窗挂住）
        const QString demoPath2 =
            QCoreApplication::applicationDirPath() + QStringLiteral("/stage-demo.json");
        CHECK(JsonStore::load(demoPath2).ok(), "阶段6：示例存档可读");
        CHECK(w2.openFromPath(demoPath2), "阶段6：w2 显式加载示例存档");
        auto* core2 = w2.findChild<EditorCore*>();
        auto* runtime2 = w2.findChild<RuntimeData*>();
        CHECK(core2 && runtime2, "阶段6：新窗口组件装配完整");

        CHECK(!runtime2->isDirty() && !w2.isWindowModified(),
              "标题栏：初始（加载示例存档）不脏、无 *");
        CHECK(runtime2->currentFilePath().endsWith(QStringLiteral("stage-demo.json")),
              "标题栏：初始路径为示例存档");
        CHECK(w2.windowTitle().contains(QStringLiteral("stage-demo.json[*]")),
              "标题栏：文件名 + [*] 占位符格式");

        // 编辑 → 标题出现 *
        CHECK(core2->createObject(ObjectType::Actor, QPointF(0.0, 0.0)), "阶段6：创建演员");
        CHECK(runtime2->isDirty() && w2.isWindowModified(), "标题栏：编辑后出现 *");

        // saveToPath → * 消失、路径切换、文件可读回一致
        const QString savePath = tempDir.filePath(QStringLiteral("stage6-save.json"));
        CHECK(w2.saveToPath(savePath), "saveToPath 成功");
        CHECK(!runtime2->isDirty() && !w2.isWindowModified(), "标题栏：保存后 * 消失");
        CHECK(runtime2->currentFilePath() == savePath
                  && w2.windowTitle().contains(QStringLiteral("stage6-save.json")),
              "标题栏：文件名随路径更新");
        const LoadResult reload = JsonStore::load(savePath);
        CHECK(reload.ok() && reload.data == runtime2->fileData(),
              "saveToPath 文件可读回且与内存逐字段一致");

        // 撤销跨过保存点：先再编辑（脏），撤销回已存内容 → * 消失；重做 → * 回来
        CHECK(core2->createObject(ObjectType::Screen, QPointF(50.0, 50.0)), "阶段6：保存后再编辑");
        CHECK(runtime2->isDirty() && w2.isWindowModified(), "标题栏：保存后再编辑 * 回来");
        CHECK(core2->undo(), "阶段6：撤销二次编辑");
        CHECK(!runtime2->isDirty() && !w2.isWindowModified(),
              "标题栏：撤销回已存内容 = 干净（* 消失）");
        CHECK(core2->redo() && runtime2->isDirty() && w2.isWindowModified(),
              "标题栏：重做后 * 回来");

        // openFromPath 损坏文件：弹提示（定时关闭）且数据/路径/dirty 不变
        const QString corruptPath = tempDir.filePath(QStringLiteral("stage6-corrupt.json"));
        CHECK(writeFile(corruptPath, "{ not json !!!"), "阶段6：写入损坏测试文件");
        const FileData beforeOpen = runtime2->fileData();
        QTimer::singleShot(300, [] {
            if (QWidget* modal = QApplication::activeModalWidget())
                modal->close();
        });
        CHECK(!w2.openFromPath(corruptPath), "openFromPath 损坏文件返回 false");
        CHECK(runtime2->fileData() == beforeOpen
                  && runtime2->currentFilePath() == savePath && runtime2->isDirty(),
              "openFromPath 失败：数据/路径/dirty 均不变");

        // openFromPath 成功：替换数据、* 消失、撤销栈清空
        const QString otherPath = tempDir.filePath(QStringLiteral("stage6-other.json"));
        FileData other = FileData::createDefault();
        other.stage.width = 2000.0;
        CHECK(JsonStore::save(otherPath, other).ok, "阶段6：写入另一存档");
        CHECK(w2.openFromPath(otherPath), "openFromPath 成功");
        CHECK(runtime2->fileData() == other && runtime2->currentFilePath() == otherPath,
              "openFromPath 成功替换数据并更新路径");
        CHECK(!runtime2->isDirty() && !w2.isWindowModified(), "标题栏：打开成功后 * 消失");
        CHECK(!core2->canUndo() && !core2->canRedo(), "打开文件清空撤销栈");

        // ---- askUnsaved / maybeCloseOrContinue 门卫 ----
        CHECK(w2.askUnsaved() == MainWindow::UnsavedChoice::Discard,
              "askUnsaved：不脏直接返回 Discard（无弹窗）");
        CHECK(w2.maybeCloseOrContinue(), "门卫：不脏直接放行");

        CHECK(core2->createObject(ObjectType::Actor, QPointF(10.0, 10.0)), "阶段6：再创建演员（脏）");
        QTimer::singleShot(300, [] {
            if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                box->button(QMessageBox::Cancel)->click();
        });
        CHECK(w2.askUnsaved() == MainWindow::UnsavedChoice::Cancel, "askUnsaved：选取消");
        QTimer::singleShot(300, [] {
            if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                box->button(QMessageBox::Cancel)->click();
        });
        CHECK(!w2.maybeCloseOrContinue(), "门卫：选取消则中止");
        CHECK(runtime2->isDirty(), "取消后仍脏");

        QTimer::singleShot(300, [] {
            if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                box->button(QMessageBox::Discard)->click();
        });
        CHECK(w2.maybeCloseOrContinue(), "门卫：选不保存放行");
        CHECK(runtime2->isDirty(), "不保存放行不改变 dirty（后续动作由调用方决定）");

        // 选保存（有路径 otherPath）→ 直接保存不弹文件对话框，dirty 清除
        QTimer::singleShot(300, [] {
            if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                box->button(QMessageBox::Save)->click();
        });
        CHECK(w2.maybeCloseOrContinue(), "门卫：选保存走保存流程并放行");
        CHECK(!runtime2->isDirty(), "选保存后 dirty 清除");
        const LoadResult savedBack = JsonStore::load(otherPath);
        CHECK(savedBack.ok() && savedBack.data == runtime2->fileData(),
              "选保存后文件内容与内存一致");

        // ---- 新建菜单过同一门卫 ----
        QAction* newAction = nullptr;
        for (QAction* action : w2.findChildren<QAction*>()) {
            if (action->text() == QStringLiteral("新建(&N)"))
                newAction = action;
        }
        CHECK(newAction && newAction->shortcut() == QKeySequence::New,
              "新建菜单项已做实（Ctrl+N）");
        core2->createObject(ObjectType::Actor, QPointF(1.0, 1.0)); // 再弄脏
        QTimer::singleShot(300, [] {
            if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                box->button(QMessageBox::Cancel)->click();
        });
        newAction->trigger();
        CHECK(!runtime2->fileData().objects.isEmpty(), "新建：选取消则中止（数据保留）");
        QTimer::singleShot(300, [] {
            if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                box->button(QMessageBox::Discard)->click();
        });
        newAction->trigger();
        CHECK(runtime2->fileData() == FileData::createDefault()
                  && runtime2->currentFilePath().isEmpty(),
              "新建：选不保存后重置为默认空舞台");
        CHECK(w2.windowTitle().contains(QStringLiteral("未命名")), "无路径新文件标题显示未命名");
    }

    // ---- closeEvent：未保存三选一（不保存关闭 / 取消保留 / 不脏直接关） ----
    {
        MainWindow w3;
        w3.show();
        QTest::qWaitForWindowExposed(&w3);
        CHECK(w3.findChild<EditorCore*>()->createObject(ObjectType::Actor, QPointF(0.0, 0.0)),
              "closeEvent：制造未保存修改");
        QTimer::singleShot(300, [] {
            if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                box->button(QMessageBox::Discard)->click();
        });
        w3.close();
        CHECK(!w3.isVisible(), "closeEvent：选不保存窗口关闭");

        MainWindow w4;
        w4.show();
        QTest::qWaitForWindowExposed(&w4);
        CHECK(w4.findChild<EditorCore*>()->createObject(ObjectType::Actor, QPointF(0.0, 0.0)),
              "closeEvent：制造未保存修改");
        QTimer::singleShot(300, [] {
            if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                box->button(QMessageBox::Cancel)->click();
        });
        w4.close();
        CHECK(w4.isVisible(), "closeEvent：选取消窗口保留");
        CHECK(w4.findChild<RuntimeData*>()->isDirty(), "closeEvent：取消后仍脏");

        MainWindow w5;
        w5.show();
        QTest::qWaitForWindowExposed(&w5);
        bool modalAppeared = false;
        QTimer::singleShot(300, [&modalAppeared] {
            if (QApplication::activeModalWidget())
                modalAppeared = true;
        });
        w5.close();
        QTest::qWait(400); // 给潜在弹窗与定时器留出触发窗口
        CHECK(!modalAppeared && !w5.isVisible(), "closeEvent：不脏直接关闭、无弹窗");
    }

    // ==================== 阶段7：统一设置窗口（VSCode 风格，非模态单例） ====================
    // ---- 菜单入口：菜单栏顶层"设置"项单击直达；旧入口并入；重复打开聚焦复用 ----
    QAction* settingsAction = nullptr;
    bool oldEntriesGone = true;
    for (QAction* action : window.findChildren<QAction*>()) {
        const QString text = action->text();
        if (text == QStringLiteral("设置(&T)"))
            settingsAction = action;
        else if (text == QStringLiteral("舞台设置(&S)...")
                 || text == QStringLiteral("偏好设置(&P)...")
                 || text == QStringLiteral("舞台颜色(&C)..."))
            oldEntriesGone = false;
    }
    CHECK(settingsAction, "菜单栏顶层“设置”项已做实（单击直达）");
    CHECK(oldEntriesGone, "舞台设置/偏好设置/舞台颜色旧入口并入统一设置窗口（不再单列）");

    settingsAction->trigger();
    SettingsWindow* settingsWin = window.findChild<SettingsWindow*>();
    CHECK(settingsWin && settingsWin->isVisible() && !settingsWin->isModal(),
          "“设置”单击直达打开统一设置窗口（非模态）");
    auto* navTree = settingsWin->findChild<QListWidget*>(QStringLiteral("settingsNavTree"));
    auto* searchEdit = settingsWin->findChild<QLineEdit*>(QStringLiteral("settingsSearchEdit"));
    auto* pageStack = settingsWin->findChild<QStackedWidget*>(QStringLiteral("settingsPageStack"));
    CHECK(navTree && searchEdit && pageStack, "设置窗口组件（搜索框/分类导航/分页）装配完整");
    CHECK(navTree->count() == 3 && navTree->item(0)->text() == QStringLiteral("舞台")
              && navTree->item(1)->text() == QStringLiteral("画布")
              && navTree->item(2)->text() == QStringLiteral("时间轴"),
          "分类导航三项：舞台/画布/时间轴");
    CHECK(navTree->currentRow() == 0
              && pageStack->currentWidget()->objectName() == QStringLiteral("settingsStagePage"),
          "设置入口定位“舞台”分类");

    // 关闭只隐藏不销毁：再打开仍是同一实例
    settingsWin->close();
    CHECK(!settingsWin->isVisible(), "设置窗口关闭只隐藏");
    settingsAction->trigger();
    CHECK(window.findChildren<SettingsWindow*>().size() == 1
              && window.findChild<SettingsWindow*>() == settingsWin && settingsWin->isVisible(),
          "重复打开聚焦复用同一窗口（不新建）");
    CHECK(navTree->currentRow() == 0, "重复打开定位回“舞台”分类");

    // ---- 分类切换：右侧显示对应分类设置行 ----
    auto* widthSpin = settingsWin->findChild<QDoubleSpinBox*>(QStringLiteral("stageWidthSpin"));
    auto* promptCheck = settingsWin->findChild<QCheckBox*>(QStringLiteral("keyframePromptCheck"));
    CHECK(widthSpin && promptCheck, "舞台/时间轴设置行控件存在");
    navTree->setCurrentRow(2);
    CHECK(pageStack->currentWidget()->objectName() == QStringLiteral("settingsTimelinePage")
              && promptCheck->isVisible(),
          "点分类切换到“时间轴”页（打帧询问行可见）");
    navTree->setCurrentRow(0);
    CHECK(pageStack->currentWidget()->objectName() == QStringLiteral("settingsStagePage")
              && widthSpin->isVisible() && !promptCheck->isVisible(),
          "切回“舞台”页（宽度行可见、时间轴行隐藏）");
    settingsWin->grab().save(
        QDir::temp().absoluteFilePath(QStringLiteral("stage7-settings-stage.png")));

    // ---- 搜索过滤：命中/搜索中忽略分类分页/无匹配占位/清空恢复 ----
    auto* snapCheck = settingsWin->findChild<QCheckBox*>(QStringLiteral("gridSnapCheck"));
    auto* visibleCheck = settingsWin->findChild<QCheckBox*>(QStringLiteral("gridVisibleCheck"));
    auto* gridSizeSpin = settingsWin->findChild<QDoubleSpinBox*>(QStringLiteral("gridSizeSpin"));
    auto* snapRangeSpin = settingsWin->findChild<QDoubleSpinBox*>(QStringLiteral("snapRangeSpin"));
    auto* noMatchLabel = settingsWin->findChild<QLabel*>(QStringLiteral("settingsNoMatchLabel"));
    CHECK(snapCheck && visibleCheck && gridSizeSpin && snapRangeSpin && noMatchLabel,
          "画布设置行与无匹配占位控件存在");
    searchEdit->setText(QStringLiteral("网格"));
    CHECK(pageStack->currentWidget()->objectName() == QStringLiteral("settingsSearchPage"),
          "搜索时切到跨分类结果页");
    CHECK(snapCheck->isVisible() && visibleCheck->isVisible() && gridSizeSpin->isVisible()
              && snapRangeSpin->isVisible(),
          "搜索“网格”命中画布全部四行（跨分类；含标题/描述匹配）");
    CHECK(!widthSpin->isVisible() && !promptCheck->isVisible(), "不匹配的行隐藏");
    CHECK(!noMatchLabel->isVisible(), "有匹配时占位提示隐藏");
    settingsWin->grab().save(
        QDir::temp().absoluteFilePath(QStringLiteral("stage7-settings-search.png")));
    navTree->setCurrentRow(2); // 搜索中点分类：只记录选择，不切页
    CHECK(pageStack->currentWidget()->objectName() == QStringLiteral("settingsSearchPage"),
          "搜索中忽略分类分页（停留结果页）");
    searchEdit->setText(QStringLiteral("不存在的设置项xyz"));
    CHECK(noMatchLabel->isVisible(), "无匹配时显示占位提示");
    CHECK(!snapCheck->isVisible() && !widthSpin->isVisible() && !promptCheck->isVisible(),
          "无匹配时全部设置行隐藏");
    searchEdit->clear();
    CHECK(pageStack->currentWidget()->objectName() == QStringLiteral("settingsTimelinePage"),
          "清空搜索回到导航当前分类页（时间轴）");
    CHECK(promptCheck->isVisible(), "清空后分类页设置行恢复显示");
    navTree->setCurrentRow(0);
    settingsWin->close(); // 主窗口的设置窗口用毕

    // ---- 设置项行为（另起窗口，干净状态验证 dirty/撤销栈/回显同步） ----
    {
        MainWindow w6;
        w6.show();
        QTest::qWaitForWindowExposed(&w6);
        // 阶段8：构造即空舞台，舞台回显断言依赖示例存档（1500×900/偏移 100）→ 显式加载
        const QString demoPath6 =
            QCoreApplication::applicationDirPath() + QStringLiteral("/stage-demo.json");
        CHECK(JsonStore::load(demoPath6).ok(), "阶段7：示例存档可读");
        CHECK(w6.openFromPath(demoPath6), "阶段7：w6 显式加载示例存档");
        auto* core6 = w6.findChild<EditorCore*>();
        auto* runtime6 = w6.findChild<RuntimeData*>();
        auto* view6 = qobject_cast<StageView*>(w6.centralWidget());
        auto* scene6 = qobject_cast<StageScene*>(view6->scene());
        CHECK(core6 && runtime6 && view6 && scene6, "阶段7：新窗口组件装配完整");

        // 设置项已知起点（QSettings 跨运行持久化，先复位再开窗口保证控件回显确定）
        AppSettings& settings6 = runtime6->settings();
        settings6.setGridSnapEnabled(true);
        settings6.setGridVisible(true);
        settings6.setKeyframePromptSuppressed(true); // 模拟曾勾选“不再提示”
        settings6.setGridSizeCm(AppSettings::defaultGridSizeCm);
        settings6.setSnapRangeCm(AppSettings::defaultSnapRangeCm);

        QAction* settingsAction6 = nullptr;
        for (QAction* action : w6.findChildren<QAction*>()) {
            if (action->text() == QStringLiteral("设置(&T)"))
                settingsAction6 = action;
        }
        CHECK(settingsAction6, "阶段7：新窗口“设置”菜单项存在");
        settingsAction6->trigger();
        SettingsWindow* sw6 = w6.findChild<SettingsWindow*>();
        auto* widthSpin6 = sw6->findChild<QDoubleSpinBox*>(QStringLiteral("stageWidthSpin"));
        auto* depthSpin6 = sw6->findChild<QDoubleSpinBox*>(QStringLiteral("stageDepthSpin"));
        auto* offsetSpin6 = sw6->findChild<QDoubleSpinBox*>(QStringLiteral("stageOffsetSpin"));
        auto* colorButton6 = sw6->findChild<QPushButton*>(QStringLiteral("stageColorButton"));
        auto* snapCheck6 = sw6->findChild<QCheckBox*>(QStringLiteral("gridSnapCheck"));
        auto* visibleCheck6 = sw6->findChild<QCheckBox*>(QStringLiteral("gridVisibleCheck"));
        auto* promptCheck6 = sw6->findChild<QCheckBox*>(QStringLiteral("keyframePromptCheck"));
        auto* gridSizeSpin6 = sw6->findChild<QDoubleSpinBox*>(QStringLiteral("gridSizeSpin"));
        auto* snapRangeSpin6 = sw6->findChild<QDoubleSpinBox*>(QStringLiteral("snapRangeSpin"));
        CHECK(sw6 && widthSpin6 && depthSpin6 && offsetSpin6 && colorButton6 && snapCheck6
                  && visibleCheck6 && promptCheck6 && gridSizeSpin6 && snapRangeSpin6,
              "阶段7：设置窗口全部控件装配完整");

        // 回显当前舞台（示例存档 1500×900，偏移 100）
        CHECK(near(widthSpin6->value(), 1500.0, 0.01) && near(depthSpin6->value(), 900.0, 0.01)
                  && near(offsetSpin6->value(), 100.0, 0.01),
              "设置窗口回显当前舞台尺寸");

        // ---- 舞台颜色：选中立即生效并持久化，不弄脏文件、不产生撤销条目 ----
        const QColor colorBefore = settings6.stageColor();
        CHECK(colorButton6->text() == colorBefore.name(), "色块按钮显示当前颜色");
        const QColor newColor(QStringLiteral("#12ab34"));
        QTimer::singleShot(300, [newColor] {
            if (auto* colorDialog =
                    qobject_cast<QColorDialog*>(QApplication::activeModalWidget())) {
                colorDialog->setCurrentColor(newColor);
                colorDialog->accept();
            }
        });
        colorButton6->click(); // 模态 QColorDialog 由上面的定时器驱动
        CHECK(settings6.stageColor() == newColor, "舞台颜色选中立即生效");
        {
            AppSettings fresh; // 同进程新实例读回 = QSettings 持久化
            CHECK(fresh.stageColor() == newColor, "舞台颜色跨实例持久化");
        }
        CHECK(!runtime6->isDirty() && !core6->canUndo(),
              "舞台颜色是软件设置：不弄脏文件、不产生撤销条目");
        CHECK(colorButton6->text() == newColor.name(), "色块按钮随选择刷新");

        // ---- 舞台尺寸：setValue 不即时提交，editingFinished 才直改直提交 ----
        widthSpin6->setValue(2000.0);
        depthSpin6->setValue(1500.0);
        offsetSpin6->setValue(50.0);
        CHECK(runtime6->fileData().stage == StageData({ 1500.0, 900.0, 100.0 }),
              "setValue 不即时提交（等 editingFinished）");
        // 回车/失焦均汇于 editingFinished，自测直接触发该信号
        widthSpin6->editingFinished();
        CHECK(runtime6->fileData().stage == StageData({ 2000.0, 1500.0, 50.0 }),
              "editingFinished 提交舞台尺寸（直改直提交）");
        CHECK(near(scene6->stageRect().width(), 2000.0, 0.001)
                  && near(scene6->stageRect().height(), 1500.0, 0.001)
                  && near(scene6->stageRect().left(), 50.0 - 1000.0, 0.001),
              "StageScene 舞台矩形随 fileDataChanged 立即重建");
        CHECK(runtime6->isDirty() && w6.isWindowModified(),
              "舞台修改弄脏文件（标题栏 * 出现）");
        CHECK(!core6->canUndo(), "舞台修改不产生撤销条目（canUndo 不变）");

        // 无变更不重复提交：再触发 editingFinished 不发 fileDataChanged
        int fileSignals = 0;
        QObject::connect(runtime6, &RuntimeData::fileDataChanged, runtime6,
                         [&fileSignals] { ++fileSignals; });
        widthSpin6->editingFinished();
        CHECK(fileSignals == 0, "值无变更时 editingFinished 不提交");

        // 非法值被控件范围钳制（业务层宽/深 ≤ 0 拒绝由 DataLayerSelfTest 覆盖）
        widthSpin6->setValue(0.0);
        CHECK(near(widthSpin6->value(), 1.0, 0.001), "宽下限 1.0：非法值被控件钳制");
        widthSpin6->setValue(2000.0); // 恢复与数据一致

        // Ctrl+Z 只撤销对象编辑，舞台值保持
        CHECK(core6->createObject(ObjectType::Actor, QPointF(0.0, 0.0)), "创建演员入撤销栈");
        const qsizetype objCount6 = runtime6->fileData().objects.size();
        depthSpin6->setValue(1600.0);
        depthSpin6->editingFinished();
        CHECK(near(runtime6->fileData().stage.depth, 1600.0, 0.001) && core6->canUndo(),
              "入栈后再改舞台");
        w6.activateWindow();
        QTest::qWaitForWindowActive(&w6);
        view6->setFocus();
        QTest::keyClick(view6, Qt::Key_Z, Qt::ControlModifier);
        CHECK(runtime6->fileData().objects.size() == objCount6 - 1, "Ctrl+Z 撤销对象创建");
        CHECK(runtime6->fileData().stage == StageData({ 2000.0, 1600.0, 50.0 }),
              "Ctrl+Z 不影响舞台值");
        CHECK(!core6->canUndo(), "撤销后栈空（无舞台条目残留）");

        // 新建/打开文件后窗口控件回显同步（窗口保持打开）
        const QString otherPath = tempDir.filePath(QStringLiteral("stage7-other.json"));
        FileData other = FileData::createDefault();
        other.stage = { 2400.0, 1800.0, -250.0 };
        CHECK(JsonStore::save(otherPath, other).ok, "阶段7：写入另一存档");
        CHECK(w6.openFromPath(otherPath), "打开另一存档");
        CHECK(near(widthSpin6->value(), 2400.0, 0.01) && near(depthSpin6->value(), 1800.0, 0.01)
                  && near(offsetSpin6->value(), -250.0, 0.01),
              "打开文件后设置窗口回显同步");
        CHECK(!runtime6->isDirty(), "打开成功后 * 消失");
        runtime6->newFile();
        CHECK(near(widthSpin6->value(), 1200.0, 0.01) && near(depthSpin6->value(), 800.0, 0.01)
                  && near(offsetSpin6->value(), 0.0, 0.01),
              "新建后回显默认舞台（1200×800，偏移 0）");

        // ---- 画布/时间轴行：checkbox toggled 即写设置 + 显示网格发重绘信号 ----
        int gridSignals = 0;
        QObject::connect(sw6, &SettingsWindow::gridVisibilityChanged, sw6,
                         [&gridSignals] { ++gridSignals; });
        snapCheck6->setChecked(false);
        visibleCheck6->setChecked(false);
        promptCheck6->setChecked(true); // 勾选“打帧前询问我” = 复位“不再提示”
        CHECK(!settings6.gridSnapEnabled() && !settings6.gridVisible(),
              "网格吸附/显示网格 toggled 立即生效");
        CHECK(!settings6.keyframePromptSuppressed(),
              "勾选“打帧前询问我”复位“不再提示”");
        CHECK(gridSignals == 1, "显示网格切换发出场景重绘信号");
        {
            AppSettings fresh;
            CHECK(!fresh.gridSnapEnabled() && !fresh.gridVisible()
                      && !fresh.keyframePromptSuppressed(),
                  "画布/时间轴改动跨实例持久化");
        }

        // ---- 网格间距/吸附距离：数值即改即生效并持久化；间距变化发重绘信号 ----
        CHECK(near(gridSizeSpin6->value(), 100.0, 0.01)
                  && near(snapRangeSpin6->value(), 20.0, 0.01),
              "网格间距/吸附距离回显当前设置（默认 100cm / 20cm）");
        int appearanceSignals = 0;
        QObject::connect(sw6, &SettingsWindow::gridAppearanceChanged, sw6,
                         [&appearanceSignals] { ++appearanceSignals; });
        gridSizeSpin6->setValue(55.5);
        CHECK(settings6.gridSizeCm() == 55.5, "网格间距改动立即生效");
        CHECK(appearanceSignals == 1, "网格间距改动发出场景重绘信号");
        snapRangeSpin6->setValue(12.5);
        CHECK(settings6.snapRangeCm() == 12.5, "吸附距离改动立即生效");
        CHECK(appearanceSignals == 1, "吸附距离改动不重绘（仅拖拽时读取）");
        {
            AppSettings fresh;
            CHECK(fresh.gridSizeCm() == 55.5 && fresh.snapRangeCm() == 12.5,
                  "网格间距/吸附距离跨实例持久化");
        }
        CHECK(!runtime6->isDirty(), "网格设置是软件自身数据：不弄脏文件");

        // 还原设置，避免污染本进程后续断言与下次运行
        snapCheck6->setChecked(true);
        visibleCheck6->setChecked(true);
        promptCheck6->setChecked(false);
        settings6.setStageColor(colorBefore);
        settings6.setGridSizeCm(AppSettings::defaultGridSizeCm);
        settings6.setSnapRangeCm(AppSettings::defaultSnapRangeCm);
    }

    // “显示网格”收进设置窗口“画布”分类，设置菜单不再保留该项（避免两处 check 状态同步）
    bool gridMenuItemGone = true;
    for (QAction* action : window.findChildren<QAction*>()) {
        if (action->text() == QStringLiteral("显示网格(&G)"))
            gridMenuItemGone = false;
    }
    CHECK(gridMenuItemGone, "“显示网格”收进设置窗口，设置菜单不再重复");

    // ==================== 阶段8：启动页 StartDialog（非模态实例化直查控件，不经 exec） ====================
    // MRU 哨兵经 AppSettings 直写（与启动页同一 QSettings 存储）；
    // 用例结尾清空哨兵，避免残留影响下次运行
    {
        AppSettings settings;
        // 已知起点：清空最近列表（本区域此前的 openFromPath 已写入若干条目）
        for (const QString& p : settings.recentFiles())
            settings.removeRecentFile(p);
        CHECK(settings.recentFiles().isEmpty(), "阶段8：用例前清空最近列表");

        const QString pathA = tempDir.filePath(QStringLiteral("第一幕存档.json"));
        const QString pathB = tempDir.filePath(QStringLiteral("第二幕存档.json"));
        settings.addRecentFile(pathA);
        settings.addRecentFile(pathB); // MRU：pathB 在前

        StartDialog dialog;
        auto* searchEdit8 = dialog.findChild<QLineEdit*>(QStringLiteral("recentSearchEdit"));
        auto* recentList = dialog.findChild<QListWidget*>(QStringLiteral("recentFilesList"));
        auto* emptyLabel8 = dialog.findChild<QLabel*>(QStringLiteral("recentEmptyLabel"));
        auto* recentStack = dialog.findChild<QStackedWidget*>(QStringLiteral("recentStack"));
        auto* openOtherBtn = dialog.findChild<QPushButton*>(QStringLiteral("openOtherButton"));
        auto* newBtn = dialog.findChild<QPushButton*>(QStringLiteral("newProjectButton"));
        auto* emptyBtn = dialog.findChild<QPushButton*>(QStringLiteral("emptyProjectButton"));
        CHECK(searchEdit8 && recentList && emptyLabel8 && recentStack && openOtherBtn
                  && newBtn && emptyBtn,
              "阶段8：启动页组件（搜索框/列表/占位/三入口按钮）装配完整");
        CHECK(searchEdit8->placeholderText().contains(QStringLiteral("搜索")),
              "阶段8：搜索框占位文本");
        CHECK(recentList->count() == 2, "阶段8：最近列表行数与 MRU 一致");
        CHECK(recentList->item(0)->data(Qt::UserRole).toString() == pathB
                  && recentList->item(1)->data(Qt::UserRole).toString() == pathA,
              "阶段8：列表顺序 = MRU 新→旧");
        CHECK(recentList->item(0)->text().contains(QStringLiteral("第二幕存档.json"))
                  && recentList->item(0)->text().contains(pathB),
              "阶段8：列表项文本 = 文件名 + 完整路径两行");
        CHECK(recentStack->currentWidget() == recentList, "阶段8：有条目时显示列表而非占位");

        // 截图供人工复核（VS 风格左右布局；含条目显示）
        dialog.show();
        QTest::qWaitForWindowExposed(&dialog);
        dialog.grab().save(
            QDir::temp().absoluteFilePath(QStringLiteral("stage8-startdialog.png")));
        dialog.hide();

        // 搜索过滤：命中 / 无匹配占位 / 清空恢复
        searchEdit8->setText(QStringLiteral("第一幕"));
        CHECK(recentList->count() == 1
                  && recentList->item(0)->data(Qt::UserRole).toString() == pathA,
              "阶段8：搜索过滤命中（文件名包含，大小写不敏感）");
        searchEdit8->setText(QStringLiteral("不存在的存档xyz"));
        CHECK(recentList->count() == 0 && recentStack->currentWidget() == emptyLabel8
                  && emptyLabel8->text().contains(QStringLiteral("无匹配")),
              "阶段8：搜索无匹配显示占位提示");
        searchEdit8->clear();
        CHECK(recentList->count() == 2 && recentStack->currentWidget() == recentList,
              "阶段8：清空搜索恢复完整列表");

        // 单击条目即选择该存档（接受对话框）
        emit recentList->itemClicked(recentList->item(1)); // pathA
        CHECK(dialog.choice() == StartDialog::Choice::OpenPath
                  && dialog.selectedPath() == pathA,
              "阶段8：单击条目 → OpenPath 且路径正确");
        CHECK(dialog.result() == QDialog::Accepted && !dialog.isVisible(),
              "阶段8：单击条目接受对话框");

        // 右侧三入口：新建 / 空项目（打开存档按钮走原生文件对话框，列手测）
        {
            StartDialog dialog2;
            dialog2.findChild<QPushButton*>(QStringLiteral("newProjectButton"))->click();
            CHECK(dialog2.choice() == StartDialog::Choice::NewProject
                      && dialog2.result() == QDialog::Accepted,
                  "阶段8：新建项目按钮 → NewProject 接受");
        }
        {
            StartDialog dialog3;
            dialog3.findChild<QPushButton*>(QStringLiteral("emptyProjectButton"))->click();
            CHECK(dialog3.choice() == StartDialog::Choice::Empty
                      && dialog3.result() == QDialog::Accepted,
                  "阶段8：直接进入空项目按钮 → Empty 接受");
        }

        // 空列表占位提示
        settings.removeRecentFile(pathA);
        settings.removeRecentFile(pathB);
        {
            StartDialog dialog4;
            auto* stack4 = dialog4.findChild<QStackedWidget*>(QStringLiteral("recentStack"));
            auto* label4 = dialog4.findChild<QLabel*>(QStringLiteral("recentEmptyLabel"));
            CHECK(stack4->currentWidget() == label4
                      && label4->text().contains(QStringLiteral("暂无最近使用的存档")),
                  "阶段8：清空最近列表后空态占位提示");
        }

        // 用例结尾清理 MRU 哨兵（含其它段落 openFromPath 写入的条目）
        for (const QString& p : settings.recentFiles())
            settings.removeRecentFile(p);
        CHECK(settings.recentFiles().isEmpty(), "阶段8：用例结尾清理 MRU 哨兵");
    }

    // ==================== 阶段9：窗口布局持久化 + 倍速标注 ====================
    // ---- 倍速下拉框标注：speedLabel 文本"倍速"，speedCombo ToolTip"播放速度" ----
    {
        auto* speedLabel9 = window.findChild<QLabel*>(QStringLiteral("speedLabel"));
        CHECK(speedLabel9 && speedLabel9->text() == QStringLiteral("倍速"),
              "阶段9：倍速下拉框标注（speedLabel 文本“倍速”）");
        CHECK(speedCombo->toolTip() == QStringLiteral("播放速度"),
              "阶段9：倍速下拉框 ToolTip“播放速度”");
    }

    // ---- 窗口布局持久化：窗口1 改尺寸+浮动 Dock 关闭 → 窗口2 恢复一致 ----
    // 布局键清理由 AppSettings 置空完成（空 = 首次运行语义），沿用 MRU 哨兵模式；
    // 用例前必须清空：上文 closeEvent 用例（w3/w5）已顺带写入布局键
    {
        AppSettings settings9;
        settings9.setWindowGeometry(QByteArray());
        settings9.setWindowState(QByteArray());
        CHECK(settings9.windowGeometry().isEmpty() && settings9.windowState().isEmpty(),
              "阶段9：用例前清空布局设置键");

        // 首次运行（键为空）：默认布局不回归（1440×900，左 Hierarchy / 右 Inspector / 底时间轴）
        {
            MainWindow w9;
            auto* hierarchyDock9 = w9.findChild<QDockWidget*>(QStringLiteral("hierarchyDock"));
            auto* inspectorDock9 = w9.findChild<QDockWidget*>(QStringLiteral("inspectorDock"));
            auto* timelineDock9 = w9.findChild<QDockWidget*>(QStringLiteral("timelineDock"));
            CHECK(w9.size() == QSize(1440, 900), "阶段9：首次运行默认 1440×900");
            CHECK(hierarchyDock9 && inspectorDock9 && timelineDock9
                      && w9.dockWidgetArea(hierarchyDock9) == Qt::LeftDockWidgetArea
                      && w9.dockWidgetArea(inspectorDock9) == Qt::RightDockWidgetArea
                      && w9.dockWidgetArea(timelineDock9) == Qt::BottomDockWidgetArea
                      && !hierarchyDock9->isFloating() && !inspectorDock9->isFloating()
                      && !timelineDock9->isFloating(),
                  "阶段9：首次运行默认三栏布局（左/右/底，均非浮动）");
        }

        // 窗口1：调整尺寸 + 浮动一个 Dock → close() 保存布局
        // （offscreen 屏幕 800×800，restoreGeometry 会把恢复尺寸钳进可用区，故取 700×500）
        const QSize customSize9(700, 500);
        QSize savedSize9;
        {
            MainWindow w9a;
            w9a.resize(customSize9);
            w9a.show();
            QTest::qWaitForWindowExposed(&w9a);
            w9a.findChild<QDockWidget*>(QStringLiteral("inspectorDock"))->setFloating(true);
            CHECK(w9a.size() == customSize9, "阶段9：窗口1 尺寸调整生效");
            savedSize9 = w9a.size();
            w9a.close(); // 不脏：无弹窗直关，closeEvent 落盘布局
            CHECK(!w9a.isVisible(), "阶段9：窗口1 关闭");
        }
        CHECK(!settings9.windowGeometry().isEmpty() && !settings9.windowState().isEmpty(),
              "阶段9：关闭后布局键已写入");

        // 窗口2：几何与 Dock 布局恢复一致
        {
            MainWindow w9b;
            w9b.show();
            QTest::qWaitForWindowExposed(&w9b);
            CHECK(w9b.size() == savedSize9, "阶段9：窗口2 几何恢复一致");
            auto* inspectorDock9b =
                w9b.findChild<QDockWidget*>(QStringLiteral("inspectorDock"));
            CHECK(inspectorDock9b && inspectorDock9b->isFloating(),
                  "阶段9：窗口2 Dock 浮动状态恢复一致");
        }

        // 用例结尾清理布局设置键（哨兵模式），避免残留影响下次运行
        settings9.setWindowGeometry(QByteArray());
        settings9.setWindowState(QByteArray());
        CHECK(settings9.windowGeometry().isEmpty() && settings9.windowState().isEmpty(),
              "阶段9：用例结尾清理布局设置键");
    }

    // ==================== 关闭项目：文件菜单回启动页 ====================
    const auto findCloseProjectAction = [](MainWindow& w) -> QAction* {
        for (QAction* action : w.findChildren<QAction*>()) {
            if (action->text() == QStringLiteral("关闭项目(&C)"))
                return action;
        }
        return nullptr;
    };
    CHECK(findCloseProjectAction(window) != nullptr, "关闭项目：文件菜单项存在");

    // 门卫取消 → 留在原项目，不弹启动页
    {
        MainWindow w10;
        w10.show();
        QTest::qWaitForWindowExposed(&w10);
        auto* core10 = w10.findChild<EditorCore*>();
        auto* runtime10 = w10.findChild<RuntimeData*>();
        CHECK(core10->createObject(ObjectType::Actor, QPointF(0.0, 0.0)),
              "关闭项目：制造未保存修改");
        QAction* action10 = findCloseProjectAction(w10);
        bool startDialogAppeared = false;
        QTimer::singleShot(300, [] {
            if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                box->button(QMessageBox::Cancel)->click();
        });
        QTimer::singleShot(900, [&startDialogAppeared] {
            if (qobject_cast<StartDialog*>(QApplication::activeModalWidget()))
                startDialogAppeared = true;
        });
        action10->trigger();
        QTest::qWait(1000); // 给第二个定时器留出触发窗口
        CHECK(w10.isVisible() && runtime10->isDirty()
                  && runtime10->fileData().objects.size() == 1 && !startDialogAppeared,
              "关闭项目：门卫取消留在原项目、不弹启动页");
    }

    // 门卫选不保存 → 启动页选空项目 → 运行时重置、主窗口重新显示
    {
        MainWindow w11;
        w11.show();
        QTest::qWaitForWindowExposed(&w11);
        auto* core11 = w11.findChild<EditorCore*>();
        auto* runtime11 = w11.findChild<RuntimeData*>();
        CHECK(core11->createObject(ObjectType::Actor, QPointF(0.0, 0.0)),
              "关闭项目：制造未保存修改");
        QAction* action11 = findCloseProjectAction(w11);
        QTimer::singleShot(300, [] {
            if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                box->button(QMessageBox::Discard)->click();
        });
        QTimer::singleShot(900, [] {
            if (auto* dialog = qobject_cast<StartDialog*>(QApplication::activeModalWidget()))
                dialog->findChild<QPushButton*>(QStringLiteral("emptyProjectButton"))->click();
        });
        action11->trigger();
        CHECK(w11.isVisible(), "关闭项目：选空项目后主窗口重新显示");
        CHECK(runtime11->fileData().objects.isEmpty() && !runtime11->isDirty()
                  && w11.windowTitle().contains(QStringLiteral("未命名")),
              "关闭项目：运行时重置为默认空舞台");
        CHECK(!core11->canUndo(), "关闭项目：撤销栈随 newFile 清空");

        // 不脏 → 直弹启动页（无门卫弹窗），再选空项目回来
        bool guardAppeared = false;
        QTimer::singleShot(300, [&guardAppeared] {
            if (qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                guardAppeared = true;
        });
        QTimer::singleShot(900, [] {
            if (auto* dialog = qobject_cast<StartDialog*>(QApplication::activeModalWidget()))
                dialog->findChild<QPushButton*>(QStringLiteral("emptyProjectButton"))->click();
        });
        action11->trigger();
        CHECK(!guardAppeared && w11.isVisible() && runtime11->fileData().objects.isEmpty(),
              "关闭项目：不脏直回启动页（无门卫弹窗）");
    }

    // ============ 阶段10/11：台下准备区（设置页方块编辑器 + Inspector 双侧布局） ============
    {
        // 干净起点：新窗口 + 默认空舞台（1200×800，无准备区）
        MainWindow w12;
        w12.show();
        QTest::qWaitForWindowExposed(&w12);
        auto* core12 = w12.findChild<EditorCore*>();
        auto* runtime12 = w12.findChild<RuntimeData*>();
        CHECK(runtime12->fileData().prepAreas.isEmpty(), "阶段10：默认空舞台无准备区");

        QAction* settingsAction12 = nullptr;
        for (QAction* action : w12.findChildren<QAction*>()) {
            if (action->text() == QStringLiteral("设置(&T)"))
                settingsAction12 = action;
        }
        settingsAction12->trigger();
        SettingsWindow* sw12 = w12.findChild<SettingsWindow*>();
        auto* canvas12 = sw12->findChild<PrepAreaCanvas*>(QStringLiteral("prepAreaCanvas"));
        auto* widthSpin12 = sw12->findChild<QDoubleSpinBox*>(QStringLiteral("stageWidthSpin"));
        auto* colorButton12 = sw12->findChild<QPushButton*>(QStringLiteral("stageColorButton"));
        auto* prepWidthSpin12 = sw12->findChild<QDoubleSpinBox*>(QStringLiteral("prepWidthSpin"));
        auto* prepHeightSpin12 = sw12->findChild<QDoubleSpinBox*>(QStringLiteral("prepHeightSpin"));
        auto* inspectorTitle12 = sw12->findChild<QLabel*>(QStringLiteral("prepInspectorTitle"));
        auto* inspectorStack12 = sw12->findChild<QStackedWidget*>(QStringLiteral("stageInspectorStack"));
        auto* stagePage12 = sw12->findChild<QWidget*>(QStringLiteral("stagePropsPage"));
        auto* prepPage12 = sw12->findChild<QWidget*>(QStringLiteral("prepPropsPage"));
        auto* prepXSpin12 = sw12->findChild<QDoubleSpinBox*>(QStringLiteral("prepXSpin"));
        auto* prepYSpin12 = sw12->findChild<QDoubleSpinBox*>(QStringLiteral("prepYSpin"));
        auto* prepColorButton12 = sw12->findChild<QPushButton*>(QStringLiteral("prepColorButton"));
        CHECK(canvas12 && widthSpin12 && colorButton12 && prepWidthSpin12 && prepHeightSpin12
                  && inspectorTitle12 && inspectorStack12 && stagePage12 && prepPage12
                  && prepXSpin12 && prepYSpin12 && prepColorButton12,
              "阶段11：方块编辑器 + Inspector 双侧布局（标题/双页栈/准备区五行）装配完整");

        // 初始选中舞台 → Inspector 显示舞台属性页（标题"舞台"），准备区页隐藏
        CHECK(canvas12->selectedBlockId() == PrepAreaCanvas::stageBlockId(),
              "阶段10：初始选中舞台方块");
        CHECK(inspectorStack12->currentWidget() == stagePage12
                  && inspectorTitle12->text() == QStringLiteral("舞台"),
              "阶段11：选中舞台 → Inspector 切舞台属性页");
        CHECK(widthSpin12->isVisible() && !prepWidthSpin12->isVisible()
                  && !prepXSpin12->isVisible() && !prepColorButton12->isVisible(),
              "阶段11：舞台页行可见、准备区页行不可见");
        CHECK(near(widthSpin12->value(), 1200.0, 0.01),
              "阶段11：舞台页回显当前舞台值（默认 1200 宽）");

        // 由舞台方块像素矩形（场景 1200×800，左上 (-600,0)）推导画布当前缩放/原点，
        // 供"落点 ≈ 右键处"容差断言（像素取整误差远小于 50cm 吸附网格）
        const QRect stagePx12 = canvas12->blockRectFor(PrepAreaCanvas::stageBlockId());
        CHECK(!stagePx12.isEmpty(), "阶段10：自测钩子 blockRectFor 返回舞台像素矩形");
        const double scale12 = stagePx12.width() / 1200.0;
        const QPointF origin12 = QPointF(stagePx12.left(), stagePx12.top())
            - QPointF(-600.0, 0.0) * scale12; // 场景原点的画布像素位置
        const auto sceneAt = [origin12, scale12](const QPoint& px) {
            return QPointF((px.x() - origin12.x()) / scale12,
                           (px.y() - origin12.y()) / scale12);
        };

        int fileSignals12 = 0;
        QObject::connect(runtime12, &RuntimeData::fileDataChanged, runtime12,
                         [&fileSignals12] { ++fileSignals12; });

        // ---- 右键空白 → 新建准备区（QMenu::exec 模态菜单经 QTimer 驱动） ----
        // offscreen 下 QAction::trigger 不会让 exec 返回；实测可用路径 =
        // setActiveAction + 回车（与手写菜单探针验证一致）
        const auto pickFirstMenuAction = [] {
            if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
                menu->setActiveAction(menu->actions().first());
                QTest::keyClick(menu, Qt::Key_Return);
            }
        };
        QTimer::singleShot(200, pickFirstMenuAction);
        // 舞台方块居中绘制、四周留边 ≥24px：底边中点下方必为空白
        const QPoint blankPos12(canvas12->width() / 2, canvas12->height() - 8);
        QContextMenuEvent blankEvt(QContextMenuEvent::Mouse, blankPos12,
                                   canvas12->mapToGlobal(blankPos12));
        QApplication::sendEvent(canvas12, &blankEvt);
        CHECK(runtime12->fileData().prepAreas.size() == 1, "阶段10：右键空白新建准备区");
        {
            const PrepAreaData& area = runtime12->fileData().prepAreas.first();
            CHECK(area.name == QStringLiteral("准备区1"), "阶段10：自动命名“准备区1”");
            CHECK(area.width == 400.0 && area.height == 300.0, "阶段10：默认 400×300");
            CHECK(!area.color.isValid(), "阶段11：新建准备区默认派生态（color invalid）");
            CHECK(std::fmod(area.x, 50.0) == 0.0 && std::fmod(area.y, 50.0) == 0.0,
                  "阶段10：新建位置吸附 50cm 网格");
            const QPointF expected = sceneAt(blankPos12);
            CHECK(qAbs(area.x - expected.x()) < 30.0 && qAbs(area.y - expected.y()) < 30.0,
                  "阶段10：新建落在右键处（吸附容差内）");
        }
        // 新建后自动选中 → Inspector 切准备区属性页并回显 X/Y/宽/高
        CHECK(canvas12->selectedBlockId() == runtime12->fileData().prepAreas.first().id,
              "阶段10：新建后自动选中新准备区");
        CHECK(inspectorStack12->currentWidget() == prepPage12
                  && inspectorTitle12->text() == QStringLiteral("准备区1"),
              "阶段11：新建后 Inspector 切准备区属性页（标题 = 准备区名）");
        {
            const PrepAreaData& area = runtime12->fileData().prepAreas.first();
            CHECK(near(prepXSpin12->value(), area.x, 0.01)
                      && near(prepYSpin12->value(), area.y, 0.01)
                      && near(prepWidthSpin12->value(), 400.0, 0.01)
                      && near(prepHeightSpin12->value(), 300.0, 0.01),
                  "阶段11：选中准备区回显 X/Y/宽/高");
        }
        CHECK(prepWidthSpin12->isVisible() && prepXSpin12->isVisible()
                  && !widthSpin12->isVisible(),
              "阶段11：准备区页行可见、舞台页行不可见");
        CHECK(prepColorButton12->text().contains(QStringLiteral("跟随舞台色")),
              "阶段11：派生态准备区色块标注“跟随舞台色”");

        // ---- 合成拖拽：拖动中不提交（本地预览），松开单次提交且坐标吸附 ----
        const QString prepId12 = runtime12->fileData().prepAreas.first().id;
        const PrepAreaData beforeDrag = runtime12->fileData().prepAreas.first();
        const QRect prepPx12 = canvas12->blockRectFor(prepId12);
        CHECK(!prepPx12.isEmpty(), "阶段10：准备区方块像素矩形可求");
        // 新建后内容包围盒已变（画布变换重算）：按当前舞台像素矩形重推导变换
        const QRect stagePxNow = canvas12->blockRectFor(PrepAreaCanvas::stageBlockId());
        const double scaleNow = stagePxNow.width() / 1200.0;
        const QPointF originNow = QPointF(stagePxNow.left(), stagePxNow.top())
            - QPointF(-600.0, 0.0) * scaleNow;
        const auto sceneAtNow = [originNow, scaleNow](const QPoint& px) {
            return QPointF((px.x() - originNow.x()) / scaleNow,
                           (px.y() - originNow.y()) / scaleNow);
        };
        const int signalsBeforeDrag = fileSignals12;
        QTest::mousePress(canvas12, Qt::LeftButton, Qt::NoModifier, prepPx12.center());
        QTest::mouseMove(canvas12, prepPx12.center() + QPoint(60, 30)); // 越过拖动阈值
        QTest::mouseMove(canvas12, prepPx12.center() + QPoint(120, 60));
        CHECK(fileSignals12 == signalsBeforeDrag, "阶段10：拖动中不提交（本地预览）");
        QTest::mouseRelease(canvas12, Qt::LeftButton, Qt::NoModifier,
                            prepPx12.center() + QPoint(120, 60));
        CHECK(fileSignals12 == signalsBeforeDrag + 1, "阶段10：拖拽松开单次提交");
        {
            const PrepAreaData& area = runtime12->fileData().prepAreas.first();
            CHECK(std::fmod(area.x, 50.0) == 0.0 && std::fmod(area.y, 50.0) == 0.0,
                  "阶段10：拖拽落点吸附 50cm 网格");
            CHECK(area.x != beforeDrag.x || area.y != beforeDrag.y, "阶段10：拖拽改变了位置");
            const QPointF expected = sceneAtNow(prepPx12.center() + QPoint(120, 60));
            CHECK(qAbs(area.x - expected.x()) < 30.0 && qAbs(area.y - expected.y()) < 30.0,
                  "阶段10：拖拽落点 ≈ 松开处（吸附容差内）");
            // 拖拽提交 → fileDataChanged → 回显：X/Y 数值行与画布位置同步
            CHECK(near(prepXSpin12->value(), area.x, 0.01)
                      && near(prepYSpin12->value(), area.y, 0.01),
                  "阶段11：拖拽提交后 X/Y 数值行回显同步");
        }
        CHECK(!core12->canUndo(), "阶段10：准备区拖拽不产生撤销条目");
        CHECK(runtime12->isDirty(), "阶段10：准备区拖拽弄脏文件");

        // ---- 数值行调整：setValue 不即时提交，editingFinished 直改直提交 ----
        prepWidthSpin12->setValue(500.0);
        prepHeightSpin12->setValue(350.0);
        CHECK(near(runtime12->fileData().prepAreas.first().width, 400.0, 0.01),
              "阶段10：准备区数值 setValue 不即时提交");
        prepWidthSpin12->editingFinished();
        CHECK(near(runtime12->fileData().prepAreas.first().width, 500.0, 0.01)
                  && near(runtime12->fileData().prepAreas.first().height, 350.0, 0.01),
              "阶段10：editingFinished 提交准备区宽高（直改直提交）");
        CHECK(near(prepWidthSpin12->value(), 500.0, 0.01),
              "阶段10：提交后数值行回显同步");
        CHECK(!core12->canUndo(), "阶段10：准备区数值调整不产生撤销条目");

        // ---- X/Y 数值行（阶段11）：editingFinished 提交且画布方块位置同步更新 ----
        {
            const PrepAreaData& area = runtime12->fileData().prepAreas.first();
            prepXSpin12->setValue(area.x + 250.0); // 保持 50cm 网格上的整齐值
            prepYSpin12->setValue(area.y + 150.0);
        }
        prepXSpin12->editingFinished();
        const double expectedX12 = runtime12->fileData().prepAreas.first().x;
        const double expectedY12 = runtime12->fileData().prepAreas.first().y;
        CHECK(expectedX12 == prepXSpin12->value() && expectedY12 == prepYSpin12->value(),
              "阶段11：editingFinished 提交准备区 X/Y（直改直提交）");
        {
            // 画布方块中心应移到新坐标：提交后包围盒可能变化，按当前舞台矩形重推导
            const QRect stagePxSync = canvas12->blockRectFor(PrepAreaCanvas::stageBlockId());
            const double scaleSync = stagePxSync.width() / 1200.0;
            const QPointF originSync = QPointF(stagePxSync.left(), stagePxSync.top())
                - QPointF(-600.0, 0.0) * scaleSync;
            const QPointF expectedPx = originSync + QPointF(expectedX12, expectedY12) * scaleSync;
            const QPointF actualPx = canvas12->blockRectFor(prepId12).center();
            CHECK(qAbs(actualPx.x() - expectedPx.x()) <= 3.0
                      && qAbs(actualPx.y() - expectedPx.y()) <= 3.0,
                  "阶段11：X/Y 提交后画布方块位置同步更新");
        }
        CHECK(!core12->canUndo(), "阶段11：准备区 X/Y 调整不产生撤销条目");

        // ---- 准备区颜色（阶段11）：色块按钮弹 QColorDialog，直改直提交 ----
        // 先存盘复位 dirty，验证颜色改动独立置位
        const QString preColorPath = tempDir.filePath(QStringLiteral("stage11-precolor.json"));
        CHECK(w12.saveToPath(preColorPath) && !runtime12->isDirty(),
              "阶段11：存盘复位 dirty（颜色用例前置）");
        const QColor prepColor12(QStringLiteral("#12ab34"));
        QTimer::singleShot(300, [prepColor12] {
            if (auto* colorDialog =
                    qobject_cast<QColorDialog*>(QApplication::activeModalWidget())) {
                colorDialog->setCurrentColor(prepColor12);
                colorDialog->accept();
            }
        });
        prepColorButton12->click(); // 模态 QColorDialog 由上面的定时器驱动
        CHECK(runtime12->fileData().prepAreas.first().color == prepColor12,
              "阶段11：准备区颜色选中立即生效（独立色）");
        CHECK(runtime12->isDirty(), "阶段11：准备区颜色改动弄脏文件（dirty 置位）");
        CHECK(!core12->canUndo(), "阶段11：准备区颜色不产生撤销条目");
        CHECK(prepColorButton12->text() == prepColor12.name(),
              "阶段11：准备区色块按钮回显独立色色值");

        // 设置窗口截图存档人工复核（双侧布局 + 准备区页回显 + 独立色块）
        sw12->grab().save(
            QDir::temp().absoluteFilePath(QStringLiteral("stage11-settings-prepareas.png")));

        // ---- 选中舞台方块：Inspector 切回舞台属性页 ----
        QTest::mouseClick(canvas12, Qt::LeftButton, Qt::NoModifier,
                          canvas12->blockRectFor(PrepAreaCanvas::stageBlockId()).center());
        CHECK(canvas12->selectedBlockId() == PrepAreaCanvas::stageBlockId(),
              "阶段10：点击舞台方块选中舞台");
        CHECK(inspectorStack12->currentWidget() == stagePage12
                  && inspectorTitle12->text() == QStringLiteral("舞台")
                  && widthSpin12->isVisible() && !prepWidthSpin12->isVisible(),
              "阶段11：点舞台方块 Inspector 切回舞台属性页");

        // ---- 右键平移（同主视口 StageView）：任意位置右键按住平移内容，零提交 ----
        // 按在准备区方块上也平移（主视口语义：不按命中区分），不改变选中、不动数据
        const QRect stagePxPan = canvas12->blockRectFor(PrepAreaCanvas::stageBlockId());
        const QRect prepPxPan = canvas12->blockRectFor(prepId12);
        CHECK(!stagePxPan.isEmpty() && !prepPxPan.isEmpty(),
              "阶段11：平移前方块像素矩形可求");
        const PrepAreaData prepBeforePan = runtime12->fileData().prepAreas.first();
        const int signalsBeforePan = fileSignals12;
        const QPoint panPress = prepPxPan.center(); // 从准备区方块上按下
        QTest::mousePress(canvas12, Qt::RightButton, Qt::NoModifier, panPress);
        CHECK(canvas12->cursor().shape() == Qt::ClosedHandCursor,
              "阶段11：右键按下进入平移光标（仿主视口握拳）");
        CHECK(canvas12->selectedBlockId() == PrepAreaCanvas::stageBlockId(),
              "阶段11：右键平移不改变选中（按在方块上也不选中）");
        QTest::mouseMove(canvas12, panPress + QPoint(40, 25)); // 越过拖动阈值
        QTest::mouseMove(canvas12, panPress + QPoint(80, 50));
        CHECK(fileSignals12 == signalsBeforePan, "阶段11：右键平移中零提交");
        {
            // 拖动期间内容随手移动：方块像素偏移 ≈ 拖拽增量（blockRectFor 钩子）
            const QPoint stageDelta = canvas12->blockRectFor(PrepAreaCanvas::stageBlockId())
                                          .topLeft()
                - stagePxPan.topLeft();
            const QPoint prepDelta = canvas12->blockRectFor(prepId12).topLeft()
                - prepPxPan.topLeft();
            CHECK(qAbs(stageDelta.x() - 80) <= 1 && qAbs(stageDelta.y() - 50) <= 1
                      && qAbs(prepDelta.x() - 80) <= 1 && qAbs(prepDelta.y() - 50) <= 1,
                  "阶段11：右键平移内容随手移动（舞台/准备区像素偏移 = 拖拽增量）");
        }
        QTest::mouseRelease(canvas12, Qt::RightButton, Qt::NoModifier,
                            panPress + QPoint(80, 50));
        CHECK(fileSignals12 == signalsBeforePan, "阶段11：右键平移全程零提交");
        CHECK(runtime12->fileData().prepAreas.first().x == prepBeforePan.x
                  && runtime12->fileData().prepAreas.first().y == prepBeforePan.y,
              "阶段11：右键平移不改数据坐标");
        CHECK(!core12->canUndo(), "阶段11：右键平移不产生撤销条目");
        CHECK(canvas12->cursor().shape() == Qt::ArrowCursor, "阶段11：平移结束光标恢复");
        // Windows 右键松开即 WM_CONTEXTMENU：平移过则吞掉不弹菜单（补发合成事件验证）
        bool popupAfterPan = false;
        QTimer panGuard;
        panGuard.setSingleShot(true);
        QObject::connect(&panGuard, &QTimer::timeout, [&popupAfterPan] {
            if (QWidget* popup = QApplication::activePopupWidget()) {
                popupAfterPan = true;
                popup->close(); // 防御：不应出现菜单，出现则关闭避免卡住
            }
        });
        panGuard.start(300);
        QContextMenuEvent panEvt(QContextMenuEvent::Mouse, panPress + QPoint(80, 50),
                                 canvas12->mapToGlobal(panPress + QPoint(80, 50)));
        QApplication::sendEvent(canvas12, &panEvt); // 被吞则同步返回，不弹菜单
        panGuard.stop();
        CHECK(!popupAfterPan, "阶段11：右键平移后不弹菜单");

        // 平移后的画布截图存档人工复核（内容整体随手偏移、仍完整可见）
        canvas12->grab().save(
            QDir::temp().absoluteFilePath(QStringLiteral("stage11-settings-canvas-pan.png")));

        // ---- 舞台方块不可拖：左键拖后数据不变、无提交 ----
        const StageData stageBefore12 = runtime12->fileData().stage;
        const int signalsBeforeStageDrag = fileSignals12;
        const QRect stagePxDrag = canvas12->blockRectFor(PrepAreaCanvas::stageBlockId());
        QTest::mousePress(canvas12, Qt::LeftButton, Qt::NoModifier, stagePxDrag.center());
        QTest::mouseMove(canvas12, stagePxDrag.center() + QPoint(80, 40));
        QTest::mouseRelease(canvas12, Qt::LeftButton, Qt::NoModifier,
                            stagePxDrag.center() + QPoint(80, 40));
        CHECK(runtime12->fileData().stage == stageBefore12
                  && fileSignals12 == signalsBeforeStageDrag,
              "阶段10：舞台方块不响应拖动（centerOffset 不变、无提交）");

        // ---- 右键准备区 → 删除（QTimer 驱动菜单）；右键点击（未拖拽）不抑制菜单 ----
        const QRect prepPxDel = canvas12->blockRectFor(prepId12);
        QTest::mouseClick(canvas12, Qt::RightButton, Qt::NoModifier, prepPxDel.center());
        QTimer::singleShot(200, pickFirstMenuAction); // “删除”
        QContextMenuEvent prepEvt(QContextMenuEvent::Mouse, prepPxDel.center(),
                                  canvas12->mapToGlobal(prepPxDel.center()));
        QApplication::sendEvent(canvas12, &prepEvt);
        CHECK(runtime12->fileData().prepAreas.isEmpty(),
              "阶段10：右键准备区删除后列表清空（右键点击仍弹菜单）");
        CHECK(canvas12->selectedBlockId() == PrepAreaCanvas::stageBlockId(),
              "阶段10：删除后选中回落舞台");
        CHECK(inspectorStack12->currentWidget() == stagePage12
                  && inspectorTitle12->text() == QStringLiteral("舞台"),
              "阶段11：删除后 Inspector 回落舞台属性页");

        // ---- 右键舞台方块：无菜单 ----
        bool popupAppeared12 = false;
        QTimer popupGuard12;
        popupGuard12.setSingleShot(true);
        QObject::connect(&popupGuard12, &QTimer::timeout, [&popupAppeared12] {
            if (QWidget* popup = QApplication::activePopupWidget()) {
                popupAppeared12 = true;
                popup->close(); // 防御：不应出现菜单，出现则关闭避免卡住
            }
        });
        popupGuard12.start(200);
        const QRect stagePxMenu = canvas12->blockRectFor(PrepAreaCanvas::stageBlockId());
        QContextMenuEvent stageEvt(QContextMenuEvent::Mouse, stagePxMenu.center(),
                                   canvas12->mapToGlobal(stagePxMenu.center()));
        QApplication::sendEvent(canvas12, &stageEvt);
        popupGuard12.stop(); // 未弹菜单则取消守卫定时器
        CHECK(!popupAppeared12, "阶段10：右键舞台方块无菜单");

        // ---- 平移限界（画布无滚动条，等效主视口滚动条范围限界）：疯狂拖拽也不能
        // 把内容推出控件视野（内容包围盒始终与控件保持相交） ----
        {
            const int signalsBeforeClamp = fileSignals12;
            QTest::mousePress(canvas12, Qt::RightButton, Qt::NoModifier, QPoint(10, 10));
            QTest::mouseMove(canvas12, QPoint(10, 10) + QPoint(4000, 3000));
            QTest::mouseRelease(canvas12, Qt::RightButton, Qt::NoModifier,
                                QPoint(10, 10) + QPoint(4000, 3000));
            CHECK(canvas12->blockRectFor(PrepAreaCanvas::stageBlockId())
                      .intersects(canvas12->rect()),
                  "阶段11：平移限界——内容不会被推出控件视野");
            CHECK(fileSignals12 == signalsBeforeClamp, "阶段11：限界平移同样零提交");
            // 吞掉随之而来的 contextMenuEvent（保持抑制标记不残留，防污染后续用例）
            QContextMenuEvent clampEvt(QContextMenuEvent::Mouse, QPoint(10, 10),
                                       canvas12->mapToGlobal(QPoint(10, 10)));
            QApplication::sendEvent(canvas12, &clampEvt);
        }

        // ---- 搜索跨 Inspector 页（阶段11）：命中重挂进结果页，清空挂回原页顺序保持 ----
        auto* searchEdit12 = sw12->findChild<QLineEdit*>(QStringLiteral("settingsSearchEdit"));
        searchEdit12->setText(QStringLiteral("准备区"));
        CHECK(prepXSpin12->isVisible() && prepYSpin12->isVisible()
                  && prepWidthSpin12->isVisible() && prepHeightSpin12->isVisible()
                  && prepColorButton12->isVisible(),
              "阶段11：搜索“准备区”跨 Inspector 页命中（准备区页行重挂进结果页）");
        searchEdit12->clear();
        {
            auto* prepLayout12 = qobject_cast<QVBoxLayout*>(prepPage12->layout());
            auto* stageLayout12 = qobject_cast<QVBoxLayout*>(stagePage12->layout());
            const int ix = prepLayout12->indexOf(prepXSpin12->parentWidget());
            const int iy = prepLayout12->indexOf(prepYSpin12->parentWidget());
            const int iw = prepLayout12->indexOf(prepWidthSpin12->parentWidget());
            const int ih = prepLayout12->indexOf(prepHeightSpin12->parentWidget());
            const int ic = prepLayout12->indexOf(prepColorButton12->parentWidget());
            CHECK(ix >= 0 && ix < iy && iy < iw && iw < ih && ih < ic,
                  "阶段11：清空搜索后准备区行挂回原页且顺序保持（X/Y/宽/高/颜色）");
            CHECK(stageLayout12 && stageLayout12->indexOf(widthSpin12->parentWidget()) >= 0,
                  "阶段11：清空搜索后舞台行挂回舞台属性页");
        }

        sw12->close();
    }

    // ---- 主视口只读渲染 + 按区颜色 + resetView 含准备区（示例存档 v3） ----
    {
        MainWindow w13;
        w13.show();
        QTest::qWaitForWindowExposed(&w13);
        CHECK(w13.openFromPath(demoPath), "阶段11：示例存档（v3，含准备区）打开成功");
        auto* runtime13 = w13.findChild<RuntimeData*>();
        auto* view13 = qobject_cast<StageView*>(w13.centralWidget());
        auto* scene13 = qobject_cast<StageScene*>(view13->scene());
        CHECK(runtime13->fileData().version == 4, "阶段12：示例存档版本号 v4");
        CHECK(runtime13->fileData().prepAreas.size() == 2, "阶段10：示例存档含 2 个准备区");
        CHECK(runtime13->fileData().prepAreas.at(0).color == QColor(QStringLiteral("#d35400")),
              "阶段11：示例存档 prep-001 显式独立色");
        CHECK(!runtime13->fileData().prepAreas.at(1).color.isValid(),
              "阶段11：示例存档 prep-002 不写 color（派生态）");

        // 内容包围盒含准备区：左外侧准备区把左缘推到舞台之外、台下准备区推出底缘
        CHECK(scene13->contentRect().left() < scene13->stageRect().left()
                  && scene13->contentRect().bottom() > scene13->stageRect().bottom(),
              "阶段10：contentRect = 舞台 ∪ 准备区包围盒");
        CHECK(scene13->sceneRect().contains(scene13->contentRect()),
              "阶段10：sceneRect 外扩余量覆盖全部准备区");
        // 只读：准备区画在背景层、无图元——准备区中心命中查询返回空
        CHECK(scene13->topStageItemAt(QPointF(0.0, 1150.0)) == nullptr,
              "阶段10：准备区不可选中（背景层无图元）");

        // resetView 视野中心 = 内容包围盒中心（含准备区，≠ 舞台中心）
        CHECK(scene13->contentRect().center() != scene13->stageRect().center(),
              "阶段10：内容中心 ≠ 舞台中心（准备区参与视野定位）");
        view13->resetView();
        const QPoint viewCenter13 = view13->mapFromScene(scene13->contentRect().center());
        const QPoint vpCenter13 = view13->viewport()->rect().center();
        CHECK(qAbs(viewCenter13.x() - vpCenter13.x()) <= 2
                  && qAbs(viewCenter13.y() - vpCenter13.y()) <= 2,
              "阶段10：resetView 视野中心对准内容包围盒中心");

        // 视口截图存档人工复核（阶段11：prep-001 独立色 / prep-002 派生色 + 名称标签）
        view13->fitInView(scene13->contentRect().adjusted(-80.0, -80.0, 80.0, 80.0),
                          Qt::KeepAspectRatio);
        view13->grab().save(
            QDir::temp().absoluteFilePath(QStringLiteral("stage11-viewport-prepareas.png")));

        // ---- 设置页 Inspector（阶段11）：点舞台/准备区方块切页回显 ----
        QAction* settingsAction13 = nullptr;
        for (QAction* action : w13.findChildren<QAction*>()) {
            if (action->text() == QStringLiteral("设置(&T)"))
                settingsAction13 = action;
        }
        settingsAction13->trigger();
        SettingsWindow* sw13 = w13.findChild<SettingsWindow*>();
        auto* canvas13 = sw13->findChild<PrepAreaCanvas*>(QStringLiteral("prepAreaCanvas"));
        auto* inspectorStack13 = sw13->findChild<QStackedWidget*>(QStringLiteral("stageInspectorStack"));
        auto* inspectorTitle13 = sw13->findChild<QLabel*>(QStringLiteral("prepInspectorTitle"));
        auto* stagePage13 = sw13->findChild<QWidget*>(QStringLiteral("stagePropsPage"));
        auto* prepPage13 = sw13->findChild<QWidget*>(QStringLiteral("prepPropsPage"));
        auto* widthSpin13 = sw13->findChild<QDoubleSpinBox*>(QStringLiteral("stageWidthSpin"));
        auto* depthSpin13 = sw13->findChild<QDoubleSpinBox*>(QStringLiteral("stageDepthSpin"));
        auto* offsetSpin13 = sw13->findChild<QDoubleSpinBox*>(QStringLiteral("stageOffsetSpin"));
        auto* prepXSpin13 = sw13->findChild<QDoubleSpinBox*>(QStringLiteral("prepXSpin"));
        auto* prepYSpin13 = sw13->findChild<QDoubleSpinBox*>(QStringLiteral("prepYSpin"));
        auto* prepWidthSpin13 = sw13->findChild<QDoubleSpinBox*>(QStringLiteral("prepWidthSpin"));
        auto* prepHeightSpin13 = sw13->findChild<QDoubleSpinBox*>(QStringLiteral("prepHeightSpin"));
        auto* prepColorButton13 = sw13->findChild<QPushButton*>(QStringLiteral("prepColorButton"));
        CHECK(canvas13 && inspectorStack13 && inspectorTitle13 && stagePage13 && prepPage13
                  && widthSpin13 && depthSpin13 && offsetSpin13 && prepXSpin13 && prepYSpin13
                  && prepWidthSpin13 && prepHeightSpin13 && prepColorButton13,
              "阶段11：示例存档窗口的设置页 Inspector 装配完整");

        // 点舞台方块 → 舞台属性页回显示例存档值（1500×900 / 偏移 100）
        QTest::mouseClick(canvas13, Qt::LeftButton, Qt::NoModifier,
                          canvas13->blockRectFor(PrepAreaCanvas::stageBlockId()).center());
        CHECK(inspectorStack13->currentWidget() == stagePage13
                  && inspectorTitle13->text() == QStringLiteral("舞台"),
              "阶段11：点舞台方块 → Inspector 切舞台属性页");
        CHECK(near(widthSpin13->value(), 1500.0, 0.01)
                  && near(depthSpin13->value(), 900.0, 0.01)
                  && near(offsetSpin13->value(), 100.0, 0.01),
              "阶段11：点舞台 → 舞台页回显示例存档值");

        // 点准备区1（独立色）→ 准备区页回显 X/Y/宽/高 + 色块显示色值
        QTest::mouseClick(canvas13, Qt::LeftButton, Qt::NoModifier,
                          canvas13->blockRectFor(QStringLiteral("prep-001")).center());
        CHECK(inspectorStack13->currentWidget() == prepPage13
                  && inspectorTitle13->text() == QStringLiteral("准备区1"),
              "阶段11：点准备区方块 → Inspector 切准备区属性页");
        CHECK(near(prepXSpin13->value(), -1050.0, 0.01)
                  && near(prepYSpin13->value(), 450.0, 0.01)
                  && near(prepWidthSpin13->value(), 400.0, 0.01)
                  && near(prepHeightSpin13->value(), 300.0, 0.01),
              "阶段11：点准备区 → X/Y/宽/高回显");
        CHECK(prepColorButton13->text() == QColor(QStringLiteral("#d35400")).name(),
              "阶段11：独立色准备区色块回显色值");

        // 点准备区2（派生态）→ 色块标注“跟随舞台色”
        QTest::mouseClick(canvas13, Qt::LeftButton, Qt::NoModifier,
                          canvas13->blockRectFor(QStringLiteral("prep-002")).center());
        CHECK(inspectorTitle13->text() == QStringLiteral("准备区2")
                  && near(prepXSpin13->value(), 0.0, 0.01)
                  && near(prepYSpin13->value(), 1150.0, 0.01)
                  && near(prepWidthSpin13->value(), 600.0, 0.01)
                  && near(prepHeightSpin13->value(), 400.0, 0.01),
              "阶段11：点准备区2 → X/Y/宽/高回显");
        CHECK(prepColorButton13->text().contains(QStringLiteral("跟随舞台色")),
              "阶段11：派生态准备区色块标注“跟随舞台色”");

        // 设置窗口截图存档人工复核（双侧布局：左画布选中准备区 + 右 Inspector 准备区页）
        sw13->grab().save(
            QDir::temp().absoluteFilePath(QStringLiteral("stage11-settings-inspector.png")));
        sw13->close();
    }

    // ---- v1 旧存档兼容实测（真实 MainWindow::openFromPath 打开路径） ----
    {
        // 真实构造 version=1、无 prepAreas 节点的 JSON 存档
        const QString v1Path = tempDir.filePath(QStringLiteral("stage10-v1-legacy.json"));
        {
            QFile f(v1Path);
            CHECK(f.open(QIODevice::WriteOnly | QIODevice::Truncate), "阶段10：写入 v1 存档");
            f.write(R"({
  "version": 1,
  "stage": { "width": 1200, "depth": 800, "centerOffset": 0 },
  "objects": [
    {
      "id": "actor-001", "type": "actor", "name": "旧角", "color": "#e74c3c",
      "transform": { "x": 100, "y": 100, "rotation": 0 }, "keyframes": []
    }
  ],
  "timeline": { "duration": 60.0, "labels": [] }
})");
        }
        MainWindow w14;
        w14.show();
        QTest::qWaitForWindowExposed(&w14);
        CHECK(w14.openFromPath(v1Path), "阶段10：v1 旧存档（无 prepAreas 节点）打开成功");
        auto* runtime14 = w14.findChild<RuntimeData*>();
        auto* view14 = qobject_cast<StageView*>(w14.centralWidget());
        auto* scene14 = qobject_cast<StageScene*>(view14->scene());
        CHECK(runtime14->fileData().prepAreas.isEmpty(), "阶段10：v1 迁移后无准备区");
        CHECK(scene14->contentRect() == scene14->stageRect(),
              "阶段10：v1 存档 contentRect 退化为舞台矩形");

        // 保存后回读为 v3 格式（含恒写的 prepAreas 空数组）
        const QString v2Path = tempDir.filePath(QStringLiteral("stage10-v1-resaved.json"));
        CHECK(w14.saveToPath(v2Path), "阶段10：v1 存档另存成功");
        QFile f2(v2Path);
        CHECK(f2.open(QIODevice::ReadOnly), "阶段10：回读另存文件");
        const QJsonDocument doc = QJsonDocument::fromJson(f2.readAll());
        CHECK(doc.object().value(QStringLiteral("version")).toDouble() == 4.0,
              "阶段12：保存后版本号升为 v4");
        CHECK(doc.object().contains(QStringLiteral("prepAreas"))
                  && doc.object().value(QStringLiteral("prepAreas")).isArray(),
              "阶段10：保存后恒写 prepAreas 数组（空也写 []）");
        CHECK(!runtime14->isDirty(), "阶段10：另存后干净");
    }

    // ---- v2 旧存档兼容实测（阶段11：prepAreas 元素无 color 键 → 派生态） ----
    {
        // 真实构造 version=2、prepAreas 无 color 键的 JSON 存档
        const QString v2LegacyPath = tempDir.filePath(QStringLiteral("stage11-v2-legacy.json"));
        {
            QFile f(v2LegacyPath);
            CHECK(f.open(QIODevice::WriteOnly | QIODevice::Truncate), "阶段11：写入 v2 存档");
            f.write(R"({
  "version": 2,
  "stage": { "width": 1200, "depth": 800, "centerOffset": 0 },
  "objects": [],
  "prepAreas": [
    { "id": "prep-001", "name": "准备区1", "x": -800, "y": 500, "width": 400, "height": 300 },
    { "id": "prep-002", "name": "准备区2", "x": 0, "y": 1100, "width": 600, "height": 400 }
  ],
  "timeline": { "duration": 60.0, "labels": [] }
})");
        }
        MainWindow w15;
        w15.show();
        QTest::qWaitForWindowExposed(&w15);
        CHECK(w15.openFromPath(v2LegacyPath), "阶段11：v2 旧存档（prepAreas 无 color 键）打开成功");
        auto* runtime15 = w15.findChild<RuntimeData*>();
        CHECK(runtime15->fileData().version == 4, "阶段12：v2 迁移后版本号升为 v4");
        CHECK(runtime15->fileData().prepAreas.size() == 2, "阶段11：v2 迁移后准备区保留");
        bool allDerived15 = true;
        for (const PrepAreaData& area : runtime15->fileData().prepAreas)
            allDerived15 = allDerived15 && !area.color.isValid();
        CHECK(allDerived15, "阶段11：v2 迁移准备区全部派生态（视觉零变化）");

        // 视口截图存档人工复核（v2 旧档准备区显示派生色 = 与阶段10 观感一致）
        auto* view15 = qobject_cast<StageView*>(w15.centralWidget());
        auto* scene15 = qobject_cast<StageScene*>(view15->scene());
        view15->fitInView(scene15->contentRect().adjusted(-80.0, -80.0, 80.0, 80.0),
                          Qt::KeepAspectRatio);
        view15->grab().save(
            QDir::temp().absoluteFilePath(QStringLiteral("stage11-v2-legacy-viewport.png")));

        // 另存回读为 v3 格式：prepAreas 元素恒写 color 键（派生态 → 空串）
        const QString v3Path = tempDir.filePath(QStringLiteral("stage11-v2-resaved.json"));
        CHECK(w15.saveToPath(v3Path), "阶段11：v2 存档另存成功");
        QFile f3(v3Path);
        CHECK(f3.open(QIODevice::ReadOnly), "阶段11：回读另存文件");
        const QJsonObject root15 = QJsonDocument::fromJson(f3.readAll()).object();
        CHECK(root15.value(QStringLiteral("version")).toDouble() == 4.0,
              "阶段12：另存后版本号升为 v4");
        const QJsonArray areas15 = root15.value(QStringLiteral("prepAreas")).toArray();
        bool allHaveColorKey = areas15.size() == 2;
        for (const QJsonValue& entry : areas15)
            allHaveColorKey = allHaveColorKey
                && entry.toObject().contains(QStringLiteral("color"));
        CHECK(allHaveColorKey, "阶段11：另存后 prepAreas 恒写 color 键");
        CHECK(root15.contains(QStringLiteral("paths"))
                  && root15.value(QStringLiteral("paths")).isArray(),
              "阶段12：另存后恒写 paths 数组（空也写 []）");
        CHECK(!runtime15->isDirty(), "阶段11：另存后干净");
    }

    // ==================== 阶段12：双帧选中态 + 幽灵与直线路径可视化（纯渲染） ====================
    {
        MainWindow w16;
        w16.show();
        QTest::qWaitForWindowExposed(&w16);
        auto* core16 = w16.findChild<EditorCore*>();
        auto* runtime16 = w16.findChild<RuntimeData*>();
        auto* view16 = qobject_cast<StageView*>(w16.centralWidget());
        auto* scene16 = qobject_cast<StageScene*>(view16->scene());
        auto* timeline16 = w16.findChild<TimelineWidget*>();
        auto* canvas16 = w16.findChild<QWidget*>(QStringLiteral("timelineCanvas"));
        CHECK(core16 && runtime16 && view16 && scene16 && timeline16 && canvas16,
              "阶段12：组件装配完整");

        // 造数据（业务入口直调）：演员甲 4 帧（t=1/2/3/4，位置递进、朝向各异），
        // 演员乙 2 帧（跨对象用例）
        CHECK(core16->createObject(ObjectType::Actor, QPointF(0.0, 0.0)), "阶段12：创建演员甲");
        const QString actorA16 = runtime16->fileData().objects.last().id;
        CHECK(core16->createObject(ObjectType::Actor, QPointF(0.0, 0.0)), "阶段12：创建演员乙");
        const QString actorB16 = runtime16->fileData().objects.last().id;
        core16->upsertKeyframe(actorA16, Keyframe { 1.0, 100.0, 100.0, 0.0 });
        core16->upsertKeyframe(actorA16, Keyframe { 2.0, 200.0, 100.0, 45.0 });
        core16->upsertKeyframe(actorA16, Keyframe { 3.0, 300.0, 100.0, 90.0 });
        core16->upsertKeyframe(actorA16, Keyframe { 4.0, 400.0, 100.0, 135.0 });
        core16->upsertKeyframe(actorB16, Keyframe { 1.0, -200.0, 300.0, 0.0 });
        core16->upsertKeyframe(actorB16, Keyframe { 2.0, -300.0, 300.0, 0.0 });

        // 场景覆盖物快照：幽灵（StageObjectItem::isGhost，按透明度分前后帧）+ 白线
        struct OverlayState {
            int ghosts = 0;
            qreal opacity[2] = { -1.0, -1.0 }; // [0]=前帧（不透明） [1]=后帧（0.4）
            QPointF pos[2];
            double rotation[2] = { 0.0, 0.0 };
            bool hasLine = false;
            QLineF line;
        };
        const auto overlayState = [scene16] {
            OverlayState o;
            for (QGraphicsItem* item : scene16->items()) {
                if (auto* stageItem = dynamic_cast<StageObjectItem*>(item)) {
                    if (!stageItem->isGhost())
                        continue;
                    ++o.ghosts;
                    const int slot = stageItem->opacity() > 0.9 ? 0 : 1;
                    o.opacity[slot] = stageItem->opacity();
                    o.pos[slot] = stageItem->pos();
                    o.rotation[slot] = stageItem->rotation();
                } else if (auto* pathItem = dynamic_cast<QGraphicsPathItem*>(item)) {
                    // 路径折线（阶段12 起直线/弧线统一为 QGraphicsPathItem）
                    o.hasLine = true;
                    const QPainterPath& path = pathItem->path();
                    o.line = QLineF(
                        QPointF(path.elementAt(0).x, path.elementAt(0).y),
                        QPointF(path.elementAt(path.elementCount() - 1).x,
                                path.elementAt(path.elementCount() - 1).y));
                }
            }
            return o;
        };
        const auto expectSegment = [core16](const QString& id, double from, double to) {
            const std::optional<SegmentRef> seg = core16->activeSegment();
            return seg.has_value() && seg->objectId == id
                && near(seg->from, from, 0.001) && near(seg->to, to, 0.001);
        };
        const auto clickKey = [canvas16, timeline16](const QString& id, double t,
                                                     bool ctrl = false) {
            QTest::mouseClick(canvas16, Qt::LeftButton,
                              ctrl ? Qt::ControlModifier : Qt::NoModifier,
                              timeline16->canvasPosFor(id, t));
        };
        const auto formSegment = [canvas16, &clickKey](const QString& id, double from, double to) {
            QTest::keyClick(canvas16, Qt::Key_Escape); // 清画布选择，避免既有选中干扰
            clickKey(id, from);      // 单选第一帧
            clickKey(id, to, true);  // Ctrl 加选相邻第二帧
        };

        // ---- 单选不进双帧态；Ctrl 加选相邻帧进入 ----
        clickKey(actorA16, 2.0);
        CHECK(!core16->activeSegment().has_value(), "阶段12：单选关键帧不进双帧态");
        clickKey(actorA16, 3.0, true);
        CHECK(expectSegment(actorA16, 2.0, 3.0), "阶段12：Ctrl 加选相邻帧进入双帧态");
        {
            const OverlayState o = overlayState();
            CHECK(o.ghosts == 2 && o.hasLine, "阶段12：双帧态绘制前/后帧幽灵 + 直线白线");
            CHECK(near(o.pos[0].x(), 200.0, 0.01) && near(o.pos[0].y(), 100.0, 0.01)
                      && near(o.opacity[0], 1.0, 0.001)
                      && near(o.rotation[0], 45.0, 0.01),
                  "阶段12：前帧幽灵不透明，位姿（含朝向）= 关键帧数据值");
            CHECK(near(o.pos[1].x(), 300.0, 0.01) && near(o.opacity[1], 0.4, 0.001)
                      && near(o.rotation[1], 90.0, 0.01),
                  "阶段12：后帧幽灵 0.4 透明，位姿 = 关键帧数据值");
            CHECK(near(o.line.x1(), 200.0, 0.01) && near(o.line.y1(), 100.0, 0.01)
                      && near(o.line.x2(), 300.0, 0.01) && near(o.line.y2(), 100.0, 0.01),
                  "阶段12：白线两端点 = 两帧关键帧位置");
        }
        // 幽灵不参与选中与命中：幽灵位置命中最上层对象 = 空（真实演员此刻不在此处）；
        // 对象遍历不含幽灵（2 个真实对象）
        CHECK(scene16->topStageItemAt(QPointF(200.0, 100.0)) == nullptr,
              "阶段12：幽灵不参与命中");
        CHECK(scene16->stageItems().size() == 2, "阶段12：幽灵不进对象遍历（选中/框选）");

        // 幽灵与插值显示值分离：游标移到段中点，真实图元走插值，幽灵不动
        core16->timelinePlayer()->seekTo(2.5);
        CHECK(near(scene16->itemById(actorA16)->pos().x(), 250.0, 0.01),
              "阶段12：游标移动正常驱动插值显示值");
        CHECK(near(overlayState().pos[0].x(), 200.0, 0.01)
                  && near(overlayState().pos[1].x(), 300.0, 0.01),
              "阶段12：幽灵保持关键帧数据值（不随游标插值）");

        // 截图存档人工复核（游标移回段外：真实图元让开，两幽灵各守帧位、白线显露）
        core16->timelinePlayer()->seekTo(0.0);
        view16->fitInView(QRectF(50.0, 0.0, 400.0, 200.0), Qt::KeepAspectRatio);
        view16->grab().save(
            QDir::temp().absoluteFilePath(QStringLiteral("stage12-segment-ghosts.png")));

        // ---- Ctrl 反选其中一帧 → 退出双帧态，覆盖物移除 ----
        clickKey(actorA16, 3.0, true);
        CHECK(!core16->activeSegment().has_value(), "阶段12：Ctrl 反选一帧退出双帧态");
        {
            const OverlayState o = overlayState();
            CHECK(o.ghosts == 0 && !o.hasLine, "阶段12：双帧态消失后幽灵与白线移除");
        }

        // ---- 加选不相邻帧 → 替换为新单选（中间隔 t=3） ----
        clickKey(actorA16, 4.0, true); // 当前 {2}，加选 4 不相邻
        CHECK(!core16->activeSegment().has_value(),
              "阶段12：加选不相邻帧不进双帧态（替换为新单选）");
        clickKey(actorA16, 3.0, true); // 若已替换为单选 {4}：加选相邻 3 → 双帧 {3,4}
        CHECK(expectSegment(actorA16, 3.0, 4.0),
              "阶段12：替换生效后再加选相邻帧进入双帧态（证明已替换为单选）");

        // ---- 跨对象加选 → 替换为新单选 ----
        clickKey(actorA16, 2.0);       // 单选演员甲 t=2
        clickKey(actorB16, 1.0, true); // Ctrl 加选演员乙的帧 → 替换
        CHECK(!core16->activeSegment().has_value(), "阶段12：跨对象加选替换为新单选");
        clickKey(actorB16, 2.0, true); // 已替换为 {乙@1}：加选相邻 {乙@2} → 双帧 {1,2}
        CHECK(expectSegment(actorB16, 1.0, 2.0),
              "阶段12：跨对象替换后再加选相邻帧进入双帧态");

        // ---- 清除路径：点空白（轨道行无帧处 = 选中轨道，清关键帧选择） ----
        QTest::mouseClick(canvas16, Qt::LeftButton, Qt::NoModifier,
                          timeline16->canvasPosFor(actorB16, 8.0));
        CHECK(!core16->activeSegment().has_value(), "阶段12：点空白（选轨道）清除双帧态");

        // ---- 清除路径：单选他帧 ----
        formSegment(actorA16, 2.0, 3.0);
        CHECK(expectSegment(actorA16, 2.0, 3.0), "阶段12：双帧态重建（演员甲 2→3）");
        clickKey(actorA16, 4.0);
        CHECK(!core16->activeSegment().has_value(), "阶段12：单选他帧清除双帧态");

        // ---- 清除路径：轨道选中变化（外部对象单选联动画布轨道选中） ----
        formSegment(actorA16, 2.0, 3.0);
        core16->selection()->select(actorA16);
        CHECK(!core16->activeSegment().has_value(), "阶段12：轨道选中变化清除双帧态");

        // ---- 清除路径：播放开始 ----
        formSegment(actorA16, 2.0, 3.0);
        core16->timelinePlayer()->play();
        CHECK(!core16->activeSegment().has_value(), "阶段12：播放开始清除双帧态");
        core16->timelinePlayer()->pause();

        // ---- 清除路径：命令提交后防御（段内打帧破坏相邻性） ----
        formSegment(actorA16, 2.0, 3.0);
        CHECK(core16->upsertKeyframe(actorA16, Keyframe { 2.5, 250.0, 100.0, 60.0 }),
              "阶段12：段内打帧（t=2.5）");
        CHECK(!core16->activeSegment().has_value(),
              "阶段12：段内打帧破坏相邻性 → 防御清除双帧态");
        CHECK(overlayState().ghosts == 0, "阶段12：防御清除后幽灵移除");
        core16->undo(); // 撤销打帧，帧列表恢复 1/2/3/4

        // ---- 清除路径：命令提交后防御（删除其中一帧） ----
        formSegment(actorA16, 2.0, 3.0);
        CHECK(core16->removeKeyframes({ qMakePair(actorA16, 3.0) }), "阶段12：删除后帧");
        CHECK(!core16->activeSegment().has_value(),
              "阶段12：删除其中一帧 → 防御清除双帧态");
        {
            const OverlayState o = overlayState();
            CHECK(o.ghosts == 0 && !o.hasLine, "阶段12：删帧后场景无覆盖物");
        }
        core16->undo(); // 恢复帧列表

        // ---- 清除路径：fileReplaced（新建文件） ----
        formSegment(actorA16, 2.0, 3.0);
        CHECK(expectSegment(actorA16, 2.0, 3.0), "阶段12：双帧态重建（新建前）");
        runtime16->newFile();
        CHECK(!core16->activeSegment().has_value(), "阶段12：fileReplaced 清除双帧态");
        {
            const OverlayState o = overlayState();
            CHECK(o.ghosts == 0 && !o.hasLine, "阶段12：新建后场景无覆盖物");
        }
    }

    // ============ 阶段12 任务2：路径编辑（点选白线/Inspector 路径页/加点/拖点/级联） ============
    {
        MainWindow w17;
        w17.show();
        QTest::qWaitForWindowExposed(&w17);
        auto* core17 = w17.findChild<EditorCore*>();
        auto* runtime17 = w17.findChild<RuntimeData*>();
        auto* view17 = qobject_cast<StageView*>(w17.centralWidget());
        auto* scene17 = qobject_cast<StageScene*>(view17->scene());
        auto* timeline17 = w17.findChild<TimelineWidget*>();
        auto* canvas17 = w17.findChild<QWidget*>(QStringLiteral("timelineCanvas"));
        QWidget* vp17 = view17->viewport();
        CHECK(core17 && runtime17 && view17 && scene17 && timeline17 && canvas17,
              "阶段12：路径编辑用例组件装配完整");

        // 造数据：演员甲 2 帧段 1s→3s（(100,100)→(300,100) 直线）
        CHECK(core17->createObject(ObjectType::Actor, QPointF(0.0, 0.0)), "阶段12：路径编辑：创建演员");
        const QString actor17 = runtime17->fileData().objects.last().id;
        core17->upsertKeyframe(actor17, Keyframe { 1.0, 100.0, 100.0, 0.0 });
        core17->upsertKeyframe(actor17, Keyframe { 3.0, 300.0, 100.0, 0.0 });

        // 双帧态：Ctrl 加选相邻两帧
        QTest::mouseClick(canvas17, Qt::LeftButton, Qt::NoModifier,
                          timeline17->canvasPosFor(actor17, 1.0));
        QTest::mouseClick(canvas17, Qt::LeftButton, Qt::ControlModifier,
                          timeline17->canvasPosFor(actor17, 3.0));
        CHECK(core17->activeSegment().has_value(), "阶段12：路径编辑：双帧态就绪");

        // 视口适配到段区域
        view17->fitInView(QRectF(50.0, -60.0, 350.0, 260.0), Qt::KeepAspectRatio);
        const auto viewPos = [view17](const QPointF& scenePos) {
            return view17->mapFromScene(scenePos);
        };

        auto* pathTypeCombo = w17.findChild<QComboBox*>(QStringLiteral("pathTypeCombo"));
        auto* pathNameEdit = w17.findChild<QLineEdit*>(QStringLiteral("pathNameEdit"));
        auto* pathMetrics = w17.findChild<QLabel*>(QStringLiteral("pathMetricsLabel"));
        auto* pathTSpin = w17.findChild<QDoubleSpinBox*>(QStringLiteral("pathPointTSpin"));
        CHECK(pathTypeCombo && pathNameEdit && pathMetrics && pathTSpin,
              "阶段12：Inspector 路径页控件装配完整");

        // ---- 点选白线（直线中点）→ activePath + Inspector 路径页 ----
        CHECK(!core17->activePath().has_value(), "阶段12：点选前无 activePath");
        // dirty 对照基准：前置编辑（建对象/打帧）已弄脏文件，点选白线不得改变它
        const bool dirtyBeforeLineClick = runtime17->isDirty();
        QTest::mouseClick(vp17, Qt::LeftButton, Qt::NoModifier, viewPos(QPointF(200.0, 100.0)));
        CHECK(core17->activePath().has_value()
                  && core17->activePath()->pointIndex == -1,
              "阶段12：点选白线设置 activePath（仅路径）");
        CHECK(pathTypeCombo->isVisible(), "阶段12：Inspector 切到路径页");
        CHECK(pathNameEdit->text() == QStringLiteral("%1_1s至3s").arg(
                  runtime17->fileData().objects.first().name),
              "阶段12：路径名派生（<对象名>_<起>s至<止>s）");
        CHECK(pathNameEdit->isReadOnly(), "阶段12：路径名只读");
        CHECK(pathTypeCombo->currentIndex() == 0, "阶段12：默认连线类型 = 直线");
        auto* legSpin0 = w17.findChild<QDoubleSpinBox*>(QStringLiteral("pathLegAngleSpin0"));
        CHECK(legSpin0 && !legSpin0->isEnabled(),
              "阶段12：直线时圆心角单行存在但禁用");
        CHECK(pathMetrics->text().contains(QStringLiteral("段时长 2.0 s")),
              "阶段12：只读段时长显示");
        CHECK(runtime17->isDirty() == dirtyBeforeLineClick,
              "阶段12：点选白线不弄脏文件（纯运行时状态）");

        // ---- 切弧线：类型下拉 → 建 PathData（默认圆心角），白线变弯 ----
        pathTypeCombo->setCurrentIndex(1); // 弧线（currentIndexChanged → 命令提交）
        {
            const PathData* path = core17->findPath(actor17, 1.0, 3.0);
            CHECK(path && path->type == PathType::Arc
                      && path->legAngles.size() == 1 && path->legAngles.first() > 0.0,
                  "阶段12：切弧线建 PathData（默认圆心角）");
        }
        CHECK(runtime17->isDirty(), "阶段12：路径类型修改弄脏文件（走命令入撤销栈）");
        CHECK(legSpin0->isEnabled() && near(legSpin0->value(), 30.0, 0.01),
              "阶段12：弧线时圆心角行启用并回显默认 30°");

        // ---- 圆心角编辑：数值框提交走命令，弧顶高度随角度变化 ----
        legSpin0->setValue(90.0);
        CHECK(near(core17->findPath(actor17, 1.0, 3.0)->legAngles.first(), 90.0, 0.001),
              "阶段12：圆心角编辑生效（90°）");
        {
            // θ=90°，弦 (100,100)→(300,100)：弧顶在行进方向左侧（y 减小）≈ (200, 58.6)
            const QList<QPointF> polyline = scene17->segmentPathPolyline();
            const QPointF apex = polyline.at(polyline.size() / 2);
            CHECK(apex.y() < 100.0 - 30.0, "阶段12：弧线渲染向左鼓出（弧顶 y 明显小于弦）");
        }

        // ---- 右键白线 → 添加路径点（QTimer 驱动菜单，落点吸附在曲线上） ----
        const QPointF apexScene(200.0, 58.6);
        bool menuSeen17 = false;
        QTimer::singleShot(200, [&menuSeen17] {
            if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
                // 守卫：只在确为路径菜单时回车（防误点对象菜单 Create 卡死）；
                // 弹了别的菜单则关闭它，让断言失败而不是挂住
                if (!menu->actions().isEmpty()
                    && menu->actions().first()->text() == QStringLiteral("添加路径点")) {
                    menuSeen17 = true;
                    menu->setActiveAction(menu->actions().first());
                    QTest::keyClick(menu, Qt::Key_Return);
                } else {
                    menu->close();
                }
            }
        });
        QContextMenuEvent lineEvt(QContextMenuEvent::Mouse, viewPos(apexScene),
                                  view17->mapToGlobal(viewPos(apexScene)));
        QApplication::sendEvent(vp17, &lineEvt); // 右键事件真实投递目标是 viewport
        CHECK(menuSeen17, "阶段12：右键白线弹出路径菜单（添加路径点/恢复直线）");
        {
            const PathData* path = core17->findPath(actor17, 1.0, 3.0);
            CHECK(path && path->points.size() == 1 && path->legAngles.size() == 2,
                  "阶段12：右键白线添加路径点（1 点 2 段）");
            // 保形：加点瞬间形状不变——弧顶位置仍在新折线上
            const QList<QPointF> polyline = scene17->segmentPathPolyline();
            double minDist = 1e9;
            for (const QPointF& p : polyline)
                minDist = qMin(minDist, QLineF(p, apexScene).length());
            CHECK(minDist < 2.0, "阶段12：加点保形（弧顶仍在新折线上）");
        }
        CHECK(scene17->pathPointMarkerPositions().size() == 1,
              "阶段12：路径点标记显示（选中路径时）");

        // ---- 点选路径点 → activePath.pointIndex + Inspector t 行回显 ----
        const QPointF pointScene = [core17, actor17] {
            const PathPoint& p = core17->findPath(actor17, 1.0, 3.0)->points.first();
            return QPointF(p.x, p.y);
        }();
        QTest::mouseClick(vp17, Qt::LeftButton, Qt::NoModifier, viewPos(pointScene));
        CHECK(core17->activePath().has_value()
                  && core17->activePath()->pointIndex == 0,
              "阶段12：点选路径点（pointIndex=0）");
        CHECK(pathTSpin->isVisible(), "阶段12：选中路径点后 t 行显示");
        CHECK(near(pathTSpin->value(), 0.5, 0.02),
              "阶段12：t 回显弧长百分比（弧顶 ≈ 0.5）");

        // ---- 拖拽路径点：拖动中零提交（本地预览随动），松开吸附提交，t 回显跟随 ----
        runtime17->settings().setGridSnapEnabled(false); // 关吸附保证精确落点
        int fileSignals17 = 0;
        QObject::connect(runtime17, &RuntimeData::fileDataChanged, runtime17,
                         [&fileSignals17] { ++fileSignals17; });
        const int signalsBeforePointDrag = fileSignals17;
        // 斜向拖（避开对称位置保证 t 变化可断言）
        QTest::mousePress(vp17, Qt::LeftButton, Qt::NoModifier, viewPos(pointScene));
        QTest::mouseMove(vp17, viewPos(pointScene) + QPoint(20, 20)); // 越过阈值
        QTest::mouseMove(vp17, viewPos(QPointF(150.0, 40.0)));
        CHECK(fileSignals17 == signalsBeforePointDrag, "阶段12：拖拽路径点中零提交（本地预览）");
        CHECK(!scene17->pathPointMarkerPositions().isEmpty()
                  && near(scene17->pathPointMarkerPositions().first().x(), 150.0, 2.0)
                  && near(scene17->pathPointMarkerPositions().first().y(), 40.0, 2.0),
              "阶段12：拖拽路径点预览随动（标记与折线同步）");
        QTest::mouseRelease(vp17, Qt::LeftButton, Qt::NoModifier, viewPos(QPointF(150.0, 40.0)));
        CHECK(fileSignals17 == signalsBeforePointDrag + 1, "阶段12：拖拽路径点松开单次提交");
        CHECK(near(core17->findPath(actor17, 1.0, 3.0)->points.first().x, 150.0, 0.5)
                  && near(core17->findPath(actor17, 1.0, 3.0)->points.first().y, 40.0, 0.5),
              "阶段12：拖拽路径点提交落点");
        CHECK(qAbs(pathTSpin->value() - 0.5) > 0.02,
              "阶段12：拖拽后 t 回显投影弧长百分比（随位置刷新）");
        CHECK(core17->undo(), "阶段12：undo 拖拽路径点");
        CHECK(near(core17->findPath(actor17, 1.0, 3.0)->points.first().y,
                   pointScene.y(), 0.01),
              "阶段12：undo 恢复路径点位置");

        // ---- 改 t：点沿曲线滑动重定位 ----
        pathTSpin->setValue(0.25); // valueChanged → 命令
        {
            const PathData* path = core17->findPath(actor17, 1.0, 3.0);
            const double readBack = PathGeometry::projectToRatio(
                scene17->segmentPathPolyline(),
                QPointF(path->points.first().x, path->points.first().y));
            CHECK(near(readBack, 0.25, 0.02), "阶段12：改 t 后点滑动到弧长 25% 处");
        }

        // ---- 方向键微调路径点（视口焦点，无对象选中）----
        core17->selection()->clear();
        vp17->setFocus();
        const double beforeNudgeX = core17->findPath(actor17, 1.0, 3.0)->points.first().x;
        QTest::keyClick(vp17, Qt::Key_Left); // -10cm
        CHECK(near(core17->findPath(actor17, 1.0, 3.0)->points.first().x,
                   beforeNudgeX - 10.0, 0.01),
              "阶段12：方向键微调路径点（← -10cm）");

        // ---- Delete 删除路径点（视口焦点；对象级删除让步） ----
        QTest::keyClick(vp17, Qt::Key_Delete);
        {
            const PathData* path = core17->findPath(actor17, 1.0, 3.0);
            CHECK(path && path->points.isEmpty() && path->type == PathType::Arc,
                  "阶段12：Delete 删除选中路径点（弧线条目保留为单段弧）");
        }
        CHECK(core17->activePath().has_value()
                  && core17->activePath()->pointIndex == -1,
              "阶段12：删点后选中点清除（回退仅路径）");
        core17->undo();
        CHECK(core17->findPath(actor17, 1.0, 3.0)->points.size() == 1,
              "阶段12：undo 恢复被删路径点");

        // ---- 右键白线 → 恢复直线（PathData 移除） ----
        QTimer::singleShot(200, [] {
            if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
                // 守卫：只在确为路径菜单时回车（防误触发其它菜单动作）
                if (menu->actions().size() >= 2
                    && menu->actions().at(1)->text() == QStringLiteral("恢复直线")) {
                    menu->setActiveAction(menu->actions().at(1));
                    QTest::keyClick(menu, Qt::Key_Return);
                } else {
                    menu->close();
                }
            }
        });
        // 点取第二段弧的中点（避开端点处的演员——对象优先于白线——与路径点标记）
        const QList<QPointF> polyline17 = scene17->segmentPathPolyline();
        const QPointF lineHitScene = polyline17.at(polyline17.size() * 3 / 4);
        QContextMenuEvent resetEvt(QContextMenuEvent::Mouse, viewPos(lineHitScene),
                                   view17->mapToGlobal(viewPos(lineHitScene)));
        QApplication::sendEvent(vp17, &resetEvt);
        CHECK(core17->findPath(actor17, 1.0, 3.0) == nullptr,
              "阶段12：右键白线恢复直线（PathData 移除）");
        CHECK(scene17->segmentPathPolyline().size() == 2,
              "阶段12：恢复直线后白线退化为两点直线");

        // ---- 级联抽验（UI 层）：moveKeyframe 重绑定；删对象级联 + undo ----
        core17->setPathType(actor17, 1.0, 3.0, PathType::Arc);
        core17->moveKeyframe(actor17, 3.0, 4.0);
        CHECK(core17->findPath(actor17, 1.0, 4.0) != nullptr,
              "阶段12：级联 moveKeyframe 段标识跟随重绑定（UI 层）");
        core17->selection()->select(actor17);
        vp17->setFocus();
        QTest::keyClick(vp17, Qt::Key_Delete); // 对象级删除（无选中路径点）
        CHECK(runtime17->fileData().objects.isEmpty()
                  && runtime17->fileData().paths.isEmpty(),
              "阶段12：级联删除对象 → PathData 级联删除");
        CHECK(!core17->activePath().has_value(), "阶段12：对象删除后 activePath 防御清除");
        core17->undo();
        CHECK(runtime17->fileData().objects.size() == 1
                  && runtime17->fileData().paths.size() == 1,
              "阶段12：undo 恢复对象与路径（级联互逆）");

        // ---- Inspector 切回对象页：点视口对象 → activePath 清除 ----
        core17->setActiveSegment(SegmentRef { actor17, 1.0, 4.0 });
        core17->setActivePath(ActivePathRef { SegmentRef { actor17, 1.0, 4.0 }, -1 });
        CHECK(pathTypeCombo->isVisible(), "阶段12：设置 activePath 显示路径页");
        QTest::mouseClick(vp17, Qt::LeftButton, Qt::NoModifier,
                          viewPos(QPointF(100.0, 100.0))); // 点在演员甲上（首帧位姿）
        CHECK(!core17->activePath().has_value(), "阶段12：点选对象清除 activePath");
        CHECK(!pathTypeCombo->isVisible(), "阶段12：Inspector 切回对象页");

        // 截图存档人工复核（弧线 + 路径点标记 + 幽灵 + Inspector 路径页）
        // 注意顺序：加点是命令提交（会经画布重派生清除段），段与选中路径在其后设置
        core17->addPathPoint(actor17, 1.0, 4.0, QPointF(200.0, 70.0)); // 弧上加一点
        core17->setActiveSegment(SegmentRef { actor17, 1.0, 4.0 });
        core17->setActivePath(ActivePathRef { SegmentRef { actor17, 1.0, 4.0 }, 0 });
        view17->grab().save(
            QDir::temp().absoluteFilePath(QStringLiteral("stage12-path-arc.png")));
        w17.grab().save(
            QDir::temp().absoluteFilePath(QStringLiteral("stage12-path-inspector.png")));

        // 清理哨兵：还原吸附设置
        runtime17->settings().setGridSnapEnabled(true);
    }

    out() << Qt::endl;
    if (g_failures == 0) {
        out() << "全部交互用例通过" << Qt::endl;
        return 0;
    }
    out() << g_failures << " 个交互用例失败" << Qt::endl;
    return 1;
}
