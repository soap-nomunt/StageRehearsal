#pragma once

// 角度制式换算（渲染层入口处的唯一换算点；不依赖 Qt Widgets，单测可直接包含）。
//
// 需求制式：0° = y 轴正方向（场景坐标 y 向下，即朝向舞台外边/屏幕下方），度数逆时针增大。
//           按验收约定：rotation=90 时朝向 −x（朝左）。
// Qt 制式：QGraphicsItem::setRotation 在 y 向下的场景坐标中为顺时针正值，0° 时局部 +y 同样指向场景 +y。
//
// 推导（场景坐标 x 向右、y 向下）：
//   需求角度 θ 的朝向单位向量 = (−sinθ, cosθ)；      // θ=0 → (0,1) 下，θ=90 → (−1,0) 左
//   Qt 旋转 φ（顺时针为正）后局部 +y 的朝向向量 = (−sinφ, cosφ)。
// 两者恒等 ⇒ φ = θ，即换算在数值上是恒等映射。
// 注意：需求所说的"逆时针增大"并非屏幕上的视觉逆时针（θ=90 朝左而非朝右），
// 散写换算极易在此处翻错符号，因此统一封装，业务/数据层永不接触 Qt 角度。

#include <QPointF>

#include <cmath>

namespace RenderUtil {

inline constexpr double kPi = 3.14159265358979323846;

// 需求角度（度）→ Qt setRotation 角度（度）。恒等映射，见上方推导。
inline double toQtRotation(double requirementDegrees)
{
    return requirementDegrees;
}

// 需求角度 θ（度）对应的朝向单位向量（场景坐标：x 向右、y 向下）。
// θ=0 → (0,+1) 朝下；θ=90 → (−1,0) 朝左；θ=180 → (0,−1) 朝上；θ=270 → (+1,0) 朝右。
inline QPointF directionVector(double requirementDegrees)
{
    const double rad = requirementDegrees * kPi / 180.0;
    return QPointF(-std::sin(rad), std::cos(rad));
}

// directionVector 的反向换算：由朝向向量（无需归一化）得需求角度，归一化到 [0,360)。
// 由 (−sinθ, cosθ) = dir/|dir| 得 θ = atan2(−x, y)。零向量无朝向，返回 0。
inline double rotationFromVector(const QPointF& dir)
{
    if (std::abs(dir.x()) < 1e-12 && std::abs(dir.y()) < 1e-12)
        return 0.0;
    double degrees = std::atan2(-dir.x(), dir.y()) * 180.0 / kPi;
    degrees = std::fmod(degrees, 360.0);
    if (degrees < 0.0)
        degrees += 360.0;
    return degrees;
}

} // namespace RenderUtil
