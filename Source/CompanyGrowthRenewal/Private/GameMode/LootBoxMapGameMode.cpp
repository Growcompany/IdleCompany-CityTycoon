// Fill out your copyright notice in the Description page of Project Settings.

#include "GameMode/LootBoxMapGameMode.h"
#include "Player/LootBoxOrbitPawn.h"
#include "Player/LootBoxPlayerController.h"
#include "Manager/UIManagerSubsystem.h"
#include "Core/CGGameInstance.h"
#include "Entity/LootBox/LootBoxActor.h"
#include "UI/Panel/LootBoxLayerWidget.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h" // TActorIterator 사용
#include "AsyncLoadingScreenLibrary.h"

ALootBoxMapGameMode::ALootBoxMapGameMode()
{
	// 회전 카메라 Pawn 설정
	DefaultPawnClass = ALootBoxOrbitPawn::StaticClass();

	// LootBox 전용 컨트롤러
	PlayerControllerClass = ALootBoxPlayerController::StaticClass();

	// HUD는 나중에 필요하면 추가
	HUDClass = nullptr;
}

void ALootBoxMapGameMode::StartPlay()
{
	Super::StartPlay();

	// 현재 카테고리 확인
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (GameInstance)
	{
		ELootBoxCategory CurrentCategory = GameInstance->GetCurrentLootBoxCategory();
		UE_LOG(LogTemp, Log, TEXT("[LootBoxMapGameMode] Started with category: %d"), static_cast<uint8>(CurrentCategory));

		// 카테고리별 초기화 (향후 확장)
		InitializeLootBoxMap(CurrentCategory);
	}

	// UI 표시
	if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		LootBoxWidget = UIManager->ShowLootBoxUI();
		UE_LOG(LogTemp, Log, TEXT("[LootBoxMapGameMode] ShowLootBoxUI called"));

		// 델리게이트 바인딩
		if (LootBoxWidget)
		{
			LootBoxWidget->OnLootBoxSelected.AddUObject(this, &ALootBoxMapGameMode::OnLootBoxSelectionChanged);
			LootBoxWidget->OnLootBoxOpening.AddUObject(this, &ALootBoxMapGameMode::OnLootBoxOpeningStarted);
			UE_LOG(LogTemp, Log, TEXT("[LootBoxMapGameMode] LootBox delegates bound"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[LootBoxMapGameMode] LootBoxWidget is null!"));
		}
	}

	// 로딩 화면 종료
	UAsyncLoadingScreenLibrary::StopLoadingScreen();
}

void ALootBoxMapGameMode::OnLootBoxSelectionChanged(FName LootBoxID)
{
	UE_LOG(LogTemp, Log, TEXT("[LootBoxMapGameMode] OnLootBoxSelectionChanged: %s"), *LootBoxID.ToString());

	if (CurrentLootBox)
	{
		CurrentLootBox->SetLootBoxVisual(LootBoxID);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootBoxMapGameMode] CurrentLootBox is null!"));
	}
}

void ALootBoxMapGameMode::OnLootBoxOpeningStarted(FName LootBoxID, int32 RewardSkinID)
{
	UE_LOG(LogTemp, Log, TEXT("[LootBoxMapGameMode] OnLootBoxOpeningStarted: %s, Reward: %d"),
		*LootBoxID.ToString(), RewardSkinID);

	// CurrentLootBox가 null이면 태그로 다시 찾기 시도
	if (!CurrentLootBox)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootBoxMapGameMode] CurrentLootBox is null, attempting to find by tag..."));

		for (TActorIterator<ALootBoxActor> It(GetWorld()); It; ++It)
		{
			ALootBoxActor* LootBox = *It;
			if (LootBox && LootBox->Tags.Contains(FName("MainLootBox")))
			{
				CurrentLootBox = LootBox;
				UE_LOG(LogTemp, Log, TEXT("[LootBoxMapGameMode] Found LootBoxActor by tag: %s"),
					*CurrentLootBox->GetName());
				break;
			}
		}

		if (!CurrentLootBox)
		{
			UE_LOG(LogTemp, Error, TEXT("[LootBoxMapGameMode] No LootBoxActor with tag 'MainLootBox' found!"));
		}
	}

	// 오픈 애니메이션 재생
	if (CurrentLootBox)
	{
		CurrentLootBox->OpenLootBox(RewardSkinID);
		UE_LOG(LogTemp, Log, TEXT("[LootBoxMapGameMode] OpenLootBox animation started"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LootBoxMapGameMode] CurrentLootBox is still null after retry!"));
		UE_LOG(LogTemp, Error, TEXT("[LootBoxMapGameMode] Please ensure a LootBoxActor is placed in the level!"));

		// 애니메이션 시작 실패 시 UI 즉시 복원
		if (LootBoxWidget)
		{
			LootBoxWidget->ShowUIAfterAnimation();
			UE_LOG(LogTemp, Warning, TEXT("[LootBoxMapGameMode] UI restored due to animation failure"));
		}
	}
}

void ALootBoxMapGameMode::OnLootBoxAnimationFinished()
{
	UE_LOG(LogTemp, Log, TEXT("[LootBoxMapGameMode] LootBox animation finished"));

	// UI 다시 보이기
	if (LootBoxWidget)
	{
		LootBoxWidget->ShowUIAfterAnimation();
	}
}

void ALootBoxMapGameMode::InitializeLootBoxMap(ELootBoxCategory Category)
{
	// TActorIterator로 태그 있는 LootBox 찾기 (첫 번째 것만, 효율적)
	for (TActorIterator<ALootBoxActor> It(GetWorld()); It; ++It)
	{
		ALootBoxActor* LootBox = *It;
		if (LootBox && LootBox->Tags.Contains(FName("MainLootBox")))
		{
			CurrentLootBox = LootBox;
			UE_LOG(LogTemp, Log, TEXT("[LootBoxMapGameMode] Found LootBox by tag: %s"),
				*CurrentLootBox->GetName());
			break; // 찾았으면 즉시 중단
		}
	}

	if (!CurrentLootBox)
	{
		UE_LOG(LogTemp, Error, TEXT("[LootBoxMapGameMode] No LootBox with tag 'MainLootBox' found in level!"));
		UE_LOG(LogTemp, Error, TEXT("[LootBoxMapGameMode] Please ensure a LootBoxActor is placed in the level."));
		return;
	}

	// 카테고리별 타입 설정
	ELootBoxType ChestType = ELootBoxType::Square;

	switch (Category)
	{
	case ELootBoxCategory::BuildingSkin:
		ChestType = ELootBoxType::Sphere;
		UE_LOG(LogTemp, Log, TEXT("[LootBoxMapGameMode] Setting LootBox to Sphere for BuildingSkin"));
		break;

	case ELootBoxCategory::BuildingItem:
		ChestType = ELootBoxType::Square;
		UE_LOG(LogTemp, Log, TEXT("[LootBoxMapGameMode] Setting LootBox to Square for BuildingItem"));
		break;

	default:
		UE_LOG(LogTemp, Warning, TEXT("[LootBoxMapGameMode] Unknown category: %d"), static_cast<uint8>(Category));
		break;
	}

	// LootBox 외형 설정 (Rarity + Type 조합으로 RowName 생성)
	FString TypeStr = (ChestType == ELootBoxType::Sphere) ? TEXT("Sphere") : TEXT("Square");
	FString RarityStr = TEXT("Common"); // 초기 등급은 Common

	FName LootBoxID = FName(*(TypeStr + TEXT("_") + RarityStr));
	CurrentLootBox->SetLootBoxVisual(LootBoxID);

	// 애니메이션 완료 델리게이트 바인딩
	CurrentLootBox->OnAnimationFinished.AddUObject(this, &ALootBoxMapGameMode::OnLootBoxAnimationFinished);
	UE_LOG(LogTemp, Log, TEXT("[LootBoxMapGameMode] Animation finished delegate bound"));

	// OrbitPawn 찾아서 LootBox 설정
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PC)
	{
		ALootBoxOrbitPawn* OrbitPawn = Cast<ALootBoxOrbitPawn>(PC->GetPawn());
		if (OrbitPawn)
		{
			OrbitPawn->SetLootBox(CurrentLootBox);
			UE_LOG(LogTemp, Log, TEXT("[LootBoxMapGameMode] LootBox set to OrbitPawn"));
		}
	}
}
