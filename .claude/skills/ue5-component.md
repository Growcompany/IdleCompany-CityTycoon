---
name: UE5 컴포넌트 생성
description: ActorComponent 또는 SceneComponent 생성
---

## UE5 컴포넌트 생성 규칙

### 컴포넌트 종류 선택
- `UActorComponent`: 트랜스폼 없는 로직 전용 (데이터, 기능)
- `USceneComponent`: 트랜스폼 있는 컴포넌트 (위치, 회전 필요)

### 헤더 템플릿
```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "[컴포넌트명].generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class COMPANYGROWTH RENEWAL_API U[컴포넌트명] : public UActorComponent
{
    GENERATED_BODY()

public:
    U[컴포넌트명]();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // 블루프린트 호출 가능 함수
    UFUNCTION(BlueprintCallable, Category = "[카테고리]")
    void [함수명]();

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "[카테고리]", meta = (AllowPrivateAccess = "true"))
    // 프로퍼티
};
```

### Tick 비활성화 (성능 최적화)
```cpp
// 생성자에서
PrimaryComponentTick.bCanEverTick = false;
```
