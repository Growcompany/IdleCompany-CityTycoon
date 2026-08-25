---
name: UE5 클래스 생성
description: UE5 C++ 클래스 템플릿 생성
---

## UE5 C++ 클래스 생성 규칙

### 파일 위치
- 헤더: `Source/CompanyGrowthRenewal/Public/[폴더]/[클래스명].h`
- 구현: `Source/CompanyGrowthRenewal/Private/[폴더]/[클래스명].cpp`

### 헤더 파일 템플릿
```cpp
#pragma once

#include "CoreMinimal.h"
#include "[부모클래스].h"
#include "[클래스명].generated.h"

UCLASS()
class COMPANYGROWTH RENEWAL_API [U/A][클래스명] : public [부모클래스]
{
    GENERATED_BODY()

public:
    [U/A][클래스명]();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY()
    // 프로퍼티
};
```

### 네이밍 규칙
- `UObject` 상속 → `U` 접두사
- `AActor` 상속 → `A` 접두사
- 구조체 → `F` 접두사
- Enum → `E` 접두사
- 인터페이스 → `I` 접두사
