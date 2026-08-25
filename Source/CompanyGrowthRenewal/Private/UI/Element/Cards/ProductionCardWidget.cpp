// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Cards/ProductionCardWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Common/StatRowWidget.h"
#include "UI/Element/Cards/ItemCardSlotWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Table/ProductRecipeTable.h"
#include "Table/ProjectDataTable.h"
#include "Table/ResourceInfo.h"
#include "Enum/WidgetType.h"
#include "CommonTextBlock.h"
#include "CommonButtonBase.h"
#include "Components/Image.h"
#include "Components/WrapBox.h"
#include "Components/EditableTextBox.h"
#include "Engine/Texture2D.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

namespace
{
	FString FormatProductionTime(float TotalSeconds)
	{
		const int32 Total = FMath::CeilToInt(FMath::Max(0.0f, TotalSeconds));
		if (Total < 60)
		{
			return FString::Printf(TEXT("%ds"), Total);
		}
		if (Total < 3600)
		{
			const int32 Mins = Total / 60;
			const int32 Secs = Total % 60;
			return Secs > 0
				? FString::Printf(TEXT("%dm %ds"), Mins, Secs)
				: FString::Printf(TEXT("%dm"), Mins);
		}
		const int32 Hrs = Total / 3600;
		const int32 RemMins = (Total % 3600) / 60;
		return RemMins > 0
			? FString::Printf(TEXT("%dh %dm"), Hrs, RemMins)
			: FString::Printf(TEXT("%dh"), Hrs);
	}
}

void UProductionCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (MinusButton)        MinusButton->OnClicked().AddUObject(this, &UProductionCardWidget::HandleMinus);
	if (PlusButton)         PlusButton->OnClicked().AddUObject(this, &UProductionCardWidget::HandlePlus);
	if (AcceptButton)       AcceptButton->OnClicked().AddUObject(this, &UProductionCardWidget::HandleAccept);
	if (NumberEditableTextBox)   NumberEditableTextBox->OnTextCommitted.AddDynamic(this, &UProductionCardWidget::HandleQuantityCommitted);

	// 자식 위젯들(StatRow, ItemCard 등) 의 NativeConstruct 가 디자인 시간 default 값으로 덮어쓰는 것을 방지
	// 다음 틱에 캐시된 데이터 재적용
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UProductionCardWidget> WeakThis(this);
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis]()
		{
			if (UProductionCardWidget* Strong = WeakThis.Get())
			{
				Strong->RefreshQuantityUI();
			}
		}));
	}
}

void UProductionCardWidget::NativeDestruct()
{
	if (MinusButton)        MinusButton->OnClicked().RemoveAll(this);
	if (PlusButton)         PlusButton->OnClicked().RemoveAll(this);
	if (AcceptButton)       AcceptButton->OnClicked().RemoveAll(this);
	if (NumberEditableTextBox)   NumberEditableTextBox->OnTextCommitted.RemoveDynamic(this, &UProductionCardWidget::HandleQuantityCommitted);

	Super::NativeDestruct();
}

void UProductionCardWidget::SetOrderData(const FProductionOrder& InOrder)
{
	CachedOrder = InOrder;
	CurrentQuantity = (CachedOrder.RemainingQuantity > 0) ? 1 : 0;
	RefreshAll();
}

void UProductionCardWidget::RefreshAll()
{
	// DT_Project_* 단일 진실 — Order 에 박제된 ProductName 은 stage 시점 stale 값이라 사용 X.
	// 이름과 이미지(LoadStageImageAsync) 둘 다 같은 row 에서 라이브 조회해야 일치 보장.
	FText DisplayName = FText::FromString(CachedOrder.ProductName);
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
		{
			bool bOk = false;
			const FProjectData Data = TableMgr->GetProjectData(CachedOrder.CompanyType, CachedOrder.ProjectIndex, bOk);
			if (bOk && !Data.ProjectName.IsEmpty())
			{
				DisplayName = Data.ProjectName;
			}
		}
	}
	if (ProductionNameText)
	{
		ProductionNameText->SetText(DisplayName);
	}
	LoadStageImageAsync();
	RebuildMaterials();
	RefreshQuantityUI();
}

void UProductionCardWidget::RebuildMaterials()
{
	if (!ItemWrapBox) return;

	ItemWrapBox->ClearChildren();
	SpawnedMaterialCards.Reset();
	CachedMaterials.Reset();
	CachedProductionTimeSec = 0.0f;
	bRecipeMissing = false;

	UGameInstance* GI = GetGameInstance();
	if (!GI) return;
	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;

	auto Push = [this](EResourceType Type, int32 Amount)
	{
		if (Amount > 0)
		{
			CachedMaterials.Emplace(Type, Amount);
		}
	};

	bool bRecipeOk = false;
	const FProductRecipeTable Recipe = TableMgr->GetProductRecipe(
		CachedOrder.CompanyType, CachedOrder.ProjectIndex, bRecipeOk);

	if (bRecipeOk)
	{
		CachedProductionTimeSec = Recipe.ProductionTimeSec;
		Push(EResourceType::IronOre,    Recipe.IronOre);
		Push(EResourceType::Copper,     Recipe.Copper);
		Push(EResourceType::Silicon,    Recipe.Silicon);
		Push(EResourceType::Lithium,    Recipe.Lithium);
		Push(EResourceType::Oil,        Recipe.Oil);
		Push(EResourceType::RareEarth,  Recipe.RareEarth);
		Push(EResourceType::Aluminum,   Recipe.Aluminum);
		Push(EResourceType::Wood,       Recipe.Wood);
		Push(EResourceType::Gold,       Recipe.GoldOre);
		Push(EResourceType::DiamondOre, Recipe.DiamondOre);
	}

	// Recipe 누락 시 loud failure + 어뷰즈 차단 플래그
	if (CachedMaterials.Num() == 0)
	{
		bRecipeMissing = true;
		UE_LOG(LogTemp, Warning,
			TEXT("[ProductionCard] DT_Recipe 누락 — CompanyType=%d, ProjectIndex=%d. CSV reimport 또는 Recipe row 추가 필요"),
			static_cast<int32>(CachedOrder.CompanyType), CachedOrder.ProjectIndex);
		return;
	}

	TSubclassOf<UUserWidget> CardClass = TableMgr->GetWidgetClass(EWidgetType::ItemCardQtyInside);
	if (!CardClass) return;

	UResourceItemManager* ResMgr = GI->GetSubsystem<UResourceItemManager>();
	const int32 Qty = FMath::Max(0, CurrentQuantity);
	for (const TPair<EResourceType, int32>& Pair : CachedMaterials)
	{
		UItemCardSlotWidget* Card = CreateWidget<UItemCardSlotWidget>(this, CardClass);
		if (!Card) continue;

		bool bResOk = false;
		const FResourceInfo Info = TableMgr->GetResourceInfo(Pair.Key, bResOk);
		UTexture2D* Icon = nullptr;
		if (bResOk && !Info.Icon.IsNull())
		{
			Icon = Info.Icon.LoadSynchronous();
		}
		const int64 Need = static_cast<int64>(Pair.Value) * Qty;
		Card->SetItem(Icon, static_cast<int32>(Need));

		const int64 Have = ResMgr ? ResMgr->GetResourceAmount(Pair.Key) : 0;
		Card->SetInsufficient(Qty > 0 && Have < Need);

		// 클릭 → 부모 popup 으로 bubble up. 카드 우측 중앙을 absolute 좌표로 환산해서 전달
		const EResourceType ResType = Pair.Key;
		TWeakObjectPtr<UItemCardSlotWidget> WeakCard(Card);
		TWeakObjectPtr<UProductionCardWidget> WeakSelf(this);
		Card->OnItemCardClicked.BindLambda([WeakCard, WeakSelf, ResType](FName /*ItemID*/)
		{
			UItemCardSlotWidget* StrongCard = WeakCard.Get();
			UProductionCardWidget* StrongSelf = WeakSelf.Get();
			if (!StrongCard || !StrongSelf) return;

			const FGeometry CardGeo = StrongCard->GetCachedGeometry();
			const FVector2D LocalRightCenter(CardGeo.GetLocalSize().X, CardGeo.GetLocalSize().Y * 0.5f);
			const FVector2D AbsPos = CardGeo.LocalToAbsolute(LocalRightCenter);
			StrongSelf->OnMaterialClicked.ExecuteIfBound(ResType, AbsPos);
		});

		ItemWrapBox->AddChild(Card);
		SpawnedMaterialCards.Add(Card);
	}
}

void UProductionCardWidget::RefreshMaterialQuantities()
{
	// 카드 갱신은 Num 일치할 때만, 버튼 상태 갱신은 항상 (자원 변화는 카드와 무관히 affordability 결정)
	if (SpawnedMaterialCards.Num() == CachedMaterials.Num())
	{
		UResourceItemManager* ResMgr = nullptr;
		if (UGameInstance* GI = GetGameInstance())
		{
			ResMgr = GI->GetSubsystem<UResourceItemManager>();
		}

		const int32 Qty = FMath::Max(0, CurrentQuantity);
		for (int32 i = 0; i < SpawnedMaterialCards.Num(); ++i)
		{
			UItemCardSlotWidget* Card = SpawnedMaterialCards[i];
			if (!Card) continue;

			const int64 Need = static_cast<int64>(CachedMaterials[i].Value) * Qty;
			Card->SetQuantity(static_cast<int32>(Need));

			const int64 Have = ResMgr ? ResMgr->GetResourceAmount(CachedMaterials[i].Key) : 0;
			Card->SetInsufficient(Qty > 0 && Have < Need);
		}
	}

	UpdateAcceptButtonState();
}

bool UProductionCardWidget::HasEnoughMaterials() const
{
	if (CurrentQuantity <= 0) return false;
	if (bRecipeMissing) return false; // Recipe 누락 → 어뷰즈 방지 위해 fail-closed
	if (CachedMaterials.Num() == 0) return true; // Recipe 명시적으로 재료 0 — 무료 주문 (의도된 디자인)

	UGameInstance* GI = GetGameInstance();
	UResourceItemManager* ResMgr = GI ? GI->GetSubsystem<UResourceItemManager>() : nullptr;
	if (!ResMgr) return false;

	for (const TPair<EResourceType, int32>& M : CachedMaterials)
	{
		const int64 Need = static_cast<int64>(M.Value) * static_cast<int64>(CurrentQuantity);
		if (ResMgr->GetResourceAmount(M.Key) < Need)
		{
			return false;
		}
	}
	return true;
}

void UProductionCardWidget::UpdateAcceptButtonState()
{
	if (!AcceptButton) return;

	// 시각만 swap. 클릭은 항상 가능 — 막힌 상태에서 클릭하면 HandleAccept 가 toast 로 사유 알림.
	const bool bCanProduce = HasEnoughMaterials() && bLineCapacityAvailable;
	TSubclassOf<UCommonButtonStyle> Target = bCanProduce ? AcceptButtonStyle_Enabled : AcceptButtonStyle_Disabled;
	if (Target)
	{
		AcceptButton->SetStyle(Target);
	}
}

void UProductionCardWidget::SetLineCapacityAvailable(bool bAvailable)
{
	if (bLineCapacityAvailable == bAvailable) return;
	bLineCapacityAvailable = bAvailable;
	UpdateAcceptButtonState();
}

void UProductionCardWidget::ClampQuantity()
{
	const int32 Max = FMath::Max(0, CachedOrder.RemainingQuantity);
	CurrentQuantity = FMath::Clamp(CurrentQuantity, Max > 0 ? 1 : 0, Max);
}

void UProductionCardWidget::RefreshQuantityUI()
{
	if (NumberEditableTextBox)
	{
		NumberEditableTextBox->SetText(FText::AsNumber(CurrentQuantity));
	}
	if (Text_Time)
	{
		const float TotalSec = CachedProductionTimeSec * FMath::Max(0, CurrentQuantity);
		Text_Time->SetStatValue(FormatProductionTime(TotalSec));
	}
	RefreshMaterialQuantities();
}

void UProductionCardWidget::HandleMinus()
{
	--CurrentQuantity;
	ClampQuantity();
	RefreshQuantityUI();
}

void UProductionCardWidget::HandlePlus()
{
	++CurrentQuantity;
	ClampQuantity();
	RefreshQuantityUI();
}

void UProductionCardWidget::HandleAccept()
{
	if (CachedOrder.OrderID == 0 || CurrentQuantity <= 0) return;

	UGameInstance* GI = GetGameInstance();
	UUIManagerSubsystem* UIMgr = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;

	if (!HasEnoughMaterials())
	{
		if (UIMgr)
		{
			UIMgr->ShowNotification(
				NSLOCTEXT("ProductionCard", "InsufficientMaterials", "재료가 부족합니다"),
				2.5f, ENotificationType::Failed);
		}
		return;
	}
	if (!bLineCapacityAvailable)
	{
		if (UIMgr)
		{
			UIMgr->ShowNotification(
				NSLOCTEXT("ProductionCard", "LinesFull", "생산 라인이 가득 찼습니다"),
				2.5f, ENotificationType::Warning);
		}
		return;
	}

	// 재료 일괄 차감 (배치 단위)
	if (GI)
	{
		if (UResourceItemManager* ResMgr = GI->GetSubsystem<UResourceItemManager>())
		{
			for (const TPair<EResourceType, int32>& M : CachedMaterials)
			{
				const int64 Need = static_cast<int64>(M.Value) * static_cast<int64>(CurrentQuantity);
				if (Need > 0)
				{
					ResMgr->SpendResource(M.Key, Need, /*bShouldSave=*/true);
				}
			}
		}
	}

	OnProduceRequested.ExecuteIfBound(CachedOrder.OrderID, CurrentQuantity, CachedProductionTimeSec);
}

void UProductionCardWidget::RefundMaterials(int32 Quantity)
{
	if (Quantity <= 0 || CachedMaterials.Num() == 0) return;

	UGameInstance* GI = GetGameInstance();
	if (!GI) return;
	UResourceItemManager* ResMgr = GI->GetSubsystem<UResourceItemManager>();
	if (!ResMgr) return;

	for (const TPair<EResourceType, int32>& M : CachedMaterials)
	{
		const int64 Refund = static_cast<int64>(M.Value) * static_cast<int64>(Quantity);
		if (Refund > 0)
		{
			ResMgr->StoreResource(M.Key, Refund, /*bShouldSave=*/true);
		}
	}
}

void UProductionCardWidget::HandleQuantityCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (CommitType != ETextCommit::OnEnter && CommitType != ETextCommit::OnUserMovedFocus) return;
	CurrentQuantity = FCString::Atoi(*Text.ToString());
	ClampQuantity();
	RefreshQuantityUI();
}

void UProductionCardWidget::LoadStageImageAsync()
{
	if (!EntityImage || CachedOrder.ProjectIndex <= 0) return;

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return;

	bool bOk = false;
	const FProjectData Data = TableMgr->GetProjectData(CachedOrder.CompanyType, CachedOrder.ProjectIndex, bOk);
	if (!bOk || Data.Icon.IsNull()) return;

	const TSoftObjectPtr<UTexture2D> SoftTex = Data.Icon;

	if (UTexture2D* Already = SoftTex.Get())
	{
		EntityImage->SetBrushFromTexture(Already);
		return;
	}

	TWeakObjectPtr<UProductionCardWidget> WeakThis(this);
	UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftTex.ToSoftObjectPath(),
		FStreamableDelegate::CreateLambda([WeakThis, SoftTex]()
		{
			if (UProductionCardWidget* Strong = WeakThis.Get())
			{
				if (UTexture2D* Tex = SoftTex.Get())
				{
					if (Strong->EntityImage)
					{
						Strong->EntityImage->SetBrushFromTexture(Tex);
					}
				}
			}
		}));
}
