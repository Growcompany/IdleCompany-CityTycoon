#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

// 본사 3탭 실화면 검수용 — 원격 파이썬의 execute_console_command 로 부른다(Exec 치트는 파이썬에서 안 불린다)
// CGR.HQ.SetState <HQLevel|-1> <Money|-1> [TopTier] : 메모리 상태만 바꾼다(저장 호출 없음). TopTier = 오피스 기록이 있는 첫 빌딩의 단계
// CGR.HQ.Open <TabIndex 0~2>               : 열린 본사 패널이 있으면 닫고, 없으면 열어 탭을 고른다

#include "CommonButtonBase.h"
#include "Data/GameSaveData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Enum/ResourceType.h"
#include "Enum/WidgetType.h"
#include "HAL/IConsoleManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Player/MainMapPlayerController.h"
#include "UI/Panel/HQManagePanelWidget.h"
#include "UI/UIBase.h"
#include "UObject/UObjectIterator.h"

namespace CGRHQCapture
{
	static void SetState(const TArray<FString>& Arguments, UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		USaveLoadManager* SaveMgr = GameInstance ? GameInstance->GetSubsystem<USaveLoadManager>() : nullptr;
		UResourceItemManager* ResMgr = GameInstance ? GameInstance->GetSubsystem<UResourceItemManager>() : nullptr;
		USaveGame_GameData* SaveData = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
		if (!SaveData || !ResMgr || Arguments.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("[HQCapture] Usage: CGR.HQ.SetState <HQLevel|-1> <Money|-1> (game world required)"));
			return;
		}
		const int32 Level = FCString::Atoi(*Arguments[0]);
		const int64 Money = FCString::Atoi64(*Arguments[1]);
		if (Level >= 1) SaveData->GameData.HQLevel = Level;
		if (Arguments.Num() >= 3)
		{
			const int32 TopTier = FCString::Atoi(*Arguments[2]);
			for (auto& Pair : SaveData->GameData.OfficeDataMap)
			{
				Pair.Value.TierProgress.CurrentTier = FMath::Max(1, TopTier);
				break;
			}
		}
		if (Money >= 0)
		{
			const int64 Current = ResMgr->GetResourceAmount(EResourceType::Money);
			if (Money > Current) ResMgr->StoreResource(EResourceType::Money, Money - Current, false);
			else if (Money < Current) ResMgr->SpendResource(EResourceType::Money, Current - Money, false);
		}
		UE_LOG(LogTemp, Display, TEXT("[HQCapture] state HQLv=%d Money=%lld CanLevelUp=%d"),
			SaveData->GameData.HQLevel, ResMgr->GetResourceAmount(EResourceType::Money), SaveMgr->CanLevelUpHQ() ? 1 : 0);
	}

	static void Open(const TArray<FString>& Arguments, UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		UUIManagerSubsystem* UIMgr = GameInstance ? GameInstance->GetSubsystem<UUIManagerSubsystem>() : nullptr;
		UTableManagerSubsystem* TableMgr = GameInstance ? GameInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		UUIBase* UIBase = UIMgr ? UIMgr->GetUIBase() : nullptr;
		if (!UIBase || !TableMgr)
		{
			UE_LOG(LogTemp, Error, TEXT("[HQCapture] UIBase/TableManager missing"));
			return;
		}
		// 열려 있으면: 탭 번호를 주면 그 탭으로 바꾸고(사용자 탭 클릭과 같은 경로), 없으면 닫는다
		UHQManagePanelWidget* Panel = nullptr;
		for (TObjectIterator<UHQManagePanelWidget> It; It; ++It)
		{
			if (It->GetWorld() == World && It->IsActivated()) { Panel = *It; break; }
		}
		if (Panel && Arguments.Num() == 0)
		{
			Panel->DeactivateWidget();
			UE_LOG(LogTemp, Display, TEXT("[HQCapture] closed open HQ panel"));
			return;
		}
		const int32 TabIndex = Arguments.Num() > 0 ? FMath::Clamp(FCString::Atoi(*Arguments[0]), 0, 2) : 0;
		if (!Panel)
		{
			Panel = Cast<UHQManagePanelWidget>(UIBase->PushPromptClass(TableMgr->GetWidgetClass(EWidgetType::HQManagePanel).Get()));
			if (!Panel)
			{
				UE_LOG(LogTemp, Error, TEXT("[HQCapture] HQ panel push failed"));
				return;
			}
			if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(World->GetFirstPlayerController()))
			{
				PC->GoToUIMode();
			}
			UE_LOG(LogTemp, Display, TEXT("[HQCapture] HQ panel opened (tab 0); call again with a tab index to switch"));
			return;
		}
		static const TCHAR* TabNames[] = { TEXT("HQTab"), TEXT("OperationsTab"), TEXT("BuildingListTab") };
		if (FObjectProperty* TabProperty = FindFProperty<FObjectProperty>(UHQManagePanelWidget::StaticClass(), TabNames[TabIndex]))
		{
			if (UCommonButtonBase* Tab = Cast<UCommonButtonBase>(TabProperty->GetObjectPropertyValue_InContainer(Panel)))
			{
				Tab->SetIsSelected(true, false);
			}
		}
		UE_LOG(LogTemp, Display, TEXT("[HQCapture] HQ panel opened on tab %d"), TabIndex);
	}

	static FAutoConsoleCommandWithWorldAndArgs SetStateCommand(TEXT("CGR.HQ.SetState"),
		TEXT("Set HQ level and money in memory for HQ panel capture"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetState));
	static FAutoConsoleCommandWithWorldAndArgs OpenCommand(TEXT("CGR.HQ.Open"),
		TEXT("Toggle the HQ panel and select a tab for capture"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Open));
}

#endif
