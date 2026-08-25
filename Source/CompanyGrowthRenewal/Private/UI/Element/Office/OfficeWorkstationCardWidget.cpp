// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Office/OfficeWorkstationCardWidget.h"
#include "UI/Element/Cards/EntityCardFrameWidget.h"
#include "Data/EntityCardData.h"

// Core & Game
#include "Core/CGGameInstance.h"

// Managers
#include "Manager/UIManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"

// Office
#include "Office/OfficeManager.h"

// UI
#include "UI/Element/Cards/CardInfoWidget.h"
#include "UI/UIBase.h"
#include "UI/Panel/BuildPlacementPanelWidget.h"

// Player
#include "Player/OfficePlayerController.h"
#include "Player/OfficeCameraPawn.h"

// Enum
#include "Enum/WidgetType.h"

// Engine
#include "Engine/AssetManager.h"

// Components
#include "Components/Border.h"
#include "Components/Image.h"
#include "CommonTextBlock.h"

void UOfficeWorkstationCardWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	if (UEntityCardData* CardData = Cast<UEntityCardData>(ListItemObject))
	{
		SetWorkstationInfo(CardData->WorkstationInfo);
	}
}

void UOfficeWorkstationCardWidget::SetWorkstationInfo(const FWorkstationCardTable& InWorkstationInfo)
{
	WorkstationInfo = InWorkstationInfo;
	CardInfoWidget->SetWorkstationInfo(WorkstationInfo);

	// SoftObjectPath로 아이콘 비동기 로드
	if (!WorkstationInfo.UIIcon.IsNull())
	{
		const FSoftObjectPath IconPath = WorkstationInfo.UIIcon.ToSoftObjectPath();

		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			IconPath,
			FStreamableDelegate::CreateUObject(
				this,
				&UEntityCardWidgetBase::OnIconLoaded,
				IconPath
			)
		);
	}

	// 잠금 조건 체크 (현재는 모두 해금)
	CheckUnlockCondition();

}

void UOfficeWorkstationCardWidget::HandleCardClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeWorkstationCardWidget] Workstation card clicked: %s"), *WorkstationInfo.DisplayName.ToString());

	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeWorkstationCardWidget] GameInstance not found!"));
		return;
	}

	// OfficePlayerController와 OfficeCameraPawn 가져오기
	AOfficePlayerController* PC = Cast<AOfficePlayerController>(GameInstance->GetCurrentPlayerController());
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeWorkstationCardWidget] OfficePlayerController not found!"));
		return;
	}

	AOfficeCameraPawn* OfficePawn = Cast<AOfficeCameraPawn>(PC->GetPawn());
	if (OfficePawn)
	{
		// 업무공간 배치 모드 시작
		OfficePawn->BeginWorkstationPlacement(WorkstationInfo);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeWorkstationCardWidget] OfficeCameraPawn not found!"));
		return;
	}

	// BuildPlacementPanelWidget 열기
	UTableManagerSubsystem* TableManager = GameInstance->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GameInstance->GetSubsystem<UUIManagerSubsystem>();

	if (!TableManager || !UIManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeWorkstationCardWidget] TableManager or UIManager not found!"));
		return;
	}

	TSubclassOf<UUserWidget> PlacementWidgetClass = TableManager->GetWidgetClass(EWidgetType::BuildPlacementPanel);

	// 카탈로그를 닫고 나면 이 카드(카탈로그의 자식)는 위젯 풀로 반납된다 — 멤버를 더 읽지 않도록 먼저 복사.
	const FWorkstationCardTable InfoCopy = WorkstationInfo;

	UUIBase* UIBase = UIManager->GetUIBase();

	// 카탈로그를 배치 바 "아래에 깔아두지" 않는다 — 깔아두면 배치 종료 시 되살아난다.
	// (CommonUI 는 표시 중인 위젯 제거를 전환 완료 후로 미루므로, 끝에서 두 번 pop 해도 카탈로그는 안 닫힌다.)
	// 닫기 먼저, push 나중 — 순서 고정.
	UIBase->PopBottomWidget();

	UBuildPlacementPanelWidget* PlacementWidget = Cast<UBuildPlacementPanelWidget>(UIBase->PushBottomClass(PlacementWidgetClass.Get()));

	if (PlacementWidget)
	{
		PlacementWidget->SetWorkstationInfo(InfoCopy);
		UE_LOG(LogTemp, Log, TEXT("[OfficeWorkstationCardWidget] BuildPlacementPanelWidget opened with workstation info"));
	}
}

void UOfficeWorkstationCardWidget::CheckUnlockCondition()
{
	bool bIsLocked = false;
	FText LockReason = FText::GetEmpty();

	// 레벨 조건 체크
	if (WorkstationInfo.UnlockLevel > 1)
	{
		// TODO: 플레이어 레벨과 비교
		// 현재는 모두 해금된 것으로 처리
	}

	UpdateLockUI(bIsLocked, LockReason);
}
