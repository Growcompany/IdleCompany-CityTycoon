---
name: UE5 데이터테이블 구조체
description: DataTable용 FTableRowBase 구조체 생성
---

## UE5 데이터테이블 구조체 생성 규칙

### 파일 위치
`Source/CompanyGrowthRenewal/Public/Table/`

### 구조체 템플릿
```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "[구조체명].generated.h"

USTRUCT(BlueprintType)
struct COMPANYGROWTH RENEWAL_API F[구조체명] : public FTableRowBase
{
    GENERATED_BODY()

public:
    F[구조체명]()
        : [기본값 초기화]
    {}

    // ID (RowName으로도 사용 가능)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
    FName ID;

    // 이름
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
    FText DisplayName;

    // 설명
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
    FText Description;

    // 아이콘
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    TSoftObjectPtr<UTexture2D> Icon;

    // 수치 데이터
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    int32 Value;
};
```

### DataTable 로드 방법
```cpp
// 생성자에서 로드
static ConstructorHelpers::FObjectFinder<UDataTable> DataTableFinder(TEXT("/Game/Data/DT_[테이블명]"));
if (DataTableFinder.Succeeded())
{
    DataTable = DataTableFinder.Object;
}

// 런타임에서 행 가져오기
if (F[구조체명]* Row = DataTable->FindRow<F[구조체명]>(RowName, TEXT("")))
{
    // Row 사용
}
```
