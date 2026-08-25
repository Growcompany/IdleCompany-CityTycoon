#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Table/MissionTable.h"
#include "RewardRevealPresentationWidget.generated.h"

class UHorizontalBox;
class UCommonTextBlock;
class UButtonWidget;
class UItemCardSlotWidget;
class UWidget;
class UTexture2D;

/**
 * 미션 보상 연출 베이스 — WBP 변형 2종이 이 클래스를 공유한다.
 *   UI_RewardReveal : 딤 + [받기] + 카운트다운 모달. 아이템(티켓) 보상 미션 전용.
 *   UI_RewardToast  : 딤/버튼 없는 센터 밴드 토스트. 자원 전용 보상 미션.
 * 박스는 UIE_ItemCard_QtyBelow 재사용, 좌→우 스태거 등장. 보상은 이미 지급됨(연출 전용).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API URewardRevealPresentationWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	URewardRevealPresentationWidget(const FObjectInitializer& ObjectInitializer);

	// 매니저가 AddToViewport 직후 1회 호출 — 박스 구성 + 등장 시작.
	// bInToastMode = 밴드 토스트(입력 통과, 짧은 홀드). false = 기존 모달.
	void SetupRewards(const TArray<FMissionReward>& InRewards, const FText& InTitle, bool bInToastMode = false, bool bInShowToastTitle = false);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UHorizontalBox* BoxRow;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UCommonTextBlock* TitleText;
	// 아래 둘은 모달 트리에만 존재 — 토스트 트리엔 없어서 Optional
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UCommonTextBlock* CountdownText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButtonWidget* ReceiveButton;

	UFUNCTION() void OnReceiveClicked();

	// ── 토스트 튜닝 노브 (UI_RewardToast 의 Class Defaults 가 튜닝 표면 — 값 바꾸려고 재빌드하지 말 것) ──
	// 미션 이름 표시. 끄면 보상만 뜨고 밴드가 세로로 짧아진다 (기본 끔 — 카드를 방금 탭했으므로 맥락은 이미 안다).
	UPROPERTY(EditAnywhere, Category = "Toast") bool bToastShowTitle = false;
	UPROPERTY(EditAnywhere, Category = "Toast") float ToastFadeInSeconds = 0.20f;
	UPROPERTY(EditAnywhere, Category = "Toast") float ToastStaggerSeconds = 0.12f;
	UPROPERTY(EditAnywhere, Category = "Toast") float ToastHoldSeconds = 2.80f;
	UPROPERTY(EditAnywhere, Category = "Toast") float ToastFadeOutSeconds = 0.30f;
	// 카드 사이 간격만 여기서. 카드 자체의 크기/간격/필 색은 UIE_ItemCard_QtyBelow_Toast 가 소유한다
	// (공유 카드를 코드로 비틀면 플레이트·수량필 비례가 어긋난다 — 2026-07-27 3회 반복 후 변형 WBP 로 분리).
	UPROPERTY(EditAnywhere, Category = "Toast") float ToastCardGapPx = 44.f;

private:
	// Icon/Label = 시각, 뒤 3개 = 클릭 툴팁 데이터(이름/설명/단가). 토스트는 입력 통과라 툴팁을 걸지 않는다.
	UItemCardSlotWidget* MakeBox(UTexture2D* Icon, const FText& Label, const FText& TooltipName, const FText& TooltipDesc, int32 TooltipPrice = 0);
	void BeginClose();

	// 모달용 / 토스트용 카드. 토스트 변형은 검은 밴드 위 기준으로 수량필 색(S3)·간격·글자를 자체 보유.
	UPROPERTY() TSubclassOf<UUserWidget> BoxWidgetClass;
	UPROPERTY() TSubclassOf<UUserWidget> ToastBoxWidgetClass;

	// 스태거/페이드 대상. 토스트는 카드를 SizeBox 로 감싸므로 카드가 아닌 그 컨테이너가 들어간다.
	UPROPERTY() TArray<TObjectPtr<UWidget>> Boxes;
	int32 RevealIndex = 0;
	float RevealTimer = 0.f;
	bool bRevealing = false;

	bool bToastMode = false;
	float HoldDelay = 5.0f;         // 등장 완료 후 유지 시간 (모드별로 SetupRewards 가 결정)
	float FadeInElapsed = -1.f;     // >=0 = 밴드 페이드인 중 (토스트 전용). 끝나면 -1 로 복귀

	float AutoCloseElapsed = -1.f;  // <0 = 등장 미완. 등장 완료 시 0 으로 시작
	int32 LastShownSec = -1;        // 카운트다운 갱신 게이트(초 바뀔 때만 SetText)
	bool bClosing = false;
	float CloseElapsed = 0.f;

	// 모달 고정값 (토스트 쪽은 위 EditAnywhere 노브)
	static constexpr float StaggerInterval = 0.12f;
	static constexpr float AutoCloseDelay = 5.0f;
	static constexpr float CloseDuration = 0.3f;

	// 모드별로 결정되는 실효 타이밍 (SetupRewards 가 대입)
	float ActiveStagger = StaggerInterval;
	float ActiveCloseDuration = CloseDuration;
};
