#include "InputTypeManager.h"

UInputTypeManager::UInputTypeManager()
    : CurrentInputType(EInputType::Unknown) // �ʱⰪ ����
{
}

EInputType UInputTypeManager::GetPlatformInputType()
{
#if PLATFORM_ANDROID || PLATFORM_IOS
    return EInputType::Touch;
#else
    return EInputType::KeyMouse;
#endif
}

