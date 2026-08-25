// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Manager/EmployeeManager.h"
#include "Utils/FWidgetAnimationUtils.h"
#include "EnhanceStarforceModalWidget.generated.h"

class UBorder;
class UPanelWidget;
class UTextBlock;
class UButtonWidget;
class UCloseButtonWidget;
class UImage;
class UTexture2D;
class UResourceWidget;
class UUniformGridPanel;
enum class EResourceType : uint8;

/**
 * 직원 강화 스타포스 모달 (UI_EnhanceStarforceModal) — 직원창 [강화] 버튼이 PushPromptClass 로 오픈.
 *
 * 12칸 ★ 진행판 + 성공/유지/하락 확률 공개 + Money 비용 + [강화] CTA. 파괴 없음, ★12 = MAX.
 * 입력모드는 아래 깔린 직원창이 소유 — 스택 위 모달은 건드리지 않음 (HQLevelUpCelebration/ConfirmCancel 선례).
 * 설계 SOT = docs/superpowers/specs/2026-07-14-enhancement-starforce-design.md §4
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UEnhanceStarforceModalWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

public:
	// 직원창이 push 직후 호출 — 대상 지정 + 델리게이트 재바인딩 + 전체 리빌드
	void Configure(int32 InEmployeeID);

	// M18 미션 가이드 — [강화] 실행 버튼 하이라이트 타겟
	class UWidget* GetEnhanceActionButtonWidget() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ★ 12칸 진행판 컨테이너 — C++ 가 UImage 별 12개 채움. 그룹 3개 바인딩 시 5·5·2 분배(v2), 없으면 StarBox 일렬
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UPanelWidget* StarBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UPanelWidget* StarGroup0;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UPanelWidget* StarGroup1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UPanelWidget* StarGroup2;

	// 레벨 전환 리드아웃 "N ▶ N+1" (메이플 스타포스 문법 — MAX 시 행 숨김)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* LevelFromText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* LevelToText;

	// 대상 칩 — 직원 이름 + 현재 ★N (Configure/Rebuild 주입)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* TargetNameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* TargetStarsText;

	// 확률 3행 — 0% 구간은 칩(있으면)째 Collapsed
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* OddsSuccessText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* OddsMaintainText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* OddsDowngradeText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* OddsSuccessChip;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* OddsMaintainChip;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* OddsDowngradeChip;

	// 성공 버스트 — 카드 뒤 골드 방사 글로우 (Glow_Oval, 코드 스케일+페이드)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* BurstGlow;

	// 결과 스탬프(도장) — ResultText 를 감싼 -8° 보더, 결과색 아웃라인 코드 주입 (v4)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* ResultStamp;

	// 카드 뒤 상시 앰비언트 골드 글로우 (숨쉬기 펄스 — NativeTick)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* AmbientGlow;

	// 성공 착지 스파클 3연타 (DT_UIVFXTexture SparkleMain/Sub 주입 — HQ 축하 문법)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* SparkleA;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* SparkleB;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* SparkleC;

	// 성공 착지 광택 스윕 — 별판을 좌→우로 스치는 샤인
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* ShineSweep;

	// CTA 레디 샤인 (DT CTAShine 주입, 강화 가능 동안 주기 스윕 — HQ 패널 문법)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* CTAShineImage;

	// 별판 골드 글로우 — 성공 착지 펄스 / 하락 레드 틴트 (트리 WellGlow)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* WellGlow;

	// 비용 행 (v4 미니멀 — 코인 아이콘 + 잉크 수치, 부족 시 빨강. MAX 면 행째 Collapsed)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UPanelWidget* CostRow;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* CostValueText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* PreviewText;

	// 프리뷰 존 패널 (v4 = 능력치 변화 존 컨테이너로 재용도 — PreviewText 가 캡션, MAX 시 존째 숨김)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* PreviewChip;

	// 성공 시 능력치 변화 그리드 (v4) — C++ 가 8행(이름 / 현재 ▶ 이후) 생성·채움. 2열 × 4행
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UUniformGridPanel* DeltaGrid;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* EnhanceActionButton;

	// 결과 리드아웃 — 성공(골드)/유지(뮤트)/하락(레드), ★12 = "MAX"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* ResultText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCloseButtonWidget* CloseButton;

	// Money 비용 칩 (UIE_Resource — SetCanAfford 빨강). MAX 면 Collapsed
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UResourceWidget* CostChip;

	// 결과 플래시 — 카드 풀블리드 RoundedBox (흰 원본, 코드가 결과색 틴트+페이드)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* ResultFlash;

	// C++ 생성 별 이미지 박스 px (WBP Class Defaults 튜닝 노브) — v4 우측 도킹(폭 860)에 맞춘 54
	UPROPERTY(EditDefaultsOnly, Category = "Starforce")
	int32 StarSize = 54;

private:
	int32 EmployeeID = -1;

	UPROPERTY()
	UEmployeeManager* EmployeeManager;

	// C++ 생성 별 12개 — 컨테이너 풀링 재사용 대비 1회 생성 후 브러시만 갱신
	UPROPERTY()
	TArray<UImage*> StarImages;

	void EnsureStarImages();

	// 별 상태 = 사전 합성 텍스처 2장 스왑 (HQ GetCondCheckIcon 패턴) — 골드+아웃라인+섀도 / 스틸+엠보스가 baked
	void ApplyStarStyle(UImage* Star, bool bFilled) const;

	// 사전 합성 별 텍스처 (EnsureStarImages 에서 1회 로드 — Transient 캐시)
	UPROPERTY(Transient)
	UTexture2D* StarTexFilled = nullptr;

	UPROPERTY(Transient)
	UTexture2D* StarTexEmpty = nullptr;

	// 능력치 변화 8행 (이름은 1회, 수치는 Rebuild 마다)
	UPROPERTY()
	TArray<UTextBlock*> DeltaCurTexts;

	UPROPERTY()
	TArray<UTextBlock*> DeltaNextTexts;

	void EnsureDeltaRows();

	// ===== 결과 연출 (성공=별 팝+골드 플래시 / 유지=미세 펄스 / 하락=셰이크+레드 플래시) =====

	FScalePunchAnimation StarPunch;
	FScalePunchAnimation ResultPunch;
	int32 PunchStarIndex = -1;

	// 플래시 (-1 = 비활성). 색·피크 알파는 결과별
	float FlashElapsed = -1.f;
	float FlashPeakAlpha = 0.f;
	FLinearColor FlashColor = FLinearColor::White;

	// 화면 셰이크 (-1 = 비활성) — 하락 전용, 루트 오버레이째 흔들기
	float ShakeElapsed = -1.f;

	// 성공: 새 별 내리꽂기 (크게 나타나 빈 칸에 박힘 — 착지 순간 플래시/버스트/사운드)
	float SlamElapsed = -1.f;

	// 하락: 별 깨짐 (붉게 흔들리며 팽창+소멸)
	float BreakElapsed = -1.f;

	// 성공 착지 후속: 스파클 3연타 / 광택 스윕 (-1 = 비활성)
	float SparkleElapsed = -1.f;
	float ShineElapsed = -1.f;

	// 앰비언트 펄스 시계
	float AmbientTime = 0.f;

	// 성공 버스트 (-1 = 비활성) — 스케일 0.7→1.35 + 페이드
	float BurstElapsed = -1.f;

	// 결과 리드아웃 자동 페이드 (-1 = 비활성, MAX 상시 표기는 페이드 없음)
	float ResultFadeElapsed = -1.f;

	// 다음 별 펄스 시계
	float PulseTime = 0.f;

	// ===== v4.2 화려함 패스 =====

	// 성공 착지: 델타존(능력치 변화) 강조 펀치
	FScalePunchAnimation DeltaPunch;

	// 성공 착지: 웰 글로우 펄스 / 하락: 웰 레드 틴트 (-1 = 비활성)
	float WellPulseElapsed = -1.f;
	float WellRedElapsed = -1.f;

	// 아이들 별 글린트 — 주기마다 채워진 별 하나가 살짝 팝
	float GlintTimer = 0.f;
	float GlintElapsed = -1.f;
	int32 GlintStarIndex = -1;

	// CTA 샤인 주기 시계 (강화 가능일 때만 스윕)
	float VfxTime = 0.f;
	bool bSheenActive = false;

	void PlayResultFx(UEmployeeManager::EEnhanceResult Result, int32 NewLevel);

	// 결과 스탬프/텍스트 일괄 세팅 (빈 텍스트 = 스탬프 숨김)
	void SetStamp(const FText& InText, const FLinearColor& InColor);

	// 별판/확률/비용/버튼 전체 갱신. ResultText 는 건드리지 않음(MAX 진입 시 비어 있을 때만 채움)
	void Rebuild();

	// Money 변화 경량 갱신 — 버튼 활성/비용색/칩만
	void RefreshAffordability();

	void OnEnhanceClicked();

	UFUNCTION()
	void OnCloseDelegate();

	void HandleEmployeeEnhanced(int32 InEmployeeID, UEmployeeManager::EEnhanceResult Result);
	void HandleResourceChanged(EResourceType Type, int64 NewValue);
};
