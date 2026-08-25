// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Cards/BuildEntityCardWidget.h"
#include "Data/EntityCardData.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "UI/Element/Cards/CardInfoWidget.h"
#include "UI/Element/Cards/BuildCardInfoWidget.h"
#include "UI/Element/Cards/EntityCardFrameWidget.h"
#include "Engine/AssetManager.h"

void UBuildEntityCardWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 에디터에서만 미리보기 적용 (게임 실행 중에는 무시)
	if (IsDesignTime())
	{
		if (bPreviewLocked)
		{
			UpdateLockUI(true, FText::FromString(TEXT("미리보기 잠금")));
		}
		else
		{
			UpdateLockUI(false, FText::GetEmpty());
		}
	}
}

void UBuildEntityCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// UIManagerSubsystem을 통한 리소스 변경 구독
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		UIResourceChangedHandle = UIMgr->OnUIResourceChanged.AddUObject(
			this, &UBuildEntityCardWidget::HandleResourceChanged);
	}
}

void UBuildEntityCardWidget::NativeDestruct()
{
	Super::NativeDestruct();

	// UIManagerSubsystem 구독 해제
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		UIMgr->OnUIResourceChanged.Remove(UIResourceChangedHandle);
	}
}

void UBuildEntityCardWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	// ListView에서 데이터가 설정될 때 호출됨 (스크롤 시 재활용될 때마다)
	if (UEntityCardData* CardData = Cast<UEntityCardData>(ListItemObject))
	{
		SetBuildableInfo(CardData->BuildableInfo);

		// 본사 레벨 잠금: RefreshBuildingCards 에서 설정된 값을 카드 프레임에 반영.
		// CheckHasConstructionCost 가 UpdateLockUI 를 덮어쓸 수 있으므로 이후에 호출해야 한다.
		// SetBuildableInfo → CheckHasConstructionCost 가 이미 실행됐으므로 여기서 덮어쓰는 것이 안전.
		// 잠금/해제 양방향 모두 반영 — 재활용된 ListView 행이 이전 카드의 잠금 상태(bIsCurrentlyLocked)를
		// 남기면 해제된 카드가 잠긴 프레임 그대로 클릭 불가가 되므로, 항상 현재 값으로 갱신한다.
		UpdateLockUI(CardData->bLevelLocked, CardData->LevelLockReason);

		// 빌딩 전용 정보(크기·인원) 주입 — CardInfoWidget 이 빌딩 파생형일 때만.
		if (UBuildCardInfoWidget* BuildInfo = Cast<UBuildCardInfoWidget>(CardInfoWidget))
		{
			BuildInfo->SetBuildingStats(
				CardData->FootprintWidthCells,
				CardData->FootprintDepthCells,
				CardData->NewBuildEmployees);
		}

		// 크기 배지는 프레임(썸네일 우상단) 소관 — 정보 위젯이 아니라 프레임에 주입.
		if (UIE_EntityCardFrame)
		{
			UIE_EntityCardFrame->SetSizeBadge(CardData->FootprintWidthCells, CardData->FootprintDepthCells);
		}
	}
}

void UBuildEntityCardWidget::SetBuildableInfo(const FBuildableCardTable& InBuildableInfo)
{
	BuildableInfo = InBuildableInfo;
	CardInfoWidget->SetBuildableInfo(BuildableInfo);

	// SoftObjectPath로 아이콘 비동기 로드
	const FSoftObjectPath IconPath = BuildableInfo.UIIcon.ToSoftObjectPath();

	UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
		IconPath,
		FStreamableDelegate::CreateUObject(
			this,
			&UEntityCardWidgetBase::OnIconLoaded,
			IconPath
		)
	);

	CheckHasConstructionCost();
}

void UBuildEntityCardWidget::HandleResourceChanged(EResourceType Type, int64 NewValue)
{
	CheckHasConstructionCost();
}

void UBuildEntityCardWidget::CheckHasConstructionCost()
{
	// 건설 비용 체크 (해금 잠금은 RefreshBuildingCards → NativeOnListItemObjectSet 에서 처리)
	if (UResourceItemManager* RMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
	{
		for (const FConstructionCost& Cost : BuildableInfo.ConstructionCosts)
		{
			// 부족 표시는 비용 칩(빨강)만 담당 — 카드 전체 디밍은 아트를 죽여 폐기 (2026-07-22 사용자 결정)
			if (CardInfoWidget)
			{
				CardInfoWidget->UpdateCostAffordability(Cost.ResourceType, RMgr->HasResource(Cost.ResourceType, Cost.Cost));
			}
		}

		// 구버전 디밍(0.55) 잔상 복원 — ListView 재활용 인스턴스 대비
		SetRenderOpacity(1.0f);
	}
}

void UBuildEntityCardWidget::HandleCardClicked()
{
	// 카드 클릭 = 선택만. 상한 체크/BeginBuild/배치 패널 push 는 BuildModalWidget CTA 가 처리
	OnCardSelected.Broadcast(BuildableInfo);
}

