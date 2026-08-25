// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Office/OfficeDecorationCardWidget.h"
#include "UI/Element/Cards/EntityCardFrameWidget.h"
#include "Data/EntityCardData.h"
#include "UI/Element/Cards/CardInfoWidget.h"
#include "UI/Panel/BuildPlacementPanelWidget.h"
#include "UI/UIBase.h"
#include "Office/OfficeManager.h"
#include "Office/OfficeInterior.h"
#include "Core/CGGameInstance.h"
#include "Player/OfficePlayerController.h"
#include "Player/OfficeCameraPawn.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Enum/WidgetType.h"
#include "Enum/ResourceType.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "CommonTextBlock.h"
#include "Engine/AssetManager.h"
#include "Kismet/GameplayStatics.h"

void UOfficeDecorationCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CacheReferences();
}

void UOfficeDecorationCardWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

void UOfficeDecorationCardWidget::CacheReferences()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// OfficeManager 찾기 (WorldSubsystem)
	CachedOfficeManager = World->GetSubsystem<UOfficeManager>();

	// OfficeInterior 찾기
	CachedOfficeInterior = Cast<AOfficeInterior>(UGameplayStatics::GetActorOfClass(World, AOfficeInterior::StaticClass()));
}

void UOfficeDecorationCardWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	// ListView에서 데이터가 설정될 때 호출됨 (스크롤 시 재활용될 때마다)
	if (UEntityCardData* CardData = Cast<UEntityCardData>(ListItemObject))
	{
		SetDecorationInfo(CardData->DecorationInfo);
	}
}

void UOfficeDecorationCardWidget::SetDecorationInfo(const FDecorationCardTable& InDecorationInfo)
{
	DecorationInfo = InDecorationInfo;

	// CardInfoWidget에 정보 설정
	if (CardInfoWidget)
	{
		CardInfoWidget->SetDecorationInfo(DecorationInfo);
	}

	// 아이콘 비동기 로드 (IsNull로 경로 존재 여부 체크)
	if (!DecorationInfo.UIIcon.IsNull())
	{
		const FSoftObjectPath IconPath = DecorationInfo.UIIcon.ToSoftObjectPath();
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			IconPath,
			FStreamableDelegate::CreateUObject(this, &UEntityCardWidgetBase::OnIconLoaded, IconPath)
		);
	}

	// 잠금 조건 체크
	CheckUnlockCondition();

	UE_LOG(LogTemp, Log, TEXT("[OfficeDecorationCardWidget] SetDecorationInfo - Name: %s, Category: %d"),
		*DecorationInfo.Name.ToString(), (int32)DecorationInfo.Category);
}

void UOfficeDecorationCardWidget::CheckUnlockCondition()
{
	bool bIsLocked = false;
	FText LockReason = FText::GetEmpty();

	// 프리미엄 아이템 체크
	if (DecorationInfo.bIsPremium)
	{
		// TODO: 프리미엄 구매 여부 체크
		// 현재는 모두 해금된 것으로 처리
	}

	// 레벨 조건 체크
	if (DecorationInfo.UnlockLevel > 1)
	{
		// TODO: 플레이어 레벨과 비교
		// 현재는 모두 해금된 것으로 처리
	}

	UpdateLockUI(bIsLocked, LockReason);
}

void UOfficeDecorationCardWidget::HandleCardClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeDecorationCardWidget] HandleCardClicked - Category: %d, Surface: %d"),
		(int32)DecorationInfo.Category, (int32)DecorationInfo.AllowedSurface);

	// 바닥 타일 카테고리는 즉시 적용 (비용 없음 or 별도 처리)
	if (DecorationInfo.Category == EDecorationCategory::FloorTile)
	{
		if (CachedOfficeInterior)
		{
			CachedOfficeInterior->LoadFloorTileFromRowName(DecorationInfo.RowName);
			UE_LOG(LogTemp, Log, TEXT("[OfficeDecorationCardWidget] Floor tile applied: %s"), *DecorationInfo.RowName.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[OfficeDecorationCardWidget] OfficeInterior not found"));
		}
		OnDecorationCardSelected.Broadcast(DecorationInfo);
		return;
	}

	// Grid 배치 장식품 (가구, 의자, 화분, 파티션, 벽)
	if (DecorationInfo.AllowedSurface == EDecorationSurface::Grid)
	{
		UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
		if (!GameInstance)
		{
			UE_LOG(LogTemp, Error, TEXT("[OfficeDecorationCardWidget] GameInstance not found!"));
			return;
		}

		// 비용 체크
		UResourceItemManager* ResourceMgr = GameInstance->GetSubsystem<UResourceItemManager>();
		if (ResourceMgr && DecorationInfo.Price > 0)
		{
			int64 CurrentMoney = ResourceMgr->GetResourceAmount(EResourceType::Money);
			if (CurrentMoney < DecorationInfo.Price)
			{
				UE_LOG(LogTemp, Warning, TEXT("[OfficeDecorationCardWidget] Not enough money! Need: %d, Have: %lld"),
					DecorationInfo.Price, CurrentMoney);
				if (UUIManagerSubsystem* UIMgr = GameInstance->GetSubsystem<UUIManagerSubsystem>())
				{
					UIMgr->ShowNotification(NSLOCTEXT("Notification", "NotEnoughMoney", "자금이 부족합니다"), 3.0f, ENotificationType::Failed);
				}
				return;
			}
		}

		// OfficeCameraPawn 가져오기
		AOfficePlayerController* PC = Cast<AOfficePlayerController>(GameInstance->GetCurrentPlayerController());
		if (!PC)
		{
			UE_LOG(LogTemp, Error, TEXT("[OfficeDecorationCardWidget] OfficePlayerController not found!"));
			return;
		}

		AOfficeCameraPawn* OfficePawn = Cast<AOfficeCameraPawn>(PC->GetPawn());
		if (!OfficePawn)
		{
			UE_LOG(LogTemp, Error, TEXT("[OfficeDecorationCardWidget] OfficeCameraPawn not found!"));
			return;
		}

		// 장식품 배치 모드 시작
		OfficePawn->BeginDecorationPlacement(DecorationInfo);

		// BuildPlacementPanelWidget 열기
		UTableManagerSubsystem* TableManager = GameInstance->GetSubsystem<UTableManagerSubsystem>();
		UUIManagerSubsystem* UIManager = GameInstance->GetSubsystem<UUIManagerSubsystem>();

		if (TableManager && UIManager)
		{
			TSubclassOf<UUserWidget> PlacementWidgetClass = TableManager->GetWidgetClass(EWidgetType::BuildPlacementPanel);

			// 카탈로그를 닫으면 이 카드는 위젯 풀로 반납된다 — 멤버를 더 읽지 않도록 먼저 복사.
			const FDecorationCardTable InfoCopy = DecorationInfo;

			UUIBase* UIBase = UIManager->GetUIBase();

			// 카탈로그를 배치 바 아래에 깔아두지 않는다 — 깔아두면 배치 종료 시 되살아난다. 닫기 먼저, push 나중.
			UIBase->PopBottomWidget();

			UBuildPlacementPanelWidget* PlacementWidget = Cast<UBuildPlacementPanelWidget>(
				UIBase->PushBottomClass(PlacementWidgetClass.Get()));

			if (PlacementWidget)
			{
				PlacementWidget->SetDecorationInfo(InfoCopy);
				UE_LOG(LogTemp, Log, TEXT("[OfficeDecorationCardWidget] BuildPlacementPanelWidget opened with decoration info"));
			}
		}
	}
	else if (DecorationInfo.AllowedSurface == EDecorationSurface::Wall)
	{
		// 벽 장식 (그림, 창문)은 OfficeManager를 통해 처리
		if (CachedOfficeManager)
		{
			// 비용 체크 — Grid 분기와 같은 지점에서 막는다. 모드에 들여보낸 뒤 확정에서 거절하면 위치 잡던 게 헛수고가 된다.
			if (UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
			{
				if (DecorationInfo.Price > 0 && ResourceMgr->GetResourceAmount(EResourceType::Money) < DecorationInfo.Price)
				{
					if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
					{
						UIMgr->ShowNotification(NSLOCTEXT("Notification", "NotEnoughMoney", "자금이 부족합니다"), 3.0f, ENotificationType::Failed);
					}
					return;
				}
			}

			CachedOfficeManager->OnDecorationItemSelected(DecorationInfo);

			// BuildPlacementPanelWidget 열기 (벽 장식품도 확인/취소 버튼 필요)
			UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
			if (GameInstance)
			{
				UTableManagerSubsystem* TableManager = GameInstance->GetSubsystem<UTableManagerSubsystem>();
				UUIManagerSubsystem* UIManager = GameInstance->GetSubsystem<UUIManagerSubsystem>();

				if (TableManager && UIManager)
				{
					TSubclassOf<UUserWidget> PlacementWidgetClass = TableManager->GetWidgetClass(EWidgetType::BuildPlacementPanel);

					// 카탈로그를 닫으면 이 카드는 위젯 풀로 반납된다 — 멤버를 더 읽지 않도록 먼저 복사.
					const FDecorationCardTable InfoCopy = DecorationInfo;

					UUIBase* UIBase = UIManager->GetUIBase();

					// 카탈로그를 배치 바 아래에 깔아두지 않는다 — 깔아두면 배치 종료 시 되살아난다. 닫기 먼저, push 나중.
					UIBase->PopBottomWidget();

					UBuildPlacementPanelWidget* PlacementWidget = Cast<UBuildPlacementPanelWidget>(
						UIBase->PushBottomClass(PlacementWidgetClass.Get()));

					if (PlacementWidget)
					{
						PlacementWidget->SetDecorationInfo(InfoCopy);
						UE_LOG(LogTemp, Log, TEXT("[OfficeDecorationCardWidget] BuildPlacementPanelWidget opened for wall decoration"));
					}
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[OfficeDecorationCardWidget] OfficeManager not found"));
		}
	}

	// 델리게이트 브로드캐스트
	OnDecorationCardSelected.Broadcast(DecorationInfo);
}
