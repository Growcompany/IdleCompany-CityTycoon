---
name: UE5 서브시스템 생성
description: GameInstance/World/LocalPlayer 서브시스템 생성
---

## UE5 서브시스템 생성 규칙

### 서브시스템 종류 선택
| 종류 | 수명 | 용도 |
|------|------|------|
| `UGameInstanceSubsystem` | 게임 전체 | 전역 매니저 (사운드, 설정) |
| `UWorldSubsystem` | 월드/레벨 | 레벨별 시스템 |
| `ULocalPlayerSubsystem` | 로컬 플레이어 | 플레이어별 데이터 |

### GameInstanceSubsystem 템플릿
```cpp
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "[서브시스템명].generated.h"

UCLASS()
class COMPANYGROWTH RENEWAL_API U[서브시스템명] : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // 초기화
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    // 정리
    virtual void Deinitialize() override;
    // 서브시스템 사용 가능 여부
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

    // 접근 헬퍼 (선택)
    static U[서브시스템명]* Get(const UObject* WorldContextObject);
};
```

### 접근 방법
```cpp
// 어디서든 접근
if (UGameInstance* GI = GetGameInstance())
{
    U[서브시스템명]* Subsystem = GI->GetSubsystem<U[서브시스템명]>();
}
```

### 프로젝트 기존 예시
- `SoundManagerSubsystem` 참고
