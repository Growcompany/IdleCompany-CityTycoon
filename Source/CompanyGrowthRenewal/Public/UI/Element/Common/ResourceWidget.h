// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Enum/ResourceType.h"
#include "Global/GlobalUtilFunctions.h"
#include "Kismet/BlueprintPlatformLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Utils/FWidgetAnimationUtils.h"
#include "ResourceWidget.generated.h"

class UImage;
class UCommonTextBlock;
class USizeBox;
class UHorizontalBox;
class UOverlay;
class UButton;
class UItemTooltipWidget;
class UTexture2D;
/**
 *
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UResourceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UResourceWidget(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	EResourceType ResourceType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	int64 ResourceAmount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	int64 ResourceMaxAmount = -1;

	// 숫자 축약 끝자리 처리. 보유/수치 = Floor(기본), 비용 표시 위젯 = Ceil 권장.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	ENumberRoundMode AmountRoundMode = ENumberRoundMode::Floor;

	// 분수 형식 표시 모드 (예: 3/5)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	bool bShowAsFraction = false;

	// 실시간 보유량 카운터 선언 — 목돈 유입 시 롤업 + 자릿수 승격 연출 대상. 비용 표시 칩은 false 유지(선택 바뀔 때마다 굴러가면 안 됨).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	bool bLiveCounterRollup = false;

	// 아이콘 위치 계산이 필요한지 여부 (HUD용만 true)
	bool bShouldCalculateIconPos = false;

	// 레이아웃 설정 (EditAnywhere로 각 인스턴스마다 다르게 설정 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	float IconSize = 64.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	float IconPaddingLeft = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	float IconPaddingTop = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	float IconPaddingRight = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	float IconPaddingBottom = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	float TextPaddingLeft = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	int32 FontSize = 36;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	FLinearColor TextColor = FLinearColor::White;

	// 텍스트 슬롯 Fill (true: 공간 채우고 오른쪽 정렬 가능, false: Auto 크기)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	bool bTextSlotFill = false;

	// 텍스트 수평 정렬 (bTextSlotFill=true일 때 유효)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (EditCondition = "bTextSlotFill"))
	TEnumAsByte<EHorizontalAlignment> TextHorizontalAlignment = HAlign_Left;

	void SetAmountText() const;
	void SetImage();

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* ResourceImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* ResourceAmountText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	USizeBox* SizeBox_161;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UHorizontalBox* HorizontalBox_26;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UOverlay* Overlay_0;

	// 캡슐 위 투명 버튼 — 클릭 시 재화 상세 툴팁(이름/설명/정확수치). WBP 에 없으면 무시(옵셔널).
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButton* InfoButton;

	// SDF 칩 배경/키라인 (Hud 변형 전용) — 머티리얼이 Wpx/Hpx 파라미터를 요구해 위젯 크기를 MID 로 주입
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* ChipBG = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* ChipLine = nullptr;

	UFUNCTION()
	void HandleInfoButtonClicked();

	UPROPERTY(Transient)
	TObjectPtr<UItemTooltipWidget> ActiveTooltip;

	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void ApplyLayoutSettings();
	void UpdateTextColor();

private:
	friend class FCGRResourceWidgetFixedShellFeedbackTest;

	// SDF 칩 머티리얼에 마지막으로 주입한 위젯 크기 (Zero = 미주입)
	FVector2D LastChipMatSize = FVector2D::ZeroVector;

	void UpdateChipMaterialSize(const FGeometry& MyGeometry);

	// 아이콘 위치 계산 완료 여부
	bool bIconPosCalculated = false;

	// 타입별 색상 사용 여부
	bool bUseTypeColor = true;

	// 구매 가능 여부 (false면 빨간색으로 표시)
	bool bCanAfford = true;

	// 피드백 애니 상태 (<0 = 비활성). NativeTick 의 UpdateFeedbackAnim 이 렌더 트랜스폼으로 구동.
	float BumpElapsed = -1.f;
	float BumpPeak = 0.08f;
	float ShakeElapsed = -1.f;

	// 자릿수 승격 마일스톤 채널 — 숫자 래퍼 펀치는 Bump가, 키라인 플래시는 이 타이머가 담당한다. 루트 렌더 트랜스폼은 Shake만 소유한다.
	float MilestoneElapsed = -1.f;
	bool bFeedbackXformApplied = false;

	// ChipLine 원본 틴트 (플래시 복원용). WBP 가 흰색이 아닐 수도 있어 최초 1회 캐시.
	FLinearColor ChipLineBaseColor = FLinearColor::White;
	bool bChipLineBaseCached = false;

	// 재화별 최고 자릿수 티어. INDEX_NONE = 미시딩(최초 표시값으로 시딩, 그때는 발화 없음).
	// 세이브 영속화는 SaveLoadManager 소관 — 현재는 위젯 수명 내 메모리 폴백.
	int32 PeakDigitTier = INDEX_NONE;
	double LastMilestoneSoundTime = -100.0;

	void UpdateFeedbackAnim(float DeltaTime);

	static int32 GetDigitTier(int64 Value);
	static float ComputeRollupDuration(int64 Delta);
	bool ShouldRollup(int64 NewAmount) const;
	void EvaluateDigitMilestone(int64 DisplayedValue);
	void PlayMilestoneFeedback(int32 NewTier);
	void ApplyChipLineFlash(float Intensity);

	// 카운트업 롤 — 알파(0→1)만 애니(float 정밀도), 실값은 int64 From/To 를 double 보간 (거액 자릿수 보존)
	FNumberCountUpAnimation CountUpAnim;
	int64 CountFrom = 0;
	int64 CountTo = 0;

public:
	void SetResourceType(EResourceType type, bool bUseTypeColor = true);

	// bLiveCounterRollup 칩에 한해 목돈(델타 >= 1000 && >= 표시값 2%)이면 자동으로 롤업 경로, 그 외는 즉시 대입.
	void SetValue(int64 amount);

	// 카운트업 롤 — 현재 표시값에서 amount 로 Duration 동안 굴림 (드립 착지 "따라락"). 재생 중 재호출은 목표값만 갱신.
	void SetValueAnimated(int64 amount, float Duration = 0.45f);
	void SetValueWithMax(int64 current, int64 max);
	void SetCanAfford(bool bInCanAfford);

	// 재화가 아닌 아이템(뽑기권 등) 표시 — 임의 아이콘+개수. ResourceType 기반 아이콘/타입색을 끈다.
	void SetItemDisplay(UTexture2D* IconTexture, int64 Count);

	// 트랜지언트 피드백 — 값 증가 펀치(PlayBump) / 자금부족 좌우 셰이크(PlayAffordShake). NativeTick 렌더 트랜스폼 구동.
	// InPeak = 스케일 진폭 (0.08 = 1.0→1.08). 고빈도(1초 드립) 호출은 0.05 권장.
	void PlayBump(float InPeak = 0.08f);
	void PlayAffordShake();

	FVector2D GetCachedIconScreenPos() const { return CachedIconScreenPos; }

public:
	FVector2D CachedIconScreenPos;

	UFUNCTION(BlueprintCallable)
	bool CalculateIconScreenPos();  // 성공 여부 반환

	// 위치 재계산 강제 (화면 회전 등)
	void ForceRecalculateIconPos() { bIconPosCalculated = false; }
};

