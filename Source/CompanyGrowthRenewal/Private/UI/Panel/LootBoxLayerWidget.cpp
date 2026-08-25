// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/LootBoxLayerWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/IconButtonWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "CommonTextBlock.h"
#include "Core/CGGameInstance.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Data/GameSaveData.h"
#include "Data/LootBoxInventoryData.h"
#include "Table/BuildingSkinData.h"
#include "Enum/ResourceType.h"
#include "Enum/WidgetType.h"
#include "Kismet/GameplayStatics.h"

void ULootBoxLayerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// TableManager 초기화
	TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();

	// IconButtonClass 가져오기
	if (TableMgr)
	{
		IconButtonClass = TableMgr->GetWidgetClass(EWidgetType::IconButton);
		if (!IconButtonClass)
		{
			UE_LOG(LogTemp, Error, TEXT("[LootBoxLayerWidget] Failed to get IconButton class from TableManager"));
		}
	}

	BindButtonEvents();

	// GameInstance에서 현재 카테고리 가져오기
	if (UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance()))
	{
		CurrentCategory = GameInstance->GetCurrentLootBoxCategory();
		SetCategory(CurrentCategory);
	}
}

void ULootBoxLayerWidget::NativeDestruct()
{
	UnbindButtonEvents();
	Super::NativeDestruct();
}

void ULootBoxLayerWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 활성화될 때 목록 갱신
	RefreshLootBoxList();
}

void ULootBoxLayerWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
}

void ULootBoxLayerWidget::BindButtonEvents()
{
	if (BackButton)
	{
		BackButton->OnClicked().AddUObject(this, &ULootBoxLayerWidget::OnBackButtonClicked);
	}

	if (OpenButton)
	{
		OpenButton->OnClicked().AddUObject(this, &ULootBoxLayerWidget::OnOpenButtonClicked);
	}

	if (PurchaseButton)
	{
		PurchaseButton->OnClicked().AddUObject(this, &ULootBoxLayerWidget::OnPurchaseButtonClicked);
	}
}

void ULootBoxLayerWidget::UnbindButtonEvents()
{
	if (BackButton)
	{
		BackButton->OnClicked().RemoveAll(this);
	}

	if (OpenButton)
	{
		OpenButton->OnClicked().RemoveAll(this);
	}

	if (PurchaseButton)
	{
		PurchaseButton->OnClicked().RemoveAll(this);
	}
}

void ULootBoxLayerWidget::OnBackButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[LootBoxLayerWidget] Back button clicked - returning to MainMap"));

	// 메인맵으로 복귀
	if (UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance()))
	{
		GameInstance->TransitionToLevel(TEXT("MainMap_TheRiverwalkCity"), 0);
	}
}

void ULootBoxLayerWidget::SetCategory(ELootBoxCategory Category)
{
	CurrentCategory = Category;

	// 카테고리 타이틀 업데이트
	if (CategoryTitleText)
	{
		FString CategoryDisplayName;
		switch (Category)
		{
		case ELootBoxCategory::BuildingSkin:
			CategoryDisplayName = TEXT("Building Skin Boxes");
			break;
		case ELootBoxCategory::BuildingItem:
			CategoryDisplayName = TEXT("Building Item Boxes");
			break;
		default:
			CategoryDisplayName = TEXT("Loot Boxes");
			break;
		}

		CategoryTitleText->SetText(FText::FromString(CategoryDisplayName));
	}

	// 목록 갱신
	RefreshLootBoxList();
}

void ULootBoxLayerWidget::RefreshLootBoxList()
{
	if (!LootBoxContainer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootBoxLayerWidget] LootBoxContainer is null"));
		return;
	}

	if (!TableMgr)
	{
		UE_LOG(LogTemp, Error, TEXT("[LootBoxLayerWidget] TableManager not found"));
		return;
	}

	if (!IconButtonClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootBoxLayerWidget] IconButtonClass not set"));
		return;
	}

	// 기존 리스트 클리어
	LootBoxContainer->ClearChildren();
	LootBoxButtons.Empty();
	CurrentSelectedButton = nullptr;

	// TableManager에서 현재 카테고리의 모든 룩박스 가져오기 (RowName 포함)
	TArray<TPair<FName, FLootBoxTable>> AllLootBoxes = TableMgr->GetLootBoxesByCategoryWithNames(CurrentCategory);

	// BuildingSkin이면 Sphere만, BuildingItem이면 Square만 필터링
	TArray<TPair<FName, FLootBoxTable>> LootBoxes;
	ELootBoxType TargetType = (CurrentCategory == ELootBoxCategory::BuildingSkin) ? ELootBoxType::Sphere : ELootBoxType::Square;

	for (const TPair<FName, FLootBoxTable>& Pair : AllLootBoxes)
	{
		if (Pair.Value.Type == TargetType)
		{
			LootBoxes.Add(Pair);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[LootBoxLayerWidget] Filtered %d/%d lootboxes for category %d (Type: %d)"),
		LootBoxes.Num(), AllLootBoxes.Num(), (int32)CurrentCategory, (int32)TargetType);

	USaveLoadManager* SaveLoadManager = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	TMap<FName, int32> OwnedCounts;

	if (SaveLoadManager)
	{
		if (USaveGame_GameData* SaveGame = SaveLoadManager->GetCurrentSaveData())
		{
			const FLootBoxInventoryData& Inventory = SaveGame->GameData.LootBoxInventory;
			if (Inventory.CategoryInventories.Contains(CurrentCategory))
			{
				OwnedCounts = Inventory.CategoryInventories[CurrentCategory].OwnedLootBoxes;
			}
		}
	}

	// 각 룩박스마다 IconButtonWidget 생성 (Blueprint 클래스 사용)
	for (const TPair<FName, FLootBoxTable>& Pair : LootBoxes)
	{
		FName LootBoxID = Pair.Key;
		const FLootBoxTable& LootBox = Pair.Value;

		UIconButtonWidget* Btn = CreateWidget<UIconButtonWidget>(this, IconButtonClass);
		if (!Btn)
		{
			UE_LOG(LogTemp, Error, TEXT("[LootBoxLayerWidget] Failed to create IconButtonWidget for %s"), *LootBoxID.ToString());
			continue;
		}

		Btn->SetIconFromSoft(LootBox.Icon);

		// 보유 개수 표시
		int32 OwnedCount = OwnedCounts.Contains(LootBoxID) ? OwnedCounts[LootBoxID] : 0;
		Btn->SetCount(OwnedCount);

		// 클릭 이벤트 바인딩 (람다로 LootBoxID 캡처)
		Btn->OnClicked().AddLambda([this, LootBoxID]()
		{
			OnLootBoxButtonClicked(LootBoxID);
		});

		// 맵에 저장 (나중에 개별 업데이트용)
		LootBoxButtons.Add(LootBoxID, Btn);

		// HorizontalBox에 추가하고 슬롯 설정
		UHorizontalBoxSlot* BoxSlot = LootBoxContainer->AddChildToHorizontalBox(Btn);
		if (BoxSlot)
		{
			BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			BoxSlot->SetHorizontalAlignment(HAlign_Center);
			BoxSlot->SetVerticalAlignment(VAlign_Center);
			BoxSlot->SetPadding(FMargin(5.0f));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[LootBoxLayerWidget] Refreshed %d lootboxes for category %d"),
		LootBoxes.Num(), (int32)CurrentCategory);

	// 첫 번째 상자를 기본으로 선택 (아무것도 선택 안 되어 있으면)
	if (LootBoxes.Num() > 0 && SelectedLootBoxID.IsNone())
	{
		FName FirstLootBoxID = LootBoxes[0].Key;
		OnLootBoxButtonClicked(FirstLootBoxID);
		UE_LOG(LogTemp, Log, TEXT("[LootBoxLayerWidget] Auto-selected first lootbox: %s"), *FirstLootBoxID.ToString());
	}
}

void ULootBoxLayerWidget::UpdateLootBoxCount(FName LootBoxID, int32 NewCount)
{
	// LootBoxButtons 맵에서 해당 버튼 찾기
	if (UIconButtonWidget** BtnPtr = LootBoxButtons.Find(LootBoxID))
	{
		if (*BtnPtr)
		{
			(*BtnPtr)->SetCount(NewCount);
			UE_LOG(LogTemp, Log, TEXT("[LootBoxLayerWidget] Updated count for %s: %d"), *LootBoxID.ToString(), NewCount);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootBoxLayerWidget] Button not found for %s"), *LootBoxID.ToString());
	}
}

void ULootBoxLayerWidget::OnLootBoxButtonClicked(FName LootBoxID)
{
	UE_LOG(LogTemp, Log, TEXT("[LootBoxLayerWidget] LootBox selected: %s"), *LootBoxID.ToString());

	// 선택된 룩박스 ID 저장
	SelectedLootBoxID = LootBoxID;

	// 이전 선택 해제
	if (CurrentSelectedButton)
	{
		CurrentSelectedButton->SetSelected(false);
	}

	// 새로 선택된 버튼 찾기
	if (UIconButtonWidget** BtnPtr = LootBoxButtons.Find(LootBoxID))
	{
		CurrentSelectedButton = *BtnPtr;
		if (CurrentSelectedButton)
		{
			CurrentSelectedButton->SetSelected(true);
		}
	}

	// 델리게이트 브로드캐스트 - 3D 모델 변경을 위해 외부에 알림
	OnLootBoxSelected.Broadcast(LootBoxID);
}

void ULootBoxLayerWidget::OnOpenButtonClicked()
{
	if (SelectedLootBoxID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootBoxLayerWidget] No lootbox selected"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[LootBoxLayerWidget] Opening LootBox: %s"), *SelectedLootBoxID.ToString());

	// SaveLoadManager에서 보유 개수 확인 (UI 숨기기 전에 검증!)
	USaveLoadManager* SaveLoadManager = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	if (!SaveLoadManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[LootBoxLayerWidget] SaveLoadManager not found"));
		return;
	}

	USaveGame_GameData* SaveGame = SaveLoadManager->GetCurrentSaveData();
	if (!SaveGame)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootBoxLayerWidget] No save data found"));
		return;
	}

	FLootBoxInventoryData& Inventory = SaveGame->GameData.LootBoxInventory;

	// 현재 카테고리 인벤토리 확인
	if (!Inventory.CategoryInventories.Contains(CurrentCategory))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootBoxLayerWidget] No inventory for category: %d"), (int32)CurrentCategory);
		return;
	}

	FLootBoxCategoryInventory& CategoryInv = Inventory.CategoryInventories[CurrentCategory];

	// 보유 개수 확인
	if (!CategoryInv.OwnedLootBoxes.Contains(SelectedLootBoxID) || CategoryInv.OwnedLootBoxes[SelectedLootBoxID] <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootBoxLayerWidget] No lootboxes to open: %s"), *SelectedLootBoxID.ToString());
		return;
	}

	// 개수 감소
	CategoryInv.OwnedLootBoxes[SelectedLootBoxID]--;
	int32 RemainingCount = CategoryInv.OwnedLootBoxes[SelectedLootBoxID];
	UE_LOG(LogTemp, Log, TEXT("[LootBoxLayerWidget] Opened LootBox: %s, Remaining: %d"),
		*SelectedLootBoxID.ToString(), RemainingCount);

	// 보상 추첨 및 획득
	int32 RewardItemID = RollReward(SelectedLootBoxID);
	if (RewardItemID > 0)
	{
		// BuildingSkin 카테고리면 OwnedBuildingSkins에 추가 (단일 저장소)
		if (CurrentCategory == ELootBoxCategory::BuildingSkin)
		{
			FBuildingSkinInstance NewSkin(RewardItemID);
			SaveGame->GameData.OwnedBuildingSkins.Add(NewSkin);
			UE_LOG(LogTemp, Log, TEXT("[LootBoxLayerWidget] Added BuildingSkin: %d"), RewardItemID);
		}

		UE_LOG(LogTemp, Log, TEXT("[LootBoxLayerWidget] Reward obtained: %d"), RewardItemID);

		SaveLoadManager->SaveGameData();
		HideUIForAnimation();
		OnLootBoxOpening.Broadcast(SelectedLootBoxID, RewardItemID);
	}

	UpdateLootBoxCount(SelectedLootBoxID, RemainingCount);
}

void ULootBoxLayerWidget::OnPurchaseButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[LootBoxLayerWidget] Purchasing LootBox (Gacha)"));

	// 1. 다이아몬드 10개 확인
	UResourceItemManager* RMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
	if (!RMgr)
	{
		UE_LOG(LogTemp, Error, TEXT("[LootBoxLayerWidget] ResourceItemManager not found"));
		return;
	}

	const int32 PurchasePrice = 10;
	if (!RMgr->HasResource(EResourceType::Diamond, PurchasePrice))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootBoxLayerWidget] Not enough diamonds! Need: %d"), PurchasePrice);
		if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowNotification(NSLOCTEXT("Notification", "NotEnoughDiamond", "다이아몬드가 부족합니다"), 3.0f, ENotificationType::Failed);
		}
		return;
	}

	// 2. 현재 카테고리의 모든 룩박스 가져오기
	if (!TableMgr)
	{
		UE_LOG(LogTemp, Error, TEXT("[LootBoxLayerWidget] TableManager not found"));
		return;
	}

	TArray<TPair<FName, FLootBoxTable>> AllLootBoxes = TableMgr->GetLootBoxesByCategoryWithNames(CurrentCategory);

	// BuildingSkin이면 Sphere만, BuildingItem이면 Square만 필터링 (RefreshLootBoxList와 동일)
	TArray<TPair<FName, FLootBoxTable>> LootBoxes;
	ELootBoxType TargetType = (CurrentCategory == ELootBoxCategory::BuildingSkin) ? ELootBoxType::Sphere : ELootBoxType::Square;

	for (const TPair<FName, FLootBoxTable>& Pair : AllLootBoxes)
	{
		if (Pair.Value.Type == TargetType)
		{
			LootBoxes.Add(Pair);
		}
	}

	if (LootBoxes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootBoxLayerWidget] No lootboxes in category %d (Type: %d)"),
			(int32)CurrentCategory, (int32)TargetType);
		return;
	}

	// 3. 희귀도별 가중치 계산
	TArray<int32> Weights;
	int32 TotalWeight = 0;

	for (const TPair<FName, FLootBoxTable>& Pair : LootBoxes)
	{
		int32 Weight = FLootBoxRarityUtility::GetWeight(Pair.Value.Rarity);
		Weights.Add(Weight);
		TotalWeight += Weight;
	}

	// 4. 랜덤 추첨
	int32 RandomValue = FMath::RandRange(0, TotalWeight - 1);
	int32 AccumulatedWeight = 0;
	int32 SelectedIndex = 0;

	for (int32 i = 0; i < Weights.Num(); ++i)
	{
		AccumulatedWeight += Weights[i];
		if (RandomValue < AccumulatedWeight)
		{
			SelectedIndex = i;
			break;
		}
	}

	FName WonLootBoxID = LootBoxes[SelectedIndex].Key;
	const FLootBoxTable& WonLootBox = LootBoxes[SelectedIndex].Value;

	UE_LOG(LogTemp, Log, TEXT("[LootBoxLayerWidget] Gacha Result: %s (Rarity: %d)"),
		*WonLootBoxID.ToString(), (int32)WonLootBox.Rarity);

	// 5. 다이아몬드 차감
	RMgr->SpendResource(EResourceType::Diamond, PurchasePrice);

	// 6. 인벤토리에 룩박스 추가
	USaveLoadManager* SaveLoadManager = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	if (!SaveLoadManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[LootBoxLayerWidget] SaveLoadManager not found"));
		return;
	}

	USaveGame_GameData* SaveGame = SaveLoadManager->GetCurrentSaveData();
	if (!SaveGame)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootBoxLayerWidget] No save data found"));
		return;
	}

	FLootBoxInventoryData& Inventory = SaveGame->GameData.LootBoxInventory;

	// 카테고리 인벤토리 초기화 (없으면)
	if (!Inventory.CategoryInventories.Contains(CurrentCategory))
	{
		Inventory.CategoryInventories.Add(CurrentCategory, FLootBoxCategoryInventory());
	}

	FLootBoxCategoryInventory& CategoryInv = Inventory.CategoryInventories[CurrentCategory];

	// 룩박스 개수 증가
	if (!CategoryInv.OwnedLootBoxes.Contains(WonLootBoxID))
	{
		CategoryInv.OwnedLootBoxes.Add(WonLootBoxID, 0);
	}
	CategoryInv.OwnedLootBoxes[WonLootBoxID]++;
	int32 NewCount = CategoryInv.OwnedLootBoxes[WonLootBoxID];

	UE_LOG(LogTemp, Log, TEXT("[LootBoxLayerWidget] Added LootBox: %s, Total: %d"),
		*WonLootBoxID.ToString(), NewCount);

	// 7. 저장
	SaveLoadManager->SaveGameData();

	// TODO: 획득 연출 UI 표시
	// 예: "Epic LootBox 획득!" 팝업

	// UI 갱신 (개별 버튼만 업데이트 - 효율적)
	UpdateLootBoxCount(WonLootBoxID, NewCount);
}

int32 ULootBoxLayerWidget::RollReward(FName LootBoxID)
{
	if (!TableMgr)
	{
		UE_LOG(LogTemp, Error, TEXT("[LootBoxLayerWidget] TableManager not found"));
		return 0;
	}

	// 룩박스 정보 가져오기
	bool bSuccess = false;
	FLootBoxTable LootBoxData = TableMgr->GetLootBoxData(LootBoxID, bSuccess);
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootBoxLayerWidget] LootBox data not found: %s"), *LootBoxID.ToString());
		return 0;
	}

	// 해당 희귀도의 BuildingSkin 목록 가져오기
	TArray<FBuildingSkinData> AvailableSkins = TableMgr->GetBuildingSkinsByRarity(LootBoxData.Rarity);

	if (AvailableSkins.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootBoxLayerWidget] No skins available for rarity: %d"), (int32)LootBoxData.Rarity);
		return 0;
	}

	// 랜덤으로 스킨 선택
	int32 RandomIndex = FMath::RandRange(0, AvailableSkins.Num() - 1);
	int32 RewardID = AvailableSkins[RandomIndex].SkinID;

	UE_LOG(LogTemp, Log, TEXT("[LootBoxLayerWidget] Rolled reward ID: %d from %s (Selected %d out of %d available skins)"),
		RewardID, *LootBoxID.ToString(), RandomIndex + 1, AvailableSkins.Num());

	return RewardID;
}

void ULootBoxLayerWidget::OnLootBoxOpened(FName LootBoxID)
{
	UE_LOG(LogTemp, Log, TEXT("[LootBoxLayerWidget] Opening LootBox: %s"), *LootBoxID.ToString());

	// TODO: 룩박스 오픈 로직
	// 1. 인벤토리에서 개수 감소
	// 2. 보상 추첨
	// 3. 보상 획득 처리
	// 4. UI 갱신

	// 임시로 목록만 갱신
	RefreshLootBoxList();
}

void ULootBoxLayerWidget::HideUIForAnimation()
{
	if (OpenButton)
	{
		OpenButton->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (PurchaseButton)
	{
		PurchaseButton->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (LootBoxContainer)
	{
		LootBoxContainer->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 위젯의 입력만 차단 (비활성화는 하지 않음)
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetIsFocusable(false);
}

void ULootBoxLayerWidget::ShowUIAfterAnimation()
{
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);

	if (OpenButton)
	{
		OpenButton->SetVisibility(ESlateVisibility::Visible);
	}

	if (PurchaseButton)
	{
		PurchaseButton->SetVisibility(ESlateVisibility::Visible);
	}

	if (LootBoxContainer)
	{
		LootBoxContainer->SetVisibility(ESlateVisibility::Visible);
	}
}
