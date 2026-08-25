// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/WorldMapLayerWidget.h"
#include "UI/Panel/CountryDetailWidget.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "UI/Element/Trade/CountryNameWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/UIBase.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/CountryInfoTable.h"
#include "Enum/WidgetType.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

void UWorldMapLayerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UGameInstance* GI = GetWorld()->GetGameInstance();
	UResourceItemManager* RMgr = GI->GetSubsystem<UResourceItemManager>();

	// 자원 위젯 등록 + 현재 값으로 초기화
	auto RegisterResource = [&](UResourceWidget* Widget, EResourceType Type)
	{
		if (!Widget) return;
		ResourceWidgets.Add(Type, Widget);
		if (RMgr)
		{
			Widget->SetValue(RMgr->GetResourceAmount(Type));
		}
	};

	RegisterResource(UIE_Resource_Money, EResourceType::Money);
	RegisterResource(UIE_Resource_Brick, EResourceType::Brick);
	RegisterResource(UIE_Resource_Diamond, EResourceType::Diamond);

	// 자원 변경 구독
	if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
	{
		UIResourceChangedHandle = UIMgr->OnUIResourceChanged.AddUObject(
			this, &UWorldMapLayerWidget::HandleResourceChanged);
	}

	// 나라 이름표 생성 (다음 프레임에 — CountryActor가 BeginPlay 완료 후)
	GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
	{
		CreateCountryNameWidgets();
	});

	TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
}

void UWorldMapLayerWidget::NativeDestruct()
{
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->OnUIResourceChanged.Remove(UIResourceChangedHandle);
		}
	}

	ResourceWidgets.Empty();
	CountryNameDatas.Empty();

	Super::NativeDestruct();
}

void UWorldMapLayerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateCountryNamePositions();
}

void UWorldMapLayerWidget::HandleResourceChanged(EResourceType Type, int64 NewValue)
{
	if (UResourceWidget** FoundWidget = ResourceWidgets.Find(Type))
	{
		if (*FoundWidget)
		{
			(*FoundWidget)->SetValue(NewValue);
		}
	}
}

void UWorldMapLayerWidget::CreateCountryNameWidgets()
{
	if (!InGameCanvas) return;

	UGameInstance* GI = GetWorld()->GetGameInstance();
	if (!GI) return;

	UTableManagerSubsystem* LocalTableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!LocalTableMgr) return;

	UResourceItemManager* LocalRMgr = GI->GetSubsystem<UResourceItemManager>();

	TSubclassOf<UUserWidget> WidgetClass = LocalTableMgr->GetWidgetClass(EWidgetType::CountryName);
	if (!WidgetClass) return;

	// 레벨의 모든 CountryActor 찾기
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACountryActor::StaticClass(), FoundActors);

	for (AActor* Actor : FoundActors)
	{
		ACountryActor* Country = Cast<ACountryActor>(Actor);
		if (!Country || Country->CountryType == ECountryType::None) continue;

		// 위젯 생성
		UCountryNameWidget* NameWidget = CreateWidget<UCountryNameWidget>(GetWorld(), WidgetClass);
		if (!NameWidget) continue;

		// Canvas에 추가
		UCanvasPanelSlot* CanvasSlot = InGameCanvas->AddChildToCanvas(NameWidget);
		if (!CanvasSlot)
		{
			InGameCanvas->RemoveChild(NameWidget);
			continue;
		}

		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetAlignment(FVector2D(0.0f, 1.0f));
		CanvasSlot->SetPosition(FVector2D::ZeroVector);

		// 국기 설정 + 나라 타입 + 시가총액 잠금 판정
		NameWidget->SetCountryType(Country->CountryType);
		if (Country->CachedFlagIcon)
		{
			NameWidget->SetFlagTexture(Country->CachedFlagIcon);
		}
		bool bInfoOk = false;
		FCountryInfoTable Info = LocalTableMgr->GetCountryInfo(Country->CountryType, bInfoOk);
		if (bInfoOk)
		{
			const int64 PlayerRep = LocalRMgr ? LocalRMgr->GetResourceAmount(EResourceType::MarketCap) : 0;
			const bool bLocked = PlayerRep < Info.RequiredMarketCap;
			const FText Reason = FText::Format(
				NSLOCTEXT("WorldMap", "NeedMarketCap", "시가총액 {0}"),
				FText::AsNumber(Info.RequiredMarketCap));
			NameWidget->SetLockState(bLocked, Reason);
		}

		// 클릭 콜백
		NameWidget->OnFlagClicked.BindUObject(this, &UWorldMapLayerWidget::HandleCountryFlagClicked);

		FCountryNameData Data;
		Data.CountryActor = Country;
		Data.NameWidget = NameWidget;
		CountryNameDatas.Add(Data);
	}

	UE_LOG(LogTemp, Log, TEXT("[WorldMapLayerWidget] Created %d country name widgets"), CountryNameDatas.Num());
}

void UWorldMapLayerWidget::HandleCountryFlagClicked(ECountryType CountryType)
{
	if (CountryType == ECountryType::None || !TableMgr) return;

	UUIManagerSubsystem* UIMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	if (!UIMgr) return;
	UUIBase* UIBase = UIMgr->GetUIBase();
	if (!UIBase) return;

	// 카메라 포커스
	if (ACountryActor* CountryActor = ACountryActor::FindCountryActor(this, CountryType))
	{
		CountryActor->RequestFocus(true);
	}

	// CountryDetail 패널 푸시 + 나라 지정
	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::CountryDetail);
	if (!Cls) return;

	UCommonActivatableWidget* Pushed = UIBase->PushPromptClass(TSubclassOf<UCommonActivatableWidget>(Cls));
	if (UCountryDetailWidget* Detail = Cast<UCountryDetailWidget>(Pushed))
	{
		Detail->SetCountry(CountryType);
	}
}

void UWorldMapLayerWidget::UpdateCountryNamePositions()
{
	if (!InGameCanvas || CountryNameDatas.Num() == 0) return;

	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;

	// 프레임 불변 값은 국가 루프 밖에서 1회만 계산 (이전엔 국가마다 PC/Viewport/Canvas geometry 재조회)
	const FGeometry ViewportGeo = UWidgetLayoutLibrary::GetViewportWidgetGeometry(World);
	const FGeometry CanvasGeo = InGameCanvas->GetCachedGeometry();

	for (FCountryNameData& Data : CountryNameDatas)
	{
		if (!Data.CountryActor || !Data.NameWidget) continue;

		FVector2D ViewportPos;
		const bool bProjected = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
			PC, Data.CountryActor->GetActorLocation(), ViewportPos, true);

		FVector2D CanvasPos(-9999.0f, -9999.0f);
		if (bProjected)
		{
			const FVector2D AbsolutePos = ViewportGeo.LocalToAbsolute(ViewportPos);
			CanvasPos = CanvasGeo.AbsoluteToLocal(AbsolutePos);
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Data.NameWidget->Slot))
			{
				CanvasSlot->SetPosition(CanvasPos);
			}
		}

		const bool bOnScreen = (CanvasPos.X > -500.0f && CanvasPos.Y > -500.0f);
		Data.NameWidget->SetVisibility(bOnScreen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}
