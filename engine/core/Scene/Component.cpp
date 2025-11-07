#include "Component.h"
#include "SceneNode.h"

namespace Moon {

Component::Component(SceneNode* owner)
    : m_owner(owner)
    , m_enabled(true)
{
}

void Component::SetEnabled(bool enabled) {
    if (m_enabled == enabled) {
        return;
    }
    
    m_enabled = enabled;
    
    if (m_enabled) {
        OnEnable();
    } else {
        OnDisable();
    }
}

} // namespace Moon
