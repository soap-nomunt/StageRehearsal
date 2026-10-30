cmake_minimum_required(VERSION 3.20)

project(StageRehearsal VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Test Multimedia)

qt_standard_project_setup()

qt_add_executable(StageRehearsal
    src/main.cpp
    src/data/DataTypes.h
    src/data/JsonStore.h
    src/data/JsonStore.cpp
    src/data/AppSettings.h
    src/data/AppSettings.cpp
    src/data/RuntimeData.h
    src/data/RuntimeData.cpp
    src/data/ResourceFiles.h
    src/data/ResourceFiles.cpp
    src/business/EditorCore.h
    src/business/EditorCore.cpp
    src/business/Commands.h
    src/business/Commands.cpp
    src/business/UndoStack.h
    src/business/UndoStack.cpp
    src/business/SelectionModel.h
    src/business/SelectionModel.cpp
    src/business/TimelinePlayer.h
    src/business/TimelinePlayer.cpp
    src/business/KeyframeCommands.h
    src/business/KeyframeCommands.cpp
    src/business/PathGeometry.h
    src/business/PathCommands.h
    src/business/PathCommands.cpp
    src/business/ResourceCommands.h
    src/business/ResourceCommands.cpp
    src/business/ResourcePreviewPlayer.h
    src/business/ResourcePreviewPlayer.cpp
    src/render/MainWindow.h
    src/render/MainWindow.cpp
    src/render/RenderUtil.h
    src/render/KeyframePrompt.h
    src/render/StageView.h
    src/render/StageView.cpp
    src/render/StageScene.h
    src/render/StageScene.cpp
    src/render/ActorItem.h
    src/render/ActorItem.cpp
    src/render/ScreenItem.h
    src/render/ScreenItem.cpp
    src/render/PropItem.h
    src/render/PropItem.cpp
    src/render/StageObjectItem.h
    src/render/StageObjectItem.cpp
    src/render/HierarchyPanel.h
    src/render/HierarchyPanel.cpp
    src/render/InspectorPanel.h
    src/render/InspectorPanel.cpp
    src/render/ResourceWindow.h
    src/render/ResourceWindow.cpp
    src/render/dialogs/CreateObjectDialog.h
    src/render/dialogs/CreateObjectDialog.cpp
    src/render/dialogs/ShapeEditDialog.h
    src/render/dialogs/ShapeEditDialog.cpp
    src/render/SettingsWindow.h
    src/render/SettingsWindow.cpp
    src/render/PrepAreaCanvas.h
    src/render/PrepAreaCanvas.cpp
    src/render/StartDialog.h
    src/render/StartDialog.cpp
    src/render/TimelineWidget.h
    src/render/TimelineWidget.cpp
)

# 阶段2：示例存档在 configure 时拷到 exe 输出目录（VS 多配置生成器按配置分子目录，
# 额外拷一份到构建根目录以兼容单配置生成器）
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/examples/stage-demo.json"
     DESTINATION "${CMAKE_BINARY_DIR}")
foreach(build_type Debug Release RelWithDebInfo MinSizeRel)
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/examples/stage-demo.json"
         DESTINATION "${CMAKE_BINARY_DIR}/${build_type}")
endforeach()

target_include_directories(StageRehearsal PRIVATE src)

target_link_libraries(StageRehearsal PRIVATE Qt6::Widgets Qt6::Multimedia)

# 数据层自测（阶段1 验收用，独立目标）
# 阶段3 起同时覆盖业务层命令/选择模型/EditorCore（均不依赖 Widgets）
# 阶段5 追加 TimelinePlayer 插值与关键帧命令（仍不依赖 Widgets）
add_executable(DataLayerSelfTest
    tests/DataLayerSelfTest.cpp
    src/data/AppSettings.cpp
    src/data/JsonStore.cpp
    src/data/RuntimeData.cpp
    src/data/ResourceFiles.cpp
    src/business/Commands.cpp
    src/business/UndoStack.cpp
    src/business/EditorCore.cpp
    src/business/SelectionModel.cpp
    src/business/TimelinePlayer.cpp
    src/business/KeyframeCommands.cpp
    src/business/PathCommands.cpp
    src/business/ResourceCommands.cpp
    src/business/ResourcePreviewPlayer.cpp
)

target_include_directories(DataLayerSelfTest PRIVATE src)

target_link_libraries(DataLayerSelfTest PRIVATE Qt6::Core Qt6::Gui Qt6::Multimedia)

# 交互自测（阶段3 验收辅助，独立目标）：QTest 合成事件驱动真实 MainWindow，
# 覆盖需求 11~14 条的视口/面板交互路径。运行：QT_QPA_PLATFORM=offscreen
add_executable(InteractionSelfTest
    tests/InteractionSelfTest.cpp
    src/data/AppSettings.cpp
    src/data/JsonStore.cpp
    src/data/RuntimeData.cpp
    src/data/ResourceFiles.cpp
    src/business/Commands.cpp
    src/business/UndoStack.cpp
    src/business/EditorCore.cpp
    src/business/SelectionModel.cpp
    src/business/TimelinePlayer.cpp
    src/business/KeyframeCommands.cpp
    src/business/PathCommands.cpp
    src/business/ResourceCommands.cpp
    src/business/ResourcePreviewPlayer.cpp
    src/render/MainWindow.cpp
    src/render/StageScene.cpp
    src/render/StageView.cpp
    src/render/ActorItem.cpp
    src/render/ScreenItem.cpp
    src/render/PropItem.cpp
    src/render/StageObjectItem.cpp
    src/render/HierarchyPanel.cpp
    src/render/InspectorPanel.cpp
    src/render/ResourceWindow.cpp
    src/render/TimelineWidget.cpp
    src/render/dialogs/CreateObjectDialog.cpp
    src/render/dialogs/ShapeEditDialog.cpp
    src/render/SettingsWindow.cpp
    src/render/PrepAreaCanvas.cpp
    src/render/StartDialog.cpp
)

target_include_directories(InteractionSelfTest PRIVATE src)

target_link_libraries(InteractionSelfTest PRIVATE Qt6::Widgets Qt6::Test Qt6::GuiPrivate Qt6::Multimedia)
