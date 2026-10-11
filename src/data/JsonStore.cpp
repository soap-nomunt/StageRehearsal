#include "JsonStore.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace {

// ---- 序列化辅助 ----

QJsonObject transformToJson(const Transform& t)
{
    QJsonObject obj;
    obj[QStringLiteral("x")] = t.x;
    obj[QStringLiteral("y")] = t.y;
    obj[QStringLiteral("rotation")] = t.rotation;
    return obj;
}

QJsonObject keyframeToJson(const Keyframe& k)
{
    QJsonObject obj;
    obj[QStringLiteral("t")] = k.t;
    obj[QStringLiteral("x")] = k.x;
    obj[QStringLiteral("y")] = k.y;
    obj[QStringLiteral("rotation")] = k.rotation;
    return obj;
}

QJsonObject objectToJson(const ObjectData& o)
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = o.id;
    obj[QStringLiteral("type")] = objectTypeToString(o.type);
    obj[QStringLiteral("name")] = o.name;
    obj[QStringLiteral("parent")] = o.parentId; // 始终写出；空串 = 顶层
    obj[QStringLiteral("color")] = o.color.isValid() ? o.color.name(QColor::HexRgb) : QString();
    obj[QStringLiteral("transform")] = transformToJson(o.transform);
    if (o.size) {
        QJsonObject size;
        size[QStringLiteral("w")] = o.size->width();
        size[QStringLiteral("h")] = o.size->height();
        obj[QStringLiteral("size")] = size;
    }
    QJsonArray keyframes;
    for (const Keyframe& k : o.keyframes)
        keyframes.append(keyframeToJson(k));
    obj[QStringLiteral("keyframes")] = keyframes;
    return obj;
}

// ---- 解析辅助（全部先查类型再取值，任何畸形输入都不崩溃） ----

LoadResult invalid(const QString& message)
{
    return { LoadError::InvalidData, message, {} };
}

bool readDouble(const QJsonObject& obj, const QString& key, double& out)
{
    const QJsonValue v = obj.value(key);
    if (!v.isDouble())
        return false;
    out = v.toDouble();
    return true;
}

bool readString(const QJsonObject& obj, const QString& key, QString& out)
{
    const QJsonValue v = obj.value(key);
    if (!v.isString())
        return false;
    out = v.toString();
    return true;
}

bool parseTransform(const QJsonValue& value, Transform& out)
{
    if (!value.isObject())
        return false;
    const QJsonObject obj = value.toObject();
    return readDouble(obj, QStringLiteral("x"), out.x)
        && readDouble(obj, QStringLiteral("y"), out.y)
        && readDouble(obj, QStringLiteral("rotation"), out.rotation);
}

bool parseKeyframe(const QJsonValue& value, Keyframe& out)
{
    if (!value.isObject())
        return false;
    const QJsonObject obj = value.toObject();
    return readDouble(obj, QStringLiteral("t"), out.t)
        && readDouble(obj, QStringLiteral("x"), out.x)
        && readDouble(obj, QStringLiteral("y"), out.y)
        && readDouble(obj, QStringLiteral("rotation"), out.rotation);
}

bool parseObject(const QJsonValue& value, ObjectData& out, QString& error)
{
    if (!value.isObject()) {
        error = QStringLiteral("objects 数组元素不是对象");
        return false;
    }
    const QJsonObject obj = value.toObject();

    QString typeStr;
    if (!readString(obj, QStringLiteral("id"), out.id)
        || !readString(obj, QStringLiteral("type"), typeStr)
        || !readString(obj, QStringLiteral("name"), out.name)) {
        error = QStringLiteral("对象缺少 id/type/name 字段");
        return false;
    }
    if (!objectTypeFromString(typeStr, out.type)) {
        error = QStringLiteral("未知对象类型：%1").arg(typeStr);
        return false;
    }

    // parent 缺省视为顶层（旧存档兼容）；存在则必须是字符串
    const QJsonValue parentValue = obj.value(QStringLiteral("parent"));
    if (parentValue.isString())
        out.parentId = parentValue.toString();
    else if (!parentValue.isUndefined()) {
        error = QStringLiteral("对象 %1 的 parent 字段非法").arg(out.id);
        return false;
    }

    if (!parseTransform(obj.value(QStringLiteral("transform")), out.transform)) {
        error = QStringLiteral("对象 %1 的 transform 缺失或非法").arg(out.id);
        return false;
    }

    const QJsonValue colorValue = obj.value(QStringLiteral("color"));
    if (colorValue.isString())
        out.color = QColor(colorValue.toString()); // 非法颜色保持 invalid，宽松处理

    const QJsonValue sizeValue = obj.value(QStringLiteral("size"));
    if (!sizeValue.isUndefined()) {
        if (!sizeValue.isObject()) {
            error = QStringLiteral("对象 %1 的 size 非法").arg(out.id);
            return false;
        }
        const QJsonObject size = sizeValue.toObject();
        double w = 0.0, h = 0.0;
        if (!readDouble(size, QStringLiteral("w"), w) || !readDouble(size, QStringLiteral("h"), h)) {
            error = QStringLiteral("对象 %1 的 size 缺少 w/h").arg(out.id);
            return false;
        }
        out.size = QSizeF(w, h);
    }

    const QJsonValue keyframesValue = obj.value(QStringLiteral("keyframes"));
    if (!keyframesValue.isUndefined()) {
        if (!keyframesValue.isArray()) {
            error = QStringLiteral("对象 %1 的 keyframes 不是数组").arg(out.id);
            return false;
        }
        for (const QJsonValue& kfValue : keyframesValue.toArray()) {
            Keyframe kf;
            if (!parseKeyframe(kfValue, kf)) {
                error = QStringLiteral("对象 %1 存在非法关键帧").arg(out.id);
                return false;
            }
            out.keyframes.append(kf);
        }
    }
    return true;
}

bool parseTimeline(const QJsonValue& value, TimelineData& out)
{
    if (!value.isObject())
        return false;
    const QJsonObject obj = value.toObject();
    if (!readDouble(obj, QStringLiteral("duration"), out.duration))
        return false;

    const QJsonValue labelsValue = obj.value(QStringLiteral("labels"));
    if (labelsValue.isUndefined())
        return true;
    if (!labelsValue.isArray())
        return false;
    for (const QJsonValue& labelValue : labelsValue.toArray()) {
        if (!labelValue.isObject())
            return false;
        const QJsonObject labelObj = labelValue.toObject();
        SceneLabel label;
        if (!readDouble(labelObj, QStringLiteral("t"), label.t)
            || !readString(labelObj, QStringLiteral("name"), label.name))
            return false;
        out.labels.append(label);
    }
    return true;
}

} // namespace

QJsonObject JsonStore::toJson(const FileData& data)
{
    QJsonObject root;
    root[QStringLiteral("version")] = data.version;

    QJsonObject stage;
    stage[QStringLiteral("width")] = data.stage.width;
    stage[QStringLiteral("depth")] = data.stage.depth;
    stage[QStringLiteral("centerOffset")] = data.stage.centerOffset;
    root[QStringLiteral("stage")] = stage;

    QJsonArray objects;
    for (const ObjectData& o : data.objects)
        objects.append(objectToJson(o));
    root[QStringLiteral("objects")] = objects;

    QJsonObject timeline;
    timeline[QStringLiteral("duration")] = data.timeline.duration;
    QJsonArray labels;
    for (const SceneLabel& label : data.timeline.labels) {
        QJsonObject labelObj;
        labelObj[QStringLiteral("t")] = label.t;
        labelObj[QStringLiteral("name")] = label.name;
        labels.append(labelObj);
    }
    timeline[QStringLiteral("labels")] = labels;
    root[QStringLiteral("timeline")] = timeline;

    return root;
}

LoadResult JsonStore::fromJson(const QJsonObject& root)
{
    const QJsonValue versionValue = root.value(QStringLiteral("version"));
    if (!versionValue.isDouble() || versionValue.toDouble() != static_cast<double>(currentVersion)) {
        return { LoadError::UnsupportedVersion,
                 QStringLiteral("存档版本缺失或不受支持（当前支持 version %1）").arg(currentVersion),
                 {} };
    }

    FileData data;
    data.version = currentVersion;

    const QJsonValue stageValue = root.value(QStringLiteral("stage"));
    if (!stageValue.isObject())
        return invalid(QStringLiteral("缺少 stage 节点或类型不符"));
    {
        const QJsonObject stage = stageValue.toObject();
        if (!readDouble(stage, QStringLiteral("width"), data.stage.width)
            || !readDouble(stage, QStringLiteral("depth"), data.stage.depth)
            || !readDouble(stage, QStringLiteral("centerOffset"), data.stage.centerOffset))
            return invalid(QStringLiteral("stage 节点缺少 width/depth/centerOffset"));
    }

    const QJsonValue objectsValue = root.value(QStringLiteral("objects"));
    if (!objectsValue.isArray())
        return invalid(QStringLiteral("缺少 objects 数组或类型不符"));
    for (const QJsonValue& entry : objectsValue.toArray()) {
        ObjectData obj;
        QString error;
        if (!parseObject(entry, obj, error))
            return invalid(error);
        data.objects.append(obj);
    }

    const QJsonValue timelineValue = root.value(QStringLiteral("timeline"));
    if (!timelineValue.isObject())
        return invalid(QStringLiteral("缺少 timeline 节点或类型不符"));
    if (!parseTimeline(timelineValue, data.timeline))
        return invalid(QStringLiteral("timeline 节点结构非法"));

    return { LoadError::None, QString(), data };
}

LoadResult JsonStore::load(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return { LoadError::Io, QStringLiteral("无法打开文件：%1").arg(file.errorString()), {} };

    const QByteArray raw = file.readAll();
    if (file.error() != QFileDevice::NoError)
        return { LoadError::Io, QStringLiteral("读取文件失败：%1").arg(file.errorString()), {} };

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return { LoadError::Parse, QStringLiteral("JSON 解析失败：%1").arg(parseError.errorString()), {} };
    if (!doc.isObject())
        return { LoadError::Parse, QStringLiteral("存档根节点不是 JSON 对象"), {} };

    return fromJson(doc.object());
}

SaveResult JsonStore::save(const QString& path, const FileData& data)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return { false, QStringLiteral("无法写入文件：%1").arg(file.errorString()) };

    const QJsonDocument doc(toJson(data));
    if (file.write(doc.toJson(QJsonDocument::Indented)) < 0)
        return { false, QStringLiteral("写入文件失败：%1").arg(file.errorString()) };

    return {};
}
