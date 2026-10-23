// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/SlateWrapperTypes.h"
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

// 금액 표기 상세도. FitToWidth 는 칸에 들어가는 최대 자릿수까지 펼치고(1억 3442만 2342),
// 좁으면 Abbrev 와 같은 축약(1.34억)으로 내려온다 — 폭이 고정된 칩에서만 켤 것.
UENUM(BlueprintType)
enum class EAmountDetail : uint8
{
	Abbrev     UMETA(DisplayName = "Abbreviated (1.34eok)"),
	FitToWidth UMETA(DisplayName = "Detailed, fit to width")
};

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

	// 구매 불가일 때 숫자 색 (linear) ― 다크 판 위 칩은 인스턴스에서 부드러운 빨강으로 바꾼다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	FLinearColor CannotAffordColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);

	// 텍스트 슬롯 Fill (true: 공간 채우고 오른쪽 정렬 가능, false: Auto 크기)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	bool bTextSlotFill = false;

	// 텍스트 수평 정렬 (bTextSlotFill=true일 때 유효)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (EditCondition = "bTextSlotFill"))
	TEnumAsByte<EHorizontalAlignment> TextHorizontalAlignment = HAlign_Left;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	EAmountDetail AmountDetail = EAmountDetail::Abbrev;

	// FitToWidth 가 숫자 오른쪽에 남겨둘 여백 — 칩 패딩만큼 빼야 숫자가 벽에 붙지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout",
		meta = (EditCondition = "AmountDetail == EAmountDetail::FitToWidth"))
	float AmountTextRightInset = 12.0f;

	void SetAmountText() const;

	// 숫자가 쓸 수 있는 가로폭. 아이콘 칼럼 기준으로 재므로 숫자 길이에 되먹임되지 않는다.
	float GetAmountTextBudget() const;
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
	friend struct FCGRResourceWidgetSpendTestHelper;

	// SDF 칩 머티리얼에 마지막으로 주입한 위젯 크기 (Zero = 미주입)
	FVector2D LastChipMatSize = FVector2D::ZeroVector;

	// 마지막으로 상세도를 고른 가용폭. 레이아웃이 확정되는 첫 프레임과 리사이즈에만 다시 고르게 한다.
	float LastAmountBudget = -1.f;

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

	// 자릿수 승격 마일스톤 채널 — 숫자 래퍼 펀치는 Bump가, 키라인 플래시는 이 타이머가 담당한다.
	float MilestoneElapsed = -1.f;
	bool bFeedbackXformApplied = false;

	// 지출 눌림 채널. Bump 는 숫자 래퍼를 1.0 위로 부풀리고 이건 칩 루트를 아래로만 눌러 방향이 반대라 분리했다.
	// 루트 렌더 트랜스폼은 Shake(이동)와 이 채널(스케일)이 함께 쓴다 — 둘은 동시에 안 난다(실패 vs 성공).
	static constexpr float SpendRollDuration = 0.25f;   // 획득 롤업(0.35~0.6)보다 짧게 끊는다
	static constexpr float SpendDipScale = 0.94f;       // 1.0 초과 금지 — 넘으면 클립에 잘리고 래스터가 깨진다(R29)
	static constexpr float SpendDipDuration = 0.18f;
	float DipElapsed = -1.f;
	void PlaySpendDip();

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
	// 롤업과 눌림이 공유하는 정체성 게이트 ― 금액 임계와 분리해 눌림이 잔액에 묶이지 않게 한다
	bool IsLiveWalletChip() const;
	bool ShouldRollup(int64 NewAmount) const;
	void EvaluateDigitMilestone(int64 DisplayedValue);
	void PlayMilestoneFeedback(int32 NewTier);
	void ApplyChipLineFlash(float Intensity);

	// 카운트업 롤 — 알파(0→1)만 애니(float 정밀도), 실값은 int64 From/To 를 double 보간 (거액 자릿수 보존)
	FNumberCountUpAnimation CountUpAnim;
	int64 CountFrom = 0;
	int64 CountTo = 0;

	FText LabelOverride;
	// SetAmountStyleOverride ― 알파 0 = 덮어쓰지 않음(기본)
	FLinearColor AmountInkOverride = FLinearColor::Transparent;
	FString AmountPrefix;
	bool bLabelSlotSaved = false;
	FSlateChildSize SavedLabelSlotSize;
	TEnumAsByte<EHorizontalAlignment> SavedLabelSlotAlign = HAlign_Fill;
	FMargin SavedLabelSlotPadding;

public:
	void SetResourceType(EResourceType type, bool bUseTypeColor = true);

	// bLiveCounterRollup 칩에 한해 유의미한 델타(재화별 하한 && >= 표시값 2%)면 자동으로 롤업 경로, 그 외는 즉시 대입.
	void SetValue(int64 amount);

	// 표시값만 바꾸고 피드백은 내지 않는다. 코인 플라이아웃이 연출 전에 표시를 되감는 것처럼,
	// "하강" 이지만 지출이 아닌 경로가 쓴다 - 그쪽이 SetValue 를 타면 수금할 때마다 칩이 지출처럼 눌린다.
	void SetValueSilent(int64 amount);

	// 카운트업 롤 — 현재 표시값에서 amount 로 Duration 동안 굴림 (드립 착지 "따라락"). 재생 중 재호출은 목표값만 갱신.
	void SetValueAnimated(int64 amount, float Duration = 0.45f);
	void SetValueWithMax(int64 current, int64 max);
	void SetCanAfford(bool bInCanAfford);

	// 재화가 아닌 아이템(뽑기권 등) 표시 — 임의 아이콘+개수. ResourceType 기반 아이콘/타입색을 끈다.
	void SetItemDisplay(UTexture2D* IconTexture, int64 Count);

	// 숫자 대신 문구를 쓰고 아이콘을 접는다(돈이 들지 않는 책상 강화의 「무료」 등). 빈 문구면 원래 표시로 돌아간다
	void SetLabelOverride(const FText& InLabel);

	// 숫자 잉크·앞글자를 이 인스턴스만 덮는다(밝은 칩 위 받을 수량 = 진한 잉크 + 「+」). 잉크 알파 0 · 빈 앞글자 = 기본 동작. 살 수 없음 빨강이 여전히 우선
	void SetAmountStyleOverride(const FLinearColor& InInk, const FString& InPrefix);

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

