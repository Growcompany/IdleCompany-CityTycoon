// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Layout/Margin.h"
#include "Types/SlateEnums.h"
#include "Utils/FWidgetAnimationUtils.h"
#include "StatRowWidget.generated.h"

class UCommonTextBlock;
class UProgressBar;
class UPanelSlot;
class UImage;
class UTexture2D;
class UBorder;

/**
 * 스탯 정보를 표시하는 행 위젯
 * 블루프린트에서 디자인하고 C++에서 데이터만 설정
 * 에디터에서 DefaultStatName, DefaultStatValue로 기본값 설정 가능
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UStatRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 스탯 이름과 값을 설정하는 함수
	UFUNCTION(BlueprintCallable, Category = "Stat Row")
	void SetStatInfo(const FString& InStatName, const FString& InStatValue);

	// 스탯 이름만 설정
	UFUNCTION(BlueprintCallable, Category = "Stat Row")
	void SetStatName(const FString& InStatName);

	// 스탯 값만 설정
	UFUNCTION(BlueprintCallable, Category = "Stat Row")
	void SetStatValue(const FString& InStatValue);

	/**
	 * 현재 진행 상태 설정 (색상 자동 - 달성: 초록, 미달성: 빨강)
	 * @param CurrentValue 현재 점수
	 * @param TargetValue 목표 점수
	 */
	UFUNCTION(BlueprintCallable, Category = "Stat Row")
	void SetCurrentProgress(float CurrentValue, float TargetValue);

	/**
	 * 현재 진행 상태를 애니메이션과 함께 설정
	 * 숫자가 점진적으로 올라가고, 값 변경 시 스케일 펀치 효과 적용
	 * @param CurrentValue 현재 점수 (애니메이션 목표값)
	 * @param TargetValue 목표 점수
	 */
	UFUNCTION(BlueprintCallable, Category = "Stat Row")
	void SetCurrentProgressAnimated(float CurrentValue, float TargetValue);

	// 값 텍스트 색상 설정
	UFUNCTION(BlueprintCallable, Category = "Stat Row")
	void SetValueColor(const FSlateColor& InColor);

	// 이름 텍스트 색상 설정
	UFUNCTION(BlueprintCallable, Category = "Stat Row")
	void SetNameColor(const FSlateColor& InColor);

	// 폰트 크기 설정 (이름 + 값 동시)
	UFUNCTION(BlueprintCallable, Category = "Stat Row")
	void SetFontSize(int32 Size);

	// ProgressBar 위젯 반환 (ScoreOrb 타겟 좌표용)
	UProgressBar* GetProgressBarWidget() const { return ProgressBar; }

	// 색상 상수 — 외부(int64 등 자체 포맷 행)에서 동일 색 체계를 쓰도록 public
	static const FLinearColor ColorSuccess;   // 초록색
	static const FLinearColor ColorFail;      // 빨간색

	// 진행률에 따른 ProgressBar 색상 계산 (0~1: 빨강→주황→파랑, 1+: 초록)
	static FLinearColor GetProgressBarColor(float Percent);

	// 아이콘 텍스처 동적 변경. nullptr 이면 아이콘 Collapsed.
	UFUNCTION(BlueprintCallable, Category = "Stat Row|Icon")
	void SetStatIcon(UTexture2D* InIcon);

	// 행 플레이트 표시 토글 (미충족=표시). RowPlate 없는 변형에선 no-op
	UFUNCTION(BlueprintCallable, Category = "Stat Row")
	void SetRowPlateVisible(bool bVisible);

	// 아이콘 크기 동적 변경 (Brush.ImageSize)
	UFUNCTION(BlueprintCallable, Category = "Stat Row|Icon")
	void SetIconSize(FVector2D InSize);

	// 아이콘 슬롯 패딩 동적 변경 (HorizontalBox/VerticalBox/Overlay/Border 슬롯 지원)
	UFUNCTION(BlueprintCallable, Category = "Stat Row|Icon")
	void SetIconPadding(FMargin InPadding);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void SynchronizeProperties() override;

	// 에디터에서 설정 가능한 기본값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Row")
	FString DefaultStatName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Row")
	FString DefaultStatValue;

	// 폰트 크기 (0이면 WBP 기본값 그대로 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Row|Style", meta = (ClampMin = 0, ClampMax = 96))
	int32 FontSize = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Row|Style", meta = (InlineEditConditionToggle))
	bool bOverrideNameColor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Row|Style", meta = (EditCondition = "bOverrideNameColor"))
	FLinearColor NameColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Row|Style", meta = (InlineEditConditionToggle))
	bool bOverrideValueColor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Row|Style", meta = (EditCondition = "bOverrideValueColor"))
	FLinearColor ValueColor = FLinearColor::White;

	// 진행 텍스트(Current/통합)를 충족=초록/미충족=빨강으로 칠할지.
	// 바 fill이 이미 상태색(그린)인 변형(UIE_CondStatRow)에선 꺼서 텍스트를 WBP 색으로 고정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Row|Style")
	bool bColorizeProgressText = true;

	// ── 아이콘 (옵셔널) ──
	// 디자이너가 인스턴스마다 텍스처 설정. NativePreConstruct 에서 자동 적용.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Row|Icon")
	TSoftObjectPtr<UTexture2D> IconTexture;

	// 아이콘 표시 크기 (Brush.ImageSize). (0,0) 이면 WBP 기본값 그대로 사용.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Row|Icon")
	FVector2D IconSize = FVector2D(24.0f, 24.0f);

	// 아이콘 슬롯 패딩 (Right 값으로 텍스트와 간격 조절)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Row|Icon")
	FMargin IconPadding = FMargin(0.0f, 0.0f, 4.0f, 0.0f);

	// Separator(구분선) 표시 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Row|Separator")
	bool bShowSeparator = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Row|Separator", meta = (EditCondition = "bShowSeparator"))
	FMargin SeparatorPadding = FMargin(0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Row|Separator", meta = (EditCondition = "bShowSeparator"))
	TEnumAsByte<EHorizontalAlignment> SeparatorHAlign = HAlign_Fill;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Row|Separator", meta = (EditCondition = "bShowSeparator"))
	TEnumAsByte<EVerticalAlignment> SeparatorVAlign = VAlign_Bottom;

	// 블루프린트에서 바인딩할 위젯들
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* StatNameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* StatValueText;

	// 현재 점수 텍스트 (Optional - 스테이지 진행 표시용)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* CurrentStatValueText;

	// ProgressBar (Optional - 진행률 시각화)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UProgressBar* ProgressBar;

	// 구분선 위젯 (Optional - WBP 트리에서 UIE_Separator 이름으로 바인딩)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* UIE_Separator;

	// 아이콘 (Optional - 변형 WBP `UIE_StatRow_WithIcon` 에서 StatIcon 이름으로 바인딩)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* StatIcon;

	// 행 배경 플레이트 (Optional - 미충족 조건 행 부상용. HQ 패널 UIE_CondStatRow 변형에서 바인딩)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* RowPlate;

	// 에디터 설정값을 위젯에 적용
	void ApplyDefaultValues();

	// 아이콘 텍스처 + 사이즈 + 패딩을 StatIcon 에 적용 (StatIcon nullptr 시 no-op)
	void ApplyIconProperties();

	// Separator 슬롯의 Padding/HAlign/VAlign 을 리플렉션으로 적용 (Slot 종류 무관)
	void ApplySeparatorSlotProperties();

private:
	// 애니메이션 상태
	FNumberCountUpAnimation CountUpAnimation;
	FScalePunchAnimation ScalePunchAnimation;

	// 색상 판정용 캐시된 목표값
	float CachedTargetValue = 0.f;

	// 마지막으로 표시된 값 (값 변경 감지용)
	float LastDisplayedValue = 0.f;

	// 애니메이션 진행 중 텍스트 및 스케일 업데이트
	void UpdateAnimatedDisplay(float CurrentDisplayValue);

	// ProgressBar percent + 색상 업데이트
	void UpdateProgressBar(float CurrentValue, float TargetValue);
};
