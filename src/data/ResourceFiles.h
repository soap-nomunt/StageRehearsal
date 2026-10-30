#pragma once

// 项目资源目录文件辅助（阶段14，数据层）：只负责路径推导与文件复制，不依赖
// AppSettings，不做任何清单/命令修改。
//
// 约定：存档 demo.json 的资源根目录为同级的 demo.resources/，内部按类型分
// audio/、images/ 两个子目录；ResourceData::relativePath 相对该根目录。
// 导入时按资源 ID 生成目录内文件名（保留扩展名），文件已存在则追加序号，
// 绝不覆盖——删除资源/撤销导入不删除物理文件，覆盖会破坏撤销历史。

#include "DataTypes.h"

#include <QString>

namespace ResourceFiles {

// 存档对应的资源根目录绝对路径；archivePath 为空返回空串
QString resourceRootDir(const QString& archivePath);

// 类型子目录名（audio / images）
QString typeSubdir(ResourceType type);

// 相对路径安全校验：非空、非绝对路径、无盘符、无 .. 段、规范化为 "/" 分隔
bool isSafeRelativePath(const QString& relativePath);

// 将相对路径解析为资源根目录内的绝对路径；不安全/为空返回空串
QString resolveResourcePath(const QString& archivePath, const QString& relativePath);

// 复制源文件进资源目录。成功时 relativePath 输出相对资源根目录的路径；
// 失败返回 false 并写 error（调用方不得为失败项提交资源清单）。
bool importResourceFile(const QString& archivePath, ResourceType type,
                        const QString& resourceId, const QString& sourcePath,
                        QString& relativePath, QString& error);

// 另存为：完整复制整个资源根目录（含暂未被清单引用的文件），保证撤销历史中
// 的资源仍可恢复。源目录不存在视为成功（新项目尚无资源）；源/目标根相同跳过。
bool copyResourceTree(const QString& sourceArchivePath, const QString& targetArchivePath,
                      QString& error);

} // namespace ResourceFiles
