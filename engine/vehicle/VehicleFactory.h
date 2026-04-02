#pragma once

#include "core/Math/Vector3.h"

namespace Moon {

class Scene;
class SceneNode;
class PhysicsSystem;

class VehicleFactory {
public:
    static SceneNode* CreateBuggy(Scene* scene, PhysicsSystem* physicsSystem, const Vector3& position);
};

} // namespace Moon
