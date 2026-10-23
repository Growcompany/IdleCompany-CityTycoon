// Fill out your copyright notice in the Description page of Project Settings.


#include "GameMode/LoadingPreviewGameMode.h"

#include "Blueprint/UserWidget.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Manager/TableManagerSubsystem.h"
#include "UI/Panel/LoadingWidget.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
	// 맵을 다시 열어도(한 바퀴 끝) 고른 종류가 유지되도록 프로세스 단위로 기억한다.
	int32 GLoadingPreviewIndex = 1;
	constexpr uint64 LoadingPreviewHintKey = 0x4C6F61645072766CULL;

	ALoadingPreviewGameMode* FindLoadingPreviewMode(UWorld* World)
	{
		return World ? World->GetAuthGameMode<ALoadingPreviewGameMode>() : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs GLoadingPreviewSelectCommand(
		TEXT("CGR.LoadingPreview.Select"),
		TEXT("로딩 화면 미리보기의 종류를 고른다 (1부터). LoadingMap_Preview 실행 중에만 동작한다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ALoadingPreviewGameMode* Mode = FindLoadingPreviewMode(World);
			if (!Mode || Args.IsEmpty())
			{
				UE_LOG(LogTemp, Warning, TEXT("[LoadingPreview] LoadingMap_Preview 실행 중에 CGR.LoadingPreview.Select <번호> 로 쓴다"));
				return;
			}
			Mode->SelectEntry(FCString::Atoi(*Args[0]) - 1);
		}));

	FAutoConsoleCommandWithWorld GLoadingPreviewReplayCommand(
		TEXT("CGR.LoadingPreview.Replay"),
		TEXT("로딩 화면 미리보기를 처음부터 다시 재생한다."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (ALoadingPreviewGameMode* Mode = FindLoadingPreviewMode(World))
			{
				Mode->Replay();
			}
		}));
}

ALoadingPreviewGameMode::ALoadingPreviewGameMode()
{
	HUDClass = nullptr;
	// 화면 전체를 위젯이 덮으므로 폰이 필요 없다 — 기본 폰의 방향키 이동이 미리보기 키와 겹치지 않게 뺀다.
	DefaultPawnClass = nullptr;

	const auto AddEntry = [this](const TCHAR* Label, EWidgetType Type, ETitleLogoVariant Logo)
	{
		FLoadingPreviewEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.Label = Label;
		Entry.WidgetType = Type;
		Entry.LogoVariant = Logo;
	};
	AddEntry(TEXT("Current - coastal city"), EWidgetType::Loading, ETitleLogoVariant::Final);
	AddEntry(TEXT("Skyline - 3D buildings logo"), EWidgetType::TitleSkylineLoading, ETitleLogoVariant::Buildings3D);
	AddEntry(TEXT("Skyline - final logo"), EWidgetType::TitleSkylineLoading, ETitleLogoVariant::Final);
}

void ALoadingPreviewGameMode::BeginPlay()
{
	Super::BeginPlay();

	BindPreviewKeys();
	ShowSelected();
}

void ALoadingPreviewGameMode::SelectEntry(int32 Index)
{
	if (Entries.IsEmpty())
	{
		return;
	}
	GLoadingPreviewIndex = FMath::Clamp(Index, 0, Entries.Num() - 1);
	ShowSelected();
}

void ALoadingPreviewGameMode::StepEntry(int32 Delta)
{
	if (Entries.IsEmpty())
	{
		return;
	}
	const int32 Count = Entries.Num();
	SelectEntry(((GLoadingPreviewIndex + Delta) % Count + Count) % Count);
}

void ALoadingPreviewGameMode::Replay()
{
	ShowSelected();
}

void ALoadingPreviewGameMode::ShowSelected()
{
	if (Entries.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[LoadingPreview] Entries 가 비어 있다"));
		return;
	}
	GLoadingPreviewIndex = FMath::Clamp(GLoadingPreviewIndex, 0, Entries.Num() - 1);
	const FLoadingPreviewEntry& Entry = Entries[GLoadingPreviewIndex];

	if (Shown)
	{
		Shown->RemoveFromParent();
		Shown = nullptr;
	}

	UWorld* PreviewWorld = GetWorld();
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const TSubclassOf<UUserWidget> WidgetClass = TableMgr ? TableMgr->GetWidgetClass(Entry.WidgetType) : nullptr;
	APlayerController* PC = PreviewWorld ? PreviewWorld->GetFirstPlayerController() : nullptr;
	ULoadingWidget* Widget = (WidgetClass && PC) ? CreateWidget<ULoadingWidget>(PC, WidgetClass) : nullptr;
	if (!Widget)
	{
		UE_LOG(LogTemp, Error, TEXT("[LoadingPreview] '%s' 위젯 생성 실패 — DT_WidgetClass 행과 부모 클래스(ULoadingWidget)를 확인할 것"), *Entry.Label);
		return;
	}

	// 전환 대상·최소 표시 시간은 ULoadingWidget 의 WBP 튜너블이라 리플렉션으로 이 인스턴스 값만 바꾼다.
	// 전환 대상을 못 바꾸면 미리보기가 실제 게임 맵으로 넘어가 버리므로 띄우지 않는다.
	FStrProperty* TargetProp = FindFProperty<FStrProperty>(ULoadingWidget::StaticClass(), TEXT("TargetLevelPackagePath"));
	FFloatProperty* HoldProp = FindFProperty<FFloatProperty>(ULoadingWidget::StaticClass(), TEXT("MinDisplayTime"));
	if (!TargetProp || !HoldProp)
	{
		UE_LOG(LogTemp, Error, TEXT("[LoadingPreview] ULoadingWidget 의 TargetLevelPackagePath/MinDisplayTime 을 찾지 못해 미리보기를 띄우지 않는다"));
		return;
	}
	TargetProp->SetPropertyValue_InContainer(Widget, UWorld::RemovePIEPrefix(PreviewWorld->GetPackage()->GetName()));
	HoldProp->SetPropertyValue_InContainer(Widget, HoldSeconds);

	if (UTitleSkylineLoadingWidget* Skyline = Cast<UTitleSkylineLoadingWidget>(Widget))
	{
		Skyline->SetLogoVariant(Entry.LogoVariant);
	}
	Widget->AddToViewport(1000);
	Shown = Widget;

	UE_LOG(LogTemp, Log, TEXT("[LoadingPreview] %d/%d %s (%s)"), GLoadingPreviewIndex + 1, Entries.Num(),
		*Entry.Label, *WidgetClass->GetName());
	ShowHint();
}

void ALoadingPreviewGameMode::BindPreviewKeys()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}
	EnableInput(PC);
	if (!InputComponent)
	{
		return;
	}

	const auto BindAction = [this](const FKey& Key, TFunction<void()> Action)
	{
		FInputKeyBinding Binding(FInputChord(Key), IE_Pressed);
		Binding.KeyDelegate.GetDelegateForManualSet().BindWeakLambda(this, MoveTemp(Action));
		InputComponent->KeyBindings.Add(MoveTemp(Binding));
	};
	const FKey NumberKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
		EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(NumberKeys)); ++Index)
	{
		BindAction(NumberKeys[Index], [this, Index]() { SelectEntry(Index); });
	}
	BindAction(EKeys::Left, [this]() { StepEntry(-1); });
	BindAction(EKeys::Right, [this]() { StepEntry(1); });
	BindAction(EKeys::R, [this]() { Replay(); });
	BindAction(EKeys::SpaceBar, [this]() { Replay(); });
}

void ALoadingPreviewGameMode::ShowHint() const
{
	if (!GEngine || Entries.IsEmpty())
	{
		return;
	}
	const FString Hint = FString::Printf(
		TEXT("Loading preview  [%d/%d] %s      1-%d: select   Left/Right: prev/next   R: replay"),
		GLoadingPreviewIndex + 1, Entries.Num(), *Entries[GLoadingPreviewIndex].Label, FMath::Min(Entries.Num(), 9));
	GEngine->AddOnScreenDebugMessage(LoadingPreviewHintKey, 1.0e6f, FColor::White, Hint);
}
