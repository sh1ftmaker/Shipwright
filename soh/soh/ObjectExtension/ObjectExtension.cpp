#include "ObjectExtension.h"
#include <compat.h>

ObjectExtension& ObjectExtension::GetInstance() {
    static ObjectExtension instance;
    return instance;
}

ObjectExtension::Id ObjectExtension::RegisterId() {
    return NextId++;
}

void ObjectExtension::Free(const void* object) {
    if (object == nullptr) {
        return;
    }

    for (auto iter = Data.begin(); iter != Data.end();) {
        if (iter->first.first == object) {
            iter = Data.erase(iter);
        } else {
            ++iter;
        }
    }
}

extern "C" void ObjectExtension_Free(const void* object) {
    ObjectExtension::GetInstance().Free(object);
}
