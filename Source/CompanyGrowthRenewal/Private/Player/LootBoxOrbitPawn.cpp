// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/LootBoxOrbitPawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Entity/LootBox/LootBoxActor.h"
#include "Kismet/GameplayStatics.h"
#include "InputMappingContext.h"
#include "Input/InputPriorities.h"

ALootBoxOrbitPawn::ALootBoxOrbitPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// 회전 중심점
	OrbitCenter = CreateDefaultSubobject<USceneComponent>(TEXT("OrbitCenter"));
	RootComponent = OrbitCenter;

	// SpringArm 설정
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(OrbitCenter);
	SpringArm->TargetArmLength = CameraDistance;
	SpringArm->bDoCollisionTest = false;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 5.0f;

	// 카메라
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);

	// Enhanced Input 설정
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> imc_LootBox
	(TEXT("/Game/CompanyGrowth/Input/IMC_LootBox.IMC_LootBox"));
	if (imc_LootBox.Succeeded())
	{
		IMC_LootBox = imc_LootBox.Object;
	}

}

void ALootBoxOrbitPawn::BeginPlay()
{
	Super::BeginPlay();

	// SpringArm 거리 설정
	SpringArm->TargetArmLength = CameraDistance;
	SpringArm->SetRelativeLocation(FVector(250.0f, 0.0f, 0.0f));
	SpringArm->SetRelativeRotation(FRotator(-15.0f, 0.0f, 0.0f));

	// 초기 높이 설정
	OrbitCenter->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));

	// 초기 각도 설정 (180도 - 뒷면 바라보기)
	CurrentYaw = 180.0f;
	SetActorRotation(FRotator(-5.0f, 180.0f, 0.0f));

	UE_LOG(LogTemp, Log, TEXT("[LootBoxOrbitPawn] Camera initialized at 180 degrees (back view)"));
}

void ALootBoxOrbitPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 카메라 드래그 회전 기능 제거됨
	// LootBox는 고정된 각도에서 표시
}

void ALootBoxOrbitPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 참고: 입력 처리는 PlayerController에서 수행됨
	// Pawn은 카메라 역할만 담당
	UE_LOG(LogTemp, Log, TEXT("[LootBoxOrbitPawn] SetupPlayerInputComponent called (input handled by PlayerController)"));
}

void ALootBoxOrbitPawn::SetLootBox(ALootBoxActor* NewLootBox)
{
	if (!NewLootBox)
	{
		UE_LOG(LogTemp, Warning, TEXT("LootBoxOrbitPawn: NewLootBox is nullptr!"));
		return;
	}

	CurrentLootBox = NewLootBox;

	// 룩박스를 Pawn의 중심에 배치
	CurrentLootBox->SetActorLocation(GetActorLocation());
	CurrentLootBox->SetActorRotation(FRotator::ZeroRotator);

	UE_LOG(LogTemp, Log, TEXT("LootBoxOrbitPawn: LootBox set to '%s'"), *NewLootBox->GetName());
}

void ALootBoxOrbitPawn::ChangeLootBoxAppearance(ELootBoxRarity Rarity, ELootBoxType Type)
{
	if (!CurrentLootBox)
	{
		UE_LOG(LogTemp, Warning, TEXT("LootBoxOrbitPawn: No CurrentLootBox to change!"));
		return;
	}

	// Type 문자열 생성
	FString TypeStr = (Type == ELootBoxType::Square) ? TEXT("Square") : TEXT("Sphere");

	// Rarity 문자열 생성
	FString RarityStr;
	switch (Rarity)
	{
	case ELootBoxRarity::Common:
		RarityStr = TEXT("Common");
		break;
	case ELootBoxRarity::Unusual:
		RarityStr = TEXT("Unusual");
		break;
	case ELootBoxRarity::Rare:
		RarityStr = TEXT("Rare");
		break;
	case ELootBoxRarity::Epic:
		RarityStr = TEXT("Epic");
		break;
	case ELootBoxRarity::Legendary:
		RarityStr = TEXT("Legendary");
		break;
	case ELootBoxRarity::Mythic:
		RarityStr = TEXT("Mythic");
		break;
	default:
		RarityStr = TEXT("Common");
		break;
	}

	// "Square_Epic" 형태로 RowName 조합
	FName LootBoxID = FName(*(TypeStr + TEXT("_") + RarityStr));

	// 비주얼 변경 (SetLootBoxVisual이 내부에서 ApplyVisualConfig 호출)
	CurrentLootBox->SetLootBoxVisual(LootBoxID);

	UE_LOG(LogTemp, Log, TEXT("LootBoxOrbitPawn: Changed appearance to %s"), *LootBoxID.ToString());
}
