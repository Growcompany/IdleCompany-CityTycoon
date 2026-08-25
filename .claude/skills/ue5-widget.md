---
name: UE5 CommonUI 위젯
description: CommonUI 기반 위젯 생성
---

## UE5 CommonUI 위젯 생성 규칙

### 파일 위치
`Source/CompanyGrowthRenewal/Public/UI/[폴더]/`

### CommonActivatableWidget 템플릿
```cpp
#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "[위젯명].generated.h"

UCLASS()
class COMPANYGROWTH RENEWAL_API U[위젯명] : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    U[위젯명](const FObjectInitializer& ObjectInitializer);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    // 활성화/비활성화 콜백
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;

    // UI 요소 바인딩
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<class UCommonTextBlock> TitleText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<class UCommonButtonBase> CloseButton;

private:
    UFUNCTION()
    void OnCloseButtonClicked();
};
```

### 위젯 스택에 추가
```cpp
// HUD나 다른 위젯에서
if (UCommonActivatableWidgetStack* Stack = GetWidgetStack())
{
    Stack->AddWidget<U[위젯명]>();
}
```

### UI 폴더 구조
```
Public/UI/
├── Element/   # 기본 UI 요소 (버튼, 텍스트 등)
├── HUD/       # HUD 위젯
├── Interface/ # UI 인터페이스
└── Panel/     # 패널/팝업 위젯
```
