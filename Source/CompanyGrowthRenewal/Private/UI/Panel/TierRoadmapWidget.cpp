#include "UI/Panel/TierRoadmapWidget.h"
#include "Core/CGGameInstance.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "UI/Element/Building/TierNodeWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Player/MainMapPlayerController.h"
#include "Global/GlobalUtilFunctions.h"
#include "Table/TierUnlockData.h"
#include "Data/BuildingEnhancementData.h"
#include "Data/ProjectBoardData.h"
#include "Data/GameSaveData.h"
#include "Enum/WidgetType.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "CommonTextBlock.h"

namespace
{
	// 이 빌딩의 티어 진행을 세이브에서 직접 읽는다.
	// ⚠ OfficeStageProgressManager(WorldSubsystem) 경유 금지 — MainMap 인스턴스의 TierProgress 는
	//    이 빌딩 것이 아니다. 로드맵은 MainMap 관리 패널에서도 열린다.
	const FProjectTierProgress* FindTierProgress(int32 BuildingIndex)
	{
		UCGGameInstance* GI = UCGGameInstance::GetInstance();
		if (!GI) { return nullptr; }
		USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
		if (!SaveMgr) { return nullptr; }
		USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData();
		if (!SaveData) { return nullptr; }

		const FOfficeSaveData* Office = SaveData->GameData.OfficeDataMap.Find(BuildingIndex);
		return Office ? &Office->TierProgress : nullptr;
	}
}

void UTierRoadmapWidget::ConfigureForBuilding(int32 BuildingIndex)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) { return; }
	USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!SaveMgr || !TableMgr) { return; }

	const int32 Tier = SaveMgr->GetBuildingTier(BuildingIndex);
	const ECompanyType CompanyType = SaveMgr->GetBuildingCompanyType(BuildingIndex);

	bool bAtMax = false;
	const float Percent = ComputeTierProgress(BuildingIndex, Tier, bAtMax);

	if (ExpBar)
	{
		ExpBar->SetPercent(Percent);
		UGlobalUtilFunctions::UpdateProgressHead(ExpBar, ExpBarHead, Percent);
	}
	if (ExpText)
	{
		const FProjectTierProgress* Tp = FindTierProgress(BuildingIndex);
		const int32 Cleared = Tp ? FMath::Min(Tp->GetTierClearedCount(Tier), TierConstants::CLEAR_TO_UNLOCK) : 0;
		ExpText->SetText(bAtMax
			? FText::FromString(TEXT("MAX"))
			: FText::Format(NSLOCTEXT("Tier", "RoadmapClearProgress", "클리어 {0} / {1}"),
				FText::AsNumber(Cleared), FText::AsNumber(TierConstants::CLEAR_TO_UNLOCK)));
	}

	// 레일 — 1~10단계 전량 생성 후 현재 노드로 스크롤 (스크롤 콘텐츠가 있어야 좌우 탐색이 성립)
	if (!NodeRail) { return; }
	NodeRail->ClearChildren();

	TSubclassOf<UUserWidget> NodeClass = TableMgr->GetWidgetClass(EWidgetType::TierNode);
	if (!NodeClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TierRoadmap] TierNode 위젯 미등록(DT_WidgetClass)"));
		return;
	}

	UTierNodeWidget* CurrentNode = nullptr;

	for (int32 T = 1; T <= TierConstants::MAX_TIER; ++T)
	{
		UTierNodeWidget* Node = CreateWidget<UTierNodeWidget>(this, NodeClass);
		if (!Node) { continue; }

		bool bRowOk = false;
		const FTierUnlockData RowData = TableMgr->GetTierUnlockData(T, bRowOk);

		TArray<FText> UnlockNames;
		if (bRowOk)
		{
			for (EBuildingEnhancementType Type : RowData.UnlockedEnhancements)
			{
				FBuildingEnhancementDefinition Def;
				if (!TableMgr->GetEnhancementDefinition(Type, Def)) { continue; }

				// 이 산업에 노출되지 않는 슬롯은 로드맵에서도 약속하지 않는다.
				// (구 버그: 제조업 빌딩에 히트작수명 해금을 약속해 놓고 패널엔 안 나왔다)
				if (!UBuildingEnhancementHelper::IsEnhancementVisibleForCompanyType(Def.Category, CompanyType))
				{
					continue;
				}
				UnlockNames.Add(Def.DisplayName);
			}
		}

		const ETierNodeState State = (T < Tier) ? ETierNodeState::Done
			: (T == Tier) ? ETierNodeState::Current : ETierNodeState::Upcoming;

		Node->Configure(T, State, UnlockNames);

		if (UHorizontalBoxSlot* BoxSlot = NodeRail->AddChildToHorizontalBox(Node))
		{
			BoxSlot->SetVerticalAlignment(VAlign_Top);
		}

		if (T == Tier) { CurrentNode = Node; }
	}

	// SScrollBox 는 요청을 큐에 담아 지오메트리가 준비된 Tick 에 처리하므로 여기서 불러도 안전하고,
	// 스크롤박스가 없으면 조용히 무시된다 (WBP 저작 전에도 무해)
	UScrollBox* Rail = NodeScrollBox ? NodeScrollBox : Cast<UScrollBox>(NodeRail->GetParent());
	if (Rail && CurrentNode)
	{
		Rail->ScrollWidgetIntoView(CurrentNode, false, EDescendantScrollDestination::Center, 0.f);
	}
}

float UTierRoadmapWidget::ComputeTierProgress(int32 BuildingIndex, int32 Tier, bool& bOutAtMax)
{
	bOutAtMax = Tier >= TierConstants::MAX_TIER;
	if (bOutAtMax) { return 1.f; }

	const FProjectTierProgress* Tp = FindTierProgress(BuildingIndex);
	if (!Tp) { return 0.f; }

	const int32 Cleared = Tp->GetTierClearedCount(Tier);
	return FMath::Clamp(
		static_cast<float>(Cleared) / static_cast<float>(TierConstants::CLEAR_TO_UNLOCK), 0.f, 1.f);
}

void UTierRoadmapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UTierRoadmapWidget::HandleCloseClicked);
	}

	// 바깥 클릭 = 닫기. 닫기 버튼과 동작이 같아 핸들러를 공유한다(둘 다 무인자 dynamic 델리게이트)
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UTierRoadmapWidget::HandleCloseClicked);
	}

	// WBP 저작값은 디자이너 확인용 고정 프리뷰라 첫 갱신 전까지 보이면 거짓 데이터가 된다
	UGlobalUtilFunctions::InitProgressHead(ExpBarHead);
}

void UTierRoadmapWidget::SetOwnsInputMode(bool bInOwns)
{
	bOwnsInputMode = bInOwns;
}

// 해제는 Construct 의 짝인 Destruct 에서 — Deactivate 는 위에 위젯이 push 될 때도 불려서
// 재활성 시 NativeConstruct 가 다시 안 돌면 닫기 버튼이 영구히 죽는다
void UTierRoadmapWidget::NativeDestruct()
{
	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.RemoveDynamic(this, &UTierRoadmapWidget::HandleCloseClicked);
	}

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UTierRoadmapWidget::HandleCloseClicked);
	}

	Super::NativeDestruct();
}

// 모드 복원은 표시 상태의 짝인 Deactivate 에서 — Destruct 는 풀링 때문에 훨씬 늦게 올 수 있다
void UTierRoadmapWidget::NativeOnDeactivated()
{
	if (bOwnsInputMode)
	{
		if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
		{
			AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GI->GetCurrentPlayerController());
			if (PC && PC->GetCurrentInputMode() == EInputMode::UI)
			{
				PC->GoToNormalMode();
			}
		}
		// 풀링 재사용 인스턴스가 관리 패널 경로로 다시 열릴 때 소유권이 새어나가지 않게 매번 반납
		bOwnsInputMode = false;
	}

	Super::NativeOnDeactivated();
}

void UTierRoadmapWidget::HandleCloseClicked()
{
	DeactivateWidget();
}
