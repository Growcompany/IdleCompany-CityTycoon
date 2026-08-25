// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "UI/PitchBoardText.h"
#include "PitchTileWidget.generated.h"

class UBorder;
class UCommonTextBlock;
class UImage;
class UMaterialInstanceDynamic;
class UTexture2D;

// 보드가 타일 1칸에 밀어 넣는 주입 묶음. Combo 는 추천 카드가 소비하는 필드라 타일은 표시하지 않는다.
USTRUCT()
struct FPitchTileData
{
	GENERATED_BODY()

	int32 ProjectIndex = 0;
	FText ProjectName;
	FText Combo;                 // "퍼즐 · 색채"
	TSoftObjectPtr<UTexture2D> Cover;
	FLinearColor GenreColor = FLinearColor::Gray;
	PitchBoardText::ETileVerdict Verdict = PitchBoardText::ETileVerdict::Short;
	FText VerdictWord;           // "출시 가능" / "재개발 · ★B" / "그래픽 부족" / "다음 티어"
	// 잠금 딤 위 해금 조건 문구. 판정 한 단어와 자리가 달라 따로 받는다(빈값 = VerdictWord 재사용)
	FText LockText;
	FString DevelopedGrade;      // 커버 우하단 Bungee 글자 (빈값 = 숨김)
	bool bTrend = false;
	bool bRecommended = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPitchTileClicked, int32, ProjectIndex);

/**
 * 기획 보드 4×3 그리드의 타일 1칸 (UIE_PitchTile).
 * 배경 = SDF 칩(MI_UI_ResChip*) — C++ 는 판정색 틴트와 Wpx/Hpx 만 주입하고 나머지 스타일은 WBP 소유.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UPitchTileWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	void Configure(const FPitchTileData& InData);

	// 블루 링 토글 — 보드가 선택 타일 1개만 켠다
	void SetSelected(bool bInSelected);

	int32 GetProjectIndex() const { return ProjectIndex; }

	UPROPERTY(BlueprintAssignable, Category = "PitchBoard")
	FOnPitchTileClicked OnTileClicked;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnClicked() override;

	// SDF 칩 배경/키라인 — 머티리얼이 Wpx/Hpx 를 요구해 위젯 크기를 MID 로 주입
	UPROPERTY(meta = (BindWidget))
	UImage* ChipBG;

	UPROPERTY(meta = (BindWidget))
	UImage* ChipLine;

	// 선택(블루) / 추천(골드) 링 — 기본 Collapsed
	UPROPERTY(meta = (BindWidget))
	UImage* SelectRing;

	UPROPERTY(meta = (BindWidget))
	UImage* RecommendRing;

	UPROPERTY(meta = (BindWidget))
	UImage* CoverImage;

	// 커버가 없는 프로젝트의 장르색 딥 플레이트 폴백
	UPROPERTY(meta = (BindWidget))
	UBorder* CoverFill;

	UPROPERTY(meta = (BindWidget))
	UImage* Img_Trend;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_Grade;

	UPROPERTY(meta = (BindWidget))
	UBorder* LockShade;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_Lock;

	UPROPERTY(meta = (BindWidget))
	UBorder* RecommendRibbon;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_Index;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_Name;

	UPROPERTY(meta = (BindWidget))
	UImage* Dot;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_Verdict;

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor TileTintPass = FLinearColor(0.047f, 0.185f, 0.076f, 0.16f);

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor TileTintShort = FLinearColor(0.72f, 0.26f, 0.24f, 0.13f);

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor TileTintNeutral = FLinearColor(1.f, 1.f, 1.f, 0.045f);

private:
	void EnsureChipMaterials();
	void ApplyVerdictVisual(PitchBoardText::ETileVerdict Verdict);
	void UpdateChipMaterialSize(const FGeometry& MyGeometry);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ChipBGMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ChipLineMID;

	// 링 2종도 Wpx/Hpx 를 요구하는 SDF 마스터다 — 남의 위젯 크기로 베이크된 MIC 를 그대로 쓰면 코너가 어긋난다
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SelectRingMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RecommendRingMID;

	// SDF 칩 머티리얼에 마지막으로 주입한 위젯 크기 (Zero = 미주입)
	FVector2D LastChipMatSize = FVector2D::ZeroVector;

	int32 ProjectIndex = 0;
};
