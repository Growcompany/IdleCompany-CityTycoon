#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Manager/LaunchLootManagerSubsystem.h"
#include "LaunchLootOddsWidget.generated.h"

class UVerticalBox;
class UFont;

/**
 * 출시 보상 전체 확률표 팝업 — 기획 보드 [+N] 더보기 카드가 띄운다.
 * ItemTooltip 과 같은 비모달 인라인 팝오버(viewport 직접 add, 자동 dismiss).
 * 행이 전부 코드 생성이라 WBP 없이 자가 트리(RebuildWidget) — BulkModeSelector 패턴.
 * 목업 SOT = Artifact f44bf338 v2 (S3 서브면 760px, 아이콘/이름/조건·확률 행).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ULaunchLootOddsWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// 앵커(화면 절대좌표) 아래에 표시. 호출마다 행 재구성 — 인스턴스 1개 재사용(ItemTooltip 패턴)
	void ShowAt(FVector2D AbsoluteScreenPos, const TArray<FLaunchLootPreviewEntry>& Entries, float DurationSec = 6.0f, int32 ZOrder = 101);

	// 뭉치 지급 표기 "(2~3장)" — 상위 등급에서 슬롯 %가 낮아 보이는 오독 방지 (툴팁/확률표 공용)
	static FString GetBandAmountSuffix(const FLaunchLootPreviewEntry& Entry, int32 BandIndex);

	void Hide();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;

private:
	void EnsureTree();
	void BuildRows(const TArray<FLaunchLootPreviewEntry>& Entries);

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RowsBox;

	UPROPERTY(Transient)
	TObjectPtr<UFont> BoldFont;

	UPROPERTY(Transient)
	TObjectPtr<UFont> RegularFont;

	FTimerHandle DismissTimerHandle;
};
