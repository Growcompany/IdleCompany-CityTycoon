// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "BuildOpenWidget.generated.h"

class UChatMiniLogWidget;
class ABrickFactory;
class USaveLoadManager;
class UAlertMarkWidget;
enum class EFactoryUpgradeType : uint8;

/**
 *
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UBuildOpenWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// 미션 가이드 펄스 링 타겟 (MissionManagerSubsystem)
	class UWidget* GetBuildOpenButtonWidget() const;
	class UWidget* GetFactoryOpenButtonWidget() const;
	class UWidget* GetWorldMapButtonWidget() const;
	// 튜토리얼 M11 — MainMap 하단 [뽑기] 버튼 (빌딩 특성 가챠 진입) 하이라이트용
	class UWidget* GetGachaButtonWidget() const;
	// 미션판 G2(본사 레벨) 안내 — 하단 도크 [본사] 버튼
	class UWidget* GetHeadquartersButtonWidget() const;
	// 튜토리얼 M33 메뉴 소개 — 도크 [메뉴] 버튼
	class UWidget* GetMenuButtonWidget() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	UFUNCTION()
	void OnBuildOpenButtonClicked();

	UFUNCTION()
	void OnFactoryOpenButtonClicked();

	UFUNCTION()
	void OnMenuButtonClicked();

	UFUNCTION()
	void OnShopButtonClicked();

	UFUNCTION()
	void OnWorldMapButtonClicked();

	UFUNCTION()
	void OnHeadquartersButtonClicked();

	// 우상단 [뽑기] — 가챠 직접 진입 (1클릭, BM 마찰 최소화)
	// 컨텍스트별로 어느 가챠 화면으로 보낼지 결정 (MainMap=특성 가챠, OfficeMap=직원 가챠 등)
	UFUNCTION()
	void OnGachaButtonClicked();

	// 우상단 [미션] — 메뉴판을 거치지 않고 미션판으로 간다 (사용자 확정 2026-10-17, 메뉴판 미션 타일은 유지)
	UFUNCTION()
	void OnQuestButtonClicked();

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	class UButtonWidget* BuildOpenButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	class UButtonWidget* FactoryOpenButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	class UButton* MenuBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	class UButton* ShopBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	class UButton* WorldMapBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	class UButtonWidget* HeadquartersBtn;

	// MenuBtn 위 오버레이에 얹는 알림 도트 — 메뉴 시트 안에 받을 것이 하나라도 있으면 표시
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	class UAlertMarkWidget* MenuAlertMark;

	// [뽑기] 가챠 진입 (1클릭, 우상단) — WBP 매칭 이름 GachaBtn
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	class UButton* GachaBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	class UButton* QuestBtn;

	// 받을 미션 보상 수 배지 — 0 이거나 튜토리얼 중이면 접는다(메뉴 빨간 점과 같은 규칙)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	class UWidget* QuestBadge;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	class UCommonTextBlock* QuestBadgeCount;

	// 월드맵 진입 잠금 도장 — 제조업(2막) 해금 전까지 표시. 그룹 딤은 이 이미지의 부모 오버레이에 건다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	class UImage* WorldMapLockImage;

	// 채팅 미니로그 (WBP에서 직접 배치)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UChatMiniLogWidget* ChatMiniLog = nullptr;

	// 버튼 영역 슬라이드 애니메이션 (블루프린트에서 생성)
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	UWidgetAnimation* ShowButtonsAnim;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	UWidgetAnimation* HideButtonsAnim;

private:
	// 알림 마크 재평가 — 타이머 & 델리게이트 공용 엔트리
	void RefreshAlertMarks();
	void RefreshQuestBadge(int32 ClaimableCount);

	// 월드맵 = 2막 콘텐츠 — 제조업 해금과 동시 개방 (판정 소스는 DT_CompanyInfo)
	bool IsWorldMapUnlocked();
	void RefreshWorldMapLock();

	UFUNCTION() void HandleHQLeveledUp(int32 NewLevel);
	UFUNCTION() void HandleFactoryUpgraded(EFactoryUpgradeType UpgradeType);

	// 2초 주기로 자원/조건 재평가 (완료 델리게이트는 즉시 꺼짐을 보장, 타이머는 누락 방지)
	FTimerHandle AlertMarkPollHandle;

	UPROPERTY()
	TObjectPtr<ABrickFactory> CachedFactory = nullptr;

	UPROPERTY()
	TObjectPtr<USaveLoadManager> CachedSaveMgr = nullptr;
};
