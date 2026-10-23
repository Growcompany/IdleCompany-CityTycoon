#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

// 튜토리얼 자동 플레이어 ― 안내가 가리키는 대상을 실제 Slate 입력(히트 테스트 경유)으로 눌러 가며
// timeline.jsonl · shots/ · issues.json · summary.json 을 남긴다. 설계 = 2026-10-16-tutorial-full-content-design.md §5.1
// CGR.TutorialAuto.Start <OutDir> [TimeScale] / CGR.TutorialAuto.Stop / CGR.TutorialAuto.Status

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "CommonActivatableWidget.h"
#include "CoreGlobals.h"
#include "InputCoreTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Components/Widget.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Layout/WidgetPath.h"
#include "Misc/CoreDelegates.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Types/ReflectionMetadata.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

#include "Core/CGGameInstance.h"
#include "EngineUtils.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Entity/Factory/BrickFactory.h"
#include "Enum/ItemType.h"
#include "Enum/ResourceType.h"
#include "GameMode/CGGameModeBase.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/GameClockSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/PanelIntroSubsystem.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/ProjectOperationManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Office/OfficeManager.h"
#include "Player/Components/PlacementHandler.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "UI/HUD/MissionGuideOverlayWidget.h"
#include "UI/HUD/PanelIntroOverlayWidget.h"
#include "UI/UIBase.h"
#include "Enum/WidgetType.h"
#include "Manager/TableManagerSubsystem.h"
#include "Player/MainMapPlayerController.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "UI/Element/LiveOps/NoticeRowWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/TextBlock.h"

static TAutoConsoleVariable<int32> CVarTutorialAutoMoveOSCursor(
	TEXT("CGR.TutorialAuto.MoveOSCursor"), 0,
	TEXT("1 = 합성 클릭 전에 OS 커서도 같은 자리로 옮긴다(Slate 합성 이동이 실제 커서 위치로 뷰포트 커서를 되돌리는 것 방지). 0 = 이벤트만 보낸다."));

static TAutoConsoleVariable<int32> CVarTutorialAutoForceLaunchFail(
	TEXT("CGR.TutorialAuto.ForceLaunchFail"), 0,
	TEXT("1 = 첫 자체개발(M7) 개발 동안 점수를 0으로 눌러 내부 테스트 미달 → [폐기] → 재착수 안내를 한 번 검증한다."));

static TAutoConsoleVariable<float> CVarTutorialAutoFactoryHoldSeconds(
	TEXT("CGR.TutorialAuto.FactoryHoldSeconds"), 0.f,
	TEXT("M2 UI buttons are held for this many real seconds to exercise purchase-repeat guards; 0 uses normal taps."));

static TAutoConsoleVariable<int32> CVarTutorialAutoEntryProbe(
	TEXT("CGR.TutorialAuto.EntryProbe"), 0,
	TEXT("N>0 = 튜토리얼을 마친 세이브에서 시작해 최대 N초 동안 접속 자동 창(정산·출석·공지)이 뜨는 순서를 기록하고 하나씩 ESC 로 닫는다."));

static TAutoConsoleVariable<int32> CVarTutorialAutoBackProbe(
	TEXT("CGR.TutorialAuto.BackProbe"), 0,
	TEXT("1 이면 접속 창 검사 뒤 창을 하나씩 열고 뒤로가기(ESC)로 닫히는지 본다(튜토리얼을 마친 세이브)."));

static TAutoConsoleVariable<int32> CVarTutorialAutoClockOffset(
	TEXT("CGR.TutorialAuto.ClockOffset"), 0,
	TEXT("시작할 때 게임 시계를 이만큼(초) 미룬다 ― 86400 이면 다음 날 접속을 본다."));

namespace CGRTutorialAuto
{
	// 판정 상수는 실시간(초) 기준 ― UMG 연출·읽기 시간이 시간 배속과 무관하게 실시간으로 흐르기 때문
	constexpr double ObserveInterval = 0.25;
	constexpr double ActStableSeconds = 1.2;
	constexpr double HoldSeconds = 1.6;
	constexpr double TapUpSeconds = 0.06;
	constexpr double ClickEffectSeconds = 8.0;
	constexpr double StuckSeconds = 25.0;
	constexpr double StuckEndSeconds = 120.0;
	constexpr double WaitWarnSeconds = 60.0;
	// 걸음 전환 직후엔 밝은 구멍이 옮겨 가는 중이라 말풍선이 아직 없다 ― 사람 검수용 장면은 안착 뒤 이만큼 지나 찍는다
	constexpr double SettledShotDelay = 0.8;
	constexpr double WaitEndSeconds = 300.0;
	constexpr double StepEndSeconds = 600.0;
	constexpr double NoWorldEndSeconds = 300.0;
	// 마지막 보상 창(자동 닫기 5초)이 닫히고 뒤로 미룬 알림·출석 자동 창이 뜰 때까지 끝 장면을 미룬다
	constexpr double FinalSceneMaxSeconds = 30.0;
	constexpr double FinalSceneSettleSeconds = 3.5;
	// 출석 창은 장비권 알림이 사라진 뒤 열린다 ― 이만큼 기다려도 창이 없으면 창 없는 끝 장면으로 본다
	constexpr double FinalSceneEntryWaitSeconds = 12.0;
	constexpr double PopupSeconds = 5.0;
	// 소프트 존 지연 힌트(5초) 뒤에 말풍선이 뜨는 걸음까지 기다린다
	constexpr double TooltipGraceSeconds = 8.0;
	constexpr double TooltipJudgeSeconds = 0.5;
	constexpr double TransientSeconds = 2.0;
	constexpr double IntroClickInterval = 1.6;
	constexpr int32 MaxAttemptsPerState = 3;
	constexpr int32 MaxClicksPerState = 30;
	constexpr int32 PingPongTransitions = 6;
	constexpr int32 MaxLogErrorLines = 2000;
	// 부족 대기 경고 기준은 게임 시간(실시간 × 배속) ― 실제 플레이어가 1배속으로 기다리는 시간이다
	constexpr double FundWaitWarnSeconds = 45.0;
	// 맵 로딩처럼 관찰이 끊긴 구간을 대기 시간에 통째로 더하지 않는다
	constexpr double EconomyMaxStepSeconds = 2.0;
	constexpr int32 EconomyKeyCount = 8;

	const TCHAR* EconomyKeyName(int32 ResIndex)
	{
		static const TCHAR* const Names[EconomyKeyCount] = {
			TEXT("money"), TEXT("brick"), TEXT("diamond"), TEXT("recruit_normal"),
			TEXT("recruit_adv"), TEXT("trait_normal"), TEXT("skin_normal"), TEXT("equip_normal") };
		return ResIndex >= 0 && ResIndex < EconomyKeyCount ? Names[ResIndex] : TEXT("unknown");
	}

	int32 EconomyIndexOf(EResourceType Type)
	{
		switch (Type)
		{
		case EResourceType::Money: return 0;
		case EResourceType::Brick: return 1;
		case EResourceType::Diamond: return 2;
		default: return INDEX_NONE;
		}
	}

	// 살 수 없어 지목이 빠진 걸음은 비용을 읽을 버튼이 없다 ― 부족 문구에 적힌 재화로 짐작한다
	int32 EconomyIndexFromShortfall(const FString& Line)
	{
		if (Line.Contains(TEXT("벽돌"))) return 1;
		if (Line.Contains(TEXT("보석"))) return 2;
		if (Line.Contains(TEXT("자금"))) return 0;
		return INDEX_NONE;
	}

	struct FResourceSnapshot
	{
		bool bValid = false;
		int64 Values[EconomyKeyCount] = {};
	};

	void ReadResources(UGameInstance* Instance, FResourceSnapshot& Out)
	{
		const UResourceItemManager* Resources = Instance ? Instance->GetSubsystem<UResourceItemManager>() : nullptr;
		const UItemInventoryManager* Items = Instance ? Instance->GetSubsystem<UItemInventoryManager>() : nullptr;
		if (!Resources || !Items)
		{
			return;
		}
		Out.bValid = true;
		Out.Values[0] = Resources->GetResourceAmount(EResourceType::Money);
		Out.Values[1] = Resources->GetResourceAmount(EResourceType::Brick);
		Out.Values[2] = Resources->GetResourceAmount(EResourceType::Diamond);
		Out.Values[3] = Items->GetItemCount(EItemType::RecruitTicketNormal);
		Out.Values[4] = Items->GetItemCount(EItemType::RecruitTicketAdvanced);
		Out.Values[5] = Items->GetItemCount(EItemType::BuildingTraitTicketNormal);
		Out.Values[6] = Items->GetItemCount(EItemType::SkinTicketNormal);
		Out.Values[7] = Items->GetItemCount(EItemType::EmployeeEquipmentTicketNormal);
	}

	const TSet<FKey>& NoButtons()
	{
		static const TSet<FKey> Buttons;
		return Buttons;
	}

	const TSet<FKey>& LeftButton()
	{
		static const TSet<FKey> Buttons = { EKeys::LeftMouseButton };
		return Buttons;
	}

	FString GestureName(EGestureHintKind Kind)
	{
		switch (Kind)
		{
		case EGestureHintKind::Tap: return TEXT("Tap");
		case EGestureHintKind::Hold: return TEXT("Hold");
		case EGestureHintKind::Drag: return TEXT("Drag");
		default: return TEXT("None");
		}
	}

	FString SanitizeToken(const FString& In)
	{
		FString Out = In;
		for (TCHAR& Ch : Out)
		{
			if (!FChar::IsAlnum(Ch) && Ch != TEXT('-'))
			{
				Ch = TEXT('_');
			}
		}
		return Out.IsEmpty() ? FString(TEXT("none")) : Out;
	}

	FVector2D RectCenter(const FSlateRect& Rect)
	{
		return FVector2D((Rect.Left + Rect.Right) * 0.5, (Rect.Top + Rect.Bottom) * 0.5);
	}

	bool RectContains(const FSlateRect& Bounds, const FVector2D& Point)
	{
		return Point.X >= Bounds.Left && Point.X <= Bounds.Right && Point.Y >= Bounds.Top && Point.Y <= Bounds.Bottom;
	}

	bool RectInside(const FSlateRect& Inner, const FSlateRect& Bounds, float Tolerance)
	{
		return Inner.Left >= Bounds.Left - Tolerance && Inner.Top >= Bounds.Top - Tolerance
			&& Inner.Right <= Bounds.Right + Tolerance && Inner.Bottom <= Bounds.Bottom + Tolerance;
	}

	bool WidgetAbsRect(const UWidget* Target, FSlateRect& OutRect)
	{
		if (!Target)
		{
			return false;
		}
		const FGeometry& Geo = Target->GetCachedGeometry();
		const FVector2D Size = Geo.GetLocalSize();
		if (Size.X <= KINDA_SMALL_NUMBER || Size.Y <= KINDA_SMALL_NUMBER)
		{
			return false;
		}
		OutRect = Geo.GetRenderBoundingRect();
		return true;
	}

	// 오버레이(GetGuideActorBounds)와 같은 기준점 ― 공장 예약 부지 박스는 외형보다 커서 메시 바운드를 쓴다
	void GetActorClickBounds(const AActor* Actor, FVector& OutOrigin, FVector& OutExtent)
	{
		if (const ABrickFactory* Factory = Cast<ABrickFactory>(Actor))
		{
			if (const UStaticMeshComponent* Body = Cast<UStaticMeshComponent>(Factory->GetMainMeshComponent()))
			{
				if (const UStaticMesh* Mesh = Body->GetStaticMesh())
				{
					const FBox WorldBounds = Mesh->GetBoundingBox().TransformBy(Body->GetComponentTransform());
					OutOrigin = WorldBounds.GetCenter();
					OutExtent = WorldBounds.GetExtent();
					return;
				}
			}
		}
		if (const ABuildingBaseActor* Building = Cast<ABuildingBaseActor>(Actor))
		{
			const FBox WorldBounds = Building->GetBuildingWorldBounds();
			if (WorldBounds.IsValid)
			{
				OutOrigin = WorldBounds.GetCenter();
				OutExtent = WorldBounds.GetExtent();
				return;
			}
		}
		Actor->GetActorBounds(false, OutOrigin, OutExtent);
	}

	bool ProjectActorAbsolute(APlayerController* PC, AActor* Actor, FVector2D& OutAbsolute)
	{
		if (!PC || !IsValid(Actor))
		{
			return false;
		}
		FVector Origin, Extent;
		GetActorClickBounds(Actor, Origin, Extent);
		FVector2D ViewportPos;
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Origin, ViewportPos, true))
		{
			return false;
		}
		const FGeometry ViewportGeo = UWidgetLayoutLibrary::GetViewportWidgetGeometry(PC);
		OutAbsolute = ViewportGeo.LocalToAbsolute(ViewportPos);
		return true;
	}

	bool IsSlateDescendantOf(const UWidget* Child, const UWidget* Ancestor)
	{
		if (!Child || !Ancestor)
		{
			return false;
		}
		const TSharedPtr<SWidget> AncestorSlate = Ancestor->GetCachedWidget();
		if (!AncestorSlate.IsValid())
		{
			return false;
		}
		for (TSharedPtr<SWidget> Cur = Child->GetCachedWidget(); Cur.IsValid(); Cur = Cur->GetParentWidget())
		{
			if (Cur == AncestorSlate)
			{
				return true;
			}
		}
		return false;
	}

	FString JsonToString(const TSharedRef<FJsonObject>& Object, bool bPretty)
	{
		FString Out;
		if (bPretty)
		{
			const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
				TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
			FJsonSerializer::Serialize(Object, Writer);
		}
		else
		{
			const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
				TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
			FJsonSerializer::Serialize(Object, Writer);
		}
		return Out;
	}

	TSharedPtr<FJsonValue> JsonNum(double Value)
	{
		return MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Value * 10.0) / 10.0);
	}

	TSharedRef<FJsonObject> ResJson(const FResourceSnapshot& Res)
	{
		const TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		for (int32 ResIndex = 0; Res.bValid && ResIndex < EconomyKeyCount; ++ResIndex)
		{
			Out->SetNumberField(EconomyKeyName(ResIndex), static_cast<double>(Res.Values[ResIndex]));
		}
		return Out;
	}

	TSharedRef<FJsonObject> AmountsJson(const int64 (&Amounts)[EconomyKeyCount])
	{
		const TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		for (int32 ResIndex = 0; ResIndex < EconomyKeyCount; ++ResIndex)
		{
			Out->SetNumberField(EconomyKeyName(ResIndex), static_cast<double>(Amounts[ResIndex]));
		}
		return Out;
	}

	TSharedPtr<FJsonValue> PointJson(const FVector2D& Abs, const FSlateRect& Viewport)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Add(JsonNum(Abs.X - Viewport.Left));
		Values.Add(JsonNum(Abs.Y - Viewport.Top));
		return MakeShared<FJsonValueArray>(Values);
	}

	// 좌표는 뷰포트 기준 픽셀 ― 캡처 PNG 의 픽셀과 바로 대조된다
	TSharedPtr<FJsonValue> RectJson(const FSlateRect& Rect, const FSlateRect& Viewport)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Add(JsonNum(Rect.Left - Viewport.Left));
		Values.Add(JsonNum(Rect.Top - Viewport.Top));
		Values.Add(JsonNum(Rect.Right - Rect.Left));
		Values.Add(JsonNum(Rect.Bottom - Rect.Top));
		return MakeShared<FJsonValueArray>(Values);
	}

	struct FCapturedLine
	{
		FString Text;
		ELogVerbosity::Type Verbosity = ELogVerbosity::Log;
		FName Category;
	};

	// 로그는 어느 스레드에서든 들어온다 ― 잠금 안에서 쌓기만 하고 판정은 게임 스레드 틱에서 한다
	class FLogCapture final : public FOutputDevice
	{
	public:
		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			if (!V || FCString::Strstr(V, TEXT("[TutorialAuto]")))
			{
				return;
			}
			const ELogVerbosity::Type Level = static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
			const bool bGuide = FCString::Strstr(V, TEXT("[MissionGuide]")) != nullptr;
			const bool bError = Level == ELogVerbosity::Error || Level == ELogVerbosity::Fatal;
			if (!bGuide && !bError)
			{
				return;
			}
			FScopeLock Lock(&Mutex);
			if (Pending.Num() < MaxPending)
			{
				FCapturedLine& Line = Pending.AddDefaulted_GetRef();
				Line.Text = V;
				Line.Verbosity = Level;
				Line.Category = Category;
			}
		}

		virtual bool CanBeUsedOnAnyThread() const override { return true; }
		virtual bool CanBeUsedOnMultipleThreads() const override { return true; }

		void Drain(TArray<FCapturedLine>& OutLines)
		{
			FScopeLock Lock(&Mutex);
			OutLines = MoveTemp(Pending);
			Pending.Reset();
		}

	private:
		static constexpr int32 MaxPending = 4096;
		FCriticalSection Mutex;
		TArray<FCapturedLine> Pending;
	};

	struct FObservation
	{
		FString MapName;
		double GameTime = 0.0;
		bool bHasManager = false;
		FName MissionID;
		FString MissionTitle;
		FName NextMissionID;
		bool bHardGate = false;
		int32 SeqCurrent = 0;
		int32 SeqTotal = 0;
		int32 PhaseIndex = INDEX_NONE;
		bool bGated = false;

		TWeakObjectPtr<UWidget> Target;
		FString TargetName;
		FString TargetClass;
		FString TargetOuter;
		bool bTargetRect = false;
		FSlateRect TargetRect;
		bool bTargetActionable = false;
		FString TargetReason;
		bool bTargetCovered = false;

		TWeakObjectPtr<AActor> Spotlight;
		FString SpotlightName;
		bool bSpotlightScreen = false;
		FVector2D SpotlightAbs = FVector2D::ZeroVector;

		EGestureHintKind Gesture = EGestureHintKind::None;
		TWeakObjectPtr<AActor> GestureAnchor;
		FString GestureAnchorName;
		bool bAnchorScreen = false;
		FVector2D AnchorAbs = FVector2D::ZeroVector;

		FString GuideLine;
		FString MentorLine;
		bool bExplain = false;
		int32 ExplainStep = INDEX_NONE;
		bool bReadyToClaim = false;
		bool bClaimPresentable = false;
		bool bClaimRect = false;
		FSlateRect ClaimRect;
		bool bTutorialCompleted = false;
		bool bRecruitPresentation = false;
		// 보상 띠가 떠 있는 동안 튜토리얼은 말풍선을 일부러 접는다
		bool bRewardToast = false;
		// 수령 보상 창 ― 카드가 0.12초 간격으로 한 장씩 나타난다
		bool bRewardSplash = false;
		bool bRewardSplashRevealed = false;
		int64 ProgressCurrent = -1;
		int64 ProgressTarget = -1;
		// 도출형 미션은 진행도 숫자가 없다 ― 연속 배치처럼 화면이 그대로인 클릭은 놓인 책상 수로 효과를 본다
		int32 PlacedDesks = -1;

		FString PromptTop;
		int32 PromptCount = 0;
		// 진단 ― 아래쪽 스택 맨 위·패널 소개 재생·판정기가 고른 원래 걸음(오버레이 필터 전)
		FString BottomTop;
		bool bIntroPlaying = false;
		FString DerivedRaw;
		bool bPlacement = false;
		int32 OperationCount = 0;
		bool bPanelIntro = false;
		bool bCityOpening = false;
		// 사무실↔도시 전환 막이 아직 덮고 있다 ― 그 위를 누르면 막에 맞는다
		bool bLevelTransition = false;
		// 말풍선만 붙는 관찰 걸음(남은 시간·잠긴 버튼·철거 중 건물) ― 누를 대상이 아니다
		bool bObserve = false;

		bool bOverlay = false;
		bool bOverlaySettled = true;
		bool bTooltip = false;
		FSlateRect TooltipRect;

		bool bViewport = false;
		FSlateRect ViewportRect;

		// 경제 ― 보유량·본사 레벨과 「재화가 모자라 기다리는 중」 판정
		FResourceSnapshot Res;
		int32 HQLevel = 0;
		EMissionConditionType Condition = EMissionConditionType::None;
		FString ShortfallLine;
		bool bTargetCostButton = false;
		bool bTargetAffordable = true;
		int64 TargetCost = 0;
		int32 TargetCostResource = INDEX_NONE;
		bool bFundWait = false;
		int32 FundResource = INDEX_NONE;
		int64 FundNeeded = -1;

		FString PhaseToken;
		FString TargetId;
		FString WaitReason;
		FString Key;

		bool HasActorTarget() const { return GestureAnchor.IsValid() || Spotlight.IsValid(); }
		bool ActorOnScreen() const
		{
			const FVector2D Point = GestureAnchor.IsValid() ? AnchorAbs : SpotlightAbs;
			const bool bProjected = GestureAnchor.IsValid() ? bAnchorScreen : bSpotlightScreen;
			return bProjected && bViewport && RectContains(ViewportRect, Point);
		}
		FVector2D ActorPoint() const { return GestureAnchor.IsValid() ? AnchorAbs : SpotlightAbs; }
	};

	struct FIssue
	{
		FString Severity;
		FString Code;
		FString Mission;
		FString Phase;
		FString Message;
		int32 TimelineSeq = 0;
	};

	struct FMissionRecord
	{
		FName MissionID;
		double Seconds = 0.0;
		bool bShowedGuide = false;
	};

	enum class EPendingInput : uint8 { None, TapUp, HoldUp };

	class FPlayer
	{
	public:
		bool IsRunning() const { return bRunning; }

		void Start(const FString& InOutDir, float InTimeScale)
		{
			if (bRunning)
			{
				UE_LOG(LogTemp, Warning, TEXT("[TutorialAuto] 이미 실행 중입니다 ― OutDir=%s"), *OutDir);
				return;
			}
			ResetRunState();
			if (const int32 ClockOffset = CVarTutorialAutoClockOffset.GetValueOnGameThread())
			{
				UWorld* ClockWorld = FindGameWorld();
				UGameInstance* ClockGI = ClockWorld ? ClockWorld->GetGameInstance() : nullptr;
				if (UGameClockSubsystem* Clock = ClockGI ? ClockGI->GetSubsystem<UGameClockSubsystem>() : nullptr)
				{
					Clock->SetDebugOffsetSeconds(Clock->GetDebugOffsetSeconds() + ClockOffset);
					UE_LOG(LogTemp, Display, TEXT("[TutorialAuto] 시계를 %d초 미뤘습니다 ― 오늘=%s"), ClockOffset, *Clock->GetDayKey());
				}
			}
			OutDir = FPaths::ConvertRelativePathToFull(InOutDir);
			FPaths::NormalizeDirectoryName(OutDir);
			ShotsDir = FPaths::Combine(OutDir, TEXT("shots"));
			IFileManager& FileManager = IFileManager::Get();
			if (!FileManager.MakeDirectory(*ShotsDir, true))
			{
				UE_LOG(LogTemp, Error, TEXT("[TutorialAuto] 출력 폴더를 만들 수 없습니다: %s"), *ShotsDir);
				return;
			}
			// 같은 폴더 재실행이면 지난 결과를 지워 실행기가 옛 summary 를 집지 않게 한다
			for (const TCHAR* FileName : { TEXT("timeline.jsonl"), TEXT("issues.json"), TEXT("summary.json"), TEXT("log_errors.txt"), TEXT("run_info.json") })
			{
				FileManager.Delete(*FPaths::Combine(OutDir, FileName), false, true, true);
			}
			TimeScale = FMath::Clamp(InTimeScale, 0.1f, 20.f);
			StartedAt = FPlatformTime::Seconds();
			LastProgressAt = StartedAt;
			LastCommitAt = StartedAt;
			LastWorldSeenAt = StartedAt;

			if (!LogCapture)
			{
				LogCapture = new FLogCapture();
			}
			if (GLog)
			{
				GLog->AddOutputDevice(LogCapture);
			}
			PreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(this, &FPlayer::HandleEnginePreExit);
			TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FPlayer::Tick), 0.f);
			bRunning = true;

			const TSharedRef<FJsonObject> Info = MakeShared<FJsonObject>();
			Info->SetStringField(TEXT("out_dir"), OutDir);
			Info->SetNumberField(TEXT("time_scale"), TimeScale);
			Info->SetStringField(TEXT("started"), FDateTime::Now().ToIso8601());
			Info->SetBoolField(TEXT("move_os_cursor"), CVarTutorialAutoMoveOSCursor.GetValueOnGameThread() != 0);
			Info->SetBoolField(TEXT("force_launch_fail"), CVarTutorialAutoForceLaunchFail.GetValueOnGameThread() != 0);
			FFileHelper::SaveStringToFile(JsonToString(Info, true), *FPaths::Combine(OutDir, TEXT("run_info.json")),
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			WriteIssues();
			UE_LOG(LogTemp, Display, TEXT("[TutorialAuto] 시작 OutDir=%s TimeScale=%.2f"), *OutDir, TimeScale);
		}

		void Stop(const FString& Reason)
		{
			if (!bRunning)
			{
				UE_LOG(LogTemp, Display, TEXT("[TutorialAuto] 실행 중이 아닙니다."));
				return;
			}
			Finish(Reason, false);
		}

		void PrintStatus() const
		{
			if (!bRunning)
			{
				UE_LOG(LogTemp, Display, TEXT("[TutorialAuto] 정지 상태 (마지막 OutDir=%s, Block=%d Warn=%d)"),
					*OutDir, CountIssues(TEXT("Block")), CountIssues(TEXT("Warn")));
				return;
			}
			const double Now = FPlatformTime::Seconds();
			UE_LOG(LogTemp, Display,
				TEXT("[TutorialAuto] 실행 중 %.0fs seq=%d key=%s wait=%s idle=%.0fs Block=%d Warn=%d OutDir=%s"),
				Now - StartedAt, Seq, *CommittedKey, *LastObs.WaitReason, Now - LastProgressAt,
				CountIssues(TEXT("Block")), CountIssues(TEXT("Warn")), *OutDir);
		}

	private:
		// ===== 실행 상태 =====
		bool bRunning = false;
		bool bInTick = false;
		FString OutDir;
		FString ShotsDir;
		float TimeScale = 1.f;
		double StartedAt = 0.0;
		double LastObserveAt = 0.0;
		double LastWorldSeenAt = 0.0;
		FTSTicker::FDelegateHandle TickerHandle;
		FDelegateHandle PreExitHandle;
		FLogCapture* LogCapture = nullptr;   // 다른 스레드가 쓰는 중일 수 있어 지우지 않고 재사용한다
		int32 LogErrorLines = 0;

		TWeakObjectPtr<UWorld> LastWorld;
		bool bDilationApplied = false;
		TWeakObjectPtr<UMissionManagerSubsystem> BoundManager;
		FDelegateHandle PhaseHandle;
		FDelegateHandle ProgressHandle;
		int32 PhaseBroadcasts = 0;
		int32 ProgressBroadcasts = 0;

		// ===== 상태 키 / 타임라인 =====
		int32 Seq = 0;
		int32 CommittedSeq = 0;
		FString CommittedKey;
		FString CandidateKey;
		double CandidateSince = 0.0;
		int32 CandidateHits = 0;
		double StateSince = 0.0;
		double LastCommitAt = 0.0;
		double ActNotBefore = 0.0;
		double LastProgressAt = 0.0;
		FString LastFingerprint;
		FString LastProgressPrint;
		FObservation LastObs;
		TMap<FString, int32> PairTransitions;

		// ===== 미션 =====
		FName CommittedMission;
		FName LastRealMission;
		FName LastRealNext;
		double MissionStartedAt = 0.0;
		bool bMissionShowedGuide = false;
		bool bSawIncomplete = false;
		TArray<FMissionRecord> MissionRecords;
		bool bRewardShotTaken = false;
		double CompletedSeenAt = 0.0;
		double RewardClosedAt = 0.0;
		double FinalPromptSeenAt = 0.0;
		bool bProbeActive = false;
		bool bProbeShotTaken = false;
		double ProbeStartedAt = 0.0;
		double ProbeSeenAt = 0.0;
		double ProbeLastEscAt = 0.0;
		int32 ProbeEscCount = 0;
		bool bProbeNoticeClicked = false;
		double ProbeNoticeClickAt = 0.0;
		FString ProbeNoticeExpect;
		double ProbeIdleSince = 0.0;
		FString ProbeCurrent;
		TArray<FString> ProbeOrder;
		bool bBackProbe = false;
		int32 BackStep = 0;
		int32 BackPhase = 0;
		double BackPhaseAt = 0.0;
		int32 BackPass = 0;
		int32 BackBottomBefore = 0;
		// 게임 쪽 창(미션 완료·결산 등)이 끼어들면 그 단계를 치우고 다시 한다
		int32 BackRetry = 0;
		TArray<FString> BackFails;
		TWeakObjectPtr<UUserWidget> BackDialog;

		// ===== 판정 타이머 =====
		double RectStableSince = 0.0;
		FSlateRect LastTargetRect;
		bool bLastTargetRect = false;
		double TooltipVisibleSince = 0.0;
		double GatedNoTargetSince = 0.0;
		double OffscreenSince = 0.0;
		double PopupSince = 0.0;
		FString PopupClass;
		bool bWantEscape = false;
		FString LastWaitReason;
		double WaitSince = 0.0;
		bool bStuckReported = false;
		bool bSettledShotPending = false;
		bool bForcedLaunchFailDone = false;
		double LastIntroClickAt = 0.0;

		// ===== 입력 =====
		EPendingInput Pending = EPendingInput::None;
		FVector2D PendingPos = FVector2D::ZeroVector;
		double PendingUpAt = 0.0;
		uint64 PendingDownFrame = 0;
		FVector2D LastPointerPos = FVector2D::ZeroVector;
		FString ActionPrint;
		FString ActionDesc;
		double LastActionAt = 0.0;
		bool bAwaitingEffect = false;
		FResourceSnapshot ActionRes;
		int32 AttemptCount = 0;
		int32 ClickCount = 0;

		// ===== 문제 =====
		TArray<FIssue> Issues;
		TSet<FString> ReportedOnce;

		// ===== 경제 =====
		FName EconomyMission;
		double EconomyMissionStartedAt = 0.0;
		FResourceSnapshot EconomyStartRes;
		FResourceSnapshot LastEconomyRes;
		int64 MissionSpent[EconomyKeyCount] = {};
		int64 RunSpent[EconomyKeyCount] = {};
		double MissionFundWait = 0.0;
		double RunFundWait = 0.0;
		double LastEconomyAt = 0.0;
		double FundWaitSince = 0.0;
		int32 FundWaitResource = INDEX_NONE;
		int64 FundWaitNeeded = -1;
		int64 FundWaitOwnedStart = -1;
		FString FundWaitMission;
		FString FundWaitPhase;
		FString FundWaitTarget;
		bool bFundWaitWarned = false;
		double AffordWaitSince = 0.0;
		TSet<FString> AffordReported;
		TArray<TSharedPtr<FJsonValue>> MissionAffordable;
		TArray<TSharedPtr<FJsonValue>> EconomyMissions;
		int64 HQStepMoney = -1;
		FString HQStepMission;

		void ResetRunState()
		{
			LastObserveAt = 0.0;
			LastWorld.Reset();
			bDilationApplied = false;
			PhaseBroadcasts = 0;
			ProgressBroadcasts = 0;
			Seq = 0;
			CommittedSeq = 0;
			bForcedLaunchFailDone = false;
			CommittedKey.Reset();
			CandidateKey.Reset();
			CandidateHits = 0;
			StateSince = 0.0;
			LastCommitAt = 0.0;
			ActNotBefore = 0.0;
			LastFingerprint.Reset();
			LastProgressPrint.Reset();
			LastObs = FObservation();
			PairTransitions.Reset();
			CommittedMission = NAME_None;
			LastRealMission = NAME_None;
			LastRealNext = NAME_None;
			MissionStartedAt = 0.0;
			bMissionShowedGuide = false;
			bSawIncomplete = false;
			MissionRecords.Reset();
			bRewardShotTaken = false;
			CompletedSeenAt = 0.0;
			RewardClosedAt = 0.0;
			FinalPromptSeenAt = 0.0;
			bProbeActive = false;
			bProbeShotTaken = false;
			ProbeStartedAt = 0.0;
			ProbeSeenAt = 0.0;
			ProbeLastEscAt = 0.0;
			ProbeEscCount = 0;
			bProbeNoticeClicked = false;
			ProbeNoticeClickAt = 0.0;
			ProbeNoticeExpect.Reset();
			ProbeIdleSince = 0.0;
			ProbeCurrent.Reset();
			ProbeOrder.Reset();
			bBackProbe = false;
			BackStep = 0;
			BackPhase = 0;
			BackPhaseAt = 0.0;
			BackPass = 0;
			BackFails.Reset();
			BackDialog.Reset();
			RectStableSince = 0.0;
			bLastTargetRect = false;
			TooltipVisibleSince = 0.0;
			GatedNoTargetSince = 0.0;
			OffscreenSince = 0.0;
			PopupSince = 0.0;
			PopupClass.Reset();
			bWantEscape = false;
			LastWaitReason.Reset();
			WaitSince = 0.0;
			bStuckReported = false;
			LastIntroClickAt = 0.0;
			Pending = EPendingInput::None;
			ActionPrint.Reset();
			ActionDesc.Reset();
			LastActionAt = 0.0;
			bAwaitingEffect = false;
			AttemptCount = 0;
			ClickCount = 0;
			Issues.Reset();
			ReportedOnce.Reset();
			LogErrorLines = 0;
			EconomyMission = NAME_None;
			EconomyMissionStartedAt = 0.0;
			EconomyStartRes = FResourceSnapshot();
			LastEconomyRes = FResourceSnapshot();
			for (int32 ResIndex = 0; ResIndex < EconomyKeyCount; ++ResIndex)
			{
				MissionSpent[ResIndex] = 0;
				RunSpent[ResIndex] = 0;
			}
			MissionFundWait = 0.0;
			RunFundWait = 0.0;
			LastEconomyAt = 0.0;
			FundWaitSince = 0.0;
			bFundWaitWarned = false;
			AffordWaitSince = 0.0;
			AffordReported.Reset();
			MissionAffordable.Reset();
			EconomyMissions.Reset();
			HQStepMoney = -1;
			HQStepMission.Reset();
		}

		int32 CountIssues(const TCHAR* Severity) const
		{
			int32 Count = 0;
			for (const FIssue& Issue : Issues)
			{
				Count += Issue.Severity == Severity ? 1 : 0;
			}
			return Count;
		}

		static UWorld* FindGameWorld()
		{
			if (!GEngine)
			{
				return nullptr;
			}
			// PIE 를 먼저 ― 에디터에서 둘 다 살아 있으면 사람이 보는 쪽이 PIE 다
			for (EWorldType::Type Wanted : { EWorldType::PIE, EWorldType::Game })
			{
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					UWorld* Candidate = Context.World();
					if (Context.WorldType == Wanted && Candidate && Candidate->GetGameInstance())
					{
						return Candidate;
					}
				}
			}
			return nullptr;
		}

		void HandleEnginePreExit()
		{
			if (bRunning)
			{
				Finish(TEXT("engine-exit"), false);
			}
		}

		void HandlePhaseChanged() { ++PhaseBroadcasts; }
		void HandleProgressChanged() { ++ProgressBroadcasts; }

		void BindManager(UMissionManagerSubsystem* Mgr)
		{
			if (BoundManager.Get() == Mgr)
			{
				return;
			}
			UnbindManager();
			if (!Mgr)
			{
				return;
			}
			BoundManager = Mgr;
			PhaseHandle = Mgr->OnGuidePhaseChanged.AddRaw(this, &FPlayer::HandlePhaseChanged);
			ProgressHandle = Mgr->OnMissionProgressChanged.AddRaw(this, &FPlayer::HandleProgressChanged);
		}

		void UnbindManager()
		{
			if (UMissionManagerSubsystem* Old = BoundManager.Get())
			{
				Old->OnGuidePhaseChanged.Remove(PhaseHandle);
				Old->OnMissionProgressChanged.Remove(ProgressHandle);
			}
			BoundManager.Reset();
			PhaseHandle.Reset();
			ProgressHandle.Reset();
		}

		// ===== 틱 =====
		bool Tick(float /*DeltaSeconds*/)
		{
			if (!bRunning)
			{
				return false;
			}
			TGuardValue<bool> TickGuard(bInTick, true);
			const double Now = FPlatformTime::Seconds();
			UWorld* GameWorld = FindGameWorld();
			if (GameWorld != LastWorld.Get())
			{
				// 맵 이동으로 누르던 대상이 사라졌다 ― 눌린 버튼을 남기면 다음 맵 입력이 꼬인다
				ReleasePendingInput(TEXT("world-changed"));
				LastWorld = GameWorld;
				bDilationApplied = false;
			}
			if (GameWorld && !bDilationApplied && GameWorld->HasBegunPlay())
			{
				if (AWorldSettings* Settings = GameWorld->GetWorldSettings())
				{
					Settings->SetTimeDilation(TimeScale);
					bDilationApplied = true;
				}
			}
			PumpInput(GameWorld, Now);
			if (Now - LastObserveAt < ObserveInterval)
			{
				return bRunning;
			}
			LastObserveAt = Now;
			DrainLog();
			if (!GameWorld || !GameWorld->HasBegunPlay())
			{
				if (Now - LastWorldSeenAt > NoWorldEndSeconds)
				{
					AddIssue(TEXT("Block"), TEXT("no-world"), FString::Printf(TEXT("게임 월드를 %.0f초 동안 찾지 못했습니다."), Now - LastWorldSeenAt));
					Finish(TEXT("no-world"), false);
				}
				return bRunning;
			}
			LastWorldSeenAt = Now;

			FObservation Obs;
			Observe(GameWorld, Obs);
			ForceFirstLaunchFail(GameWorld, Obs);
			// 지출은 직전 상태(LastObs)에서 누른 결과다 ― 미션 전환 커밋보다 먼저 센다
			TrackEconomy(Obs, Now);
			UpdateState(Obs, Now);
			CaptureRewardSplash(Obs);
			// 접속 자동 창 기록 중엔 튜토리얼 판정·조작을 하지 않는다(진행할 미션이 없다)
			if (!bProbeActive)
			{
				Detect(Obs, Now);
				if (bRunning)
				{
					Act(GameWorld, Obs, Now);
				}
			}
			LastObs = Obs;
			if (bRunning)
			{
				CheckEnd(Obs, Now);
			}
			return bRunning;
		}

		// 미달은 난수라 회차마다 나오지 않는다 ― 첫 개발 동안 점수를 눌러 미달 창 안내를 한 번은 반드시 지나가게 한다
		void ForceFirstLaunchFail(UWorld* GameWorld, const FObservation& Obs)
		{
			if (bForcedLaunchFailDone || CVarTutorialAutoForceLaunchFail.GetValueOnGameThread() == 0
				|| Obs.Condition != EMissionConditionType::LaunchFirstInHouseProject)
			{
				return;
			}
			UOfficeStageProgressManager* Stage = GameWorld->GetSubsystem<UOfficeStageProgressManager>();
			if (!Stage)
			{
				return;
			}
			if (Stage->GetLifecycle() == EProjectLifecycle::LaunchPending)
			{
				bForcedLaunchFailDone = true;
				AddIssue(TEXT("Info"), TEXT("forced-launch-fail"), TEXT("첫 개발 점수를 0으로 눌러 내부 테스트 미달을 만들었습니다."));
				return;
			}
			if (Stage->IsStageInProgress())
			{
				Stage->Debug_FillScores(0.f);
			}
		}

		// ===== 관찰 =====
		void Observe(UWorld* GameWorld, FObservation& Obs)
		{
			Obs.MapName = GameWorld->GetMapName();
			Obs.GameTime = GameWorld->GetTimeSeconds();
			UGameInstance* Instance = GameWorld->GetGameInstance();
			if (const UCGGameInstance* CGInstance = Cast<UCGGameInstance>(Instance))
			{
				Obs.bLevelTransition = CGInstance->IsLevelTransitionActive();
			}
			APlayerController* PC = GameWorld->GetFirstPlayerController();

			if (UGameViewportClient* ViewportClient = GameWorld->GetGameViewport())
			{
				if (const TSharedPtr<SViewport> ViewportWidget = ViewportClient->GetGameViewportWidget())
				{
					Obs.ViewportRect = ViewportWidget->GetTickSpaceGeometry().GetLayoutBoundingRect();
					Obs.bViewport = Obs.ViewportRect.GetArea() > 1.f;
				}
			}

			UUIBase* UIBase = nullptr;
			UCommonActivatableWidget* TopPrompt = nullptr;
			if (UUIManagerSubsystem* UIMgr = Instance ? Instance->GetSubsystem<UUIManagerSubsystem>() : nullptr)
			{
				UIBase = UIMgr->GetUIBase();
				if (UIBase)
				{
					Obs.PromptCount = UIBase->GetPromptStackCount();
					TopPrompt = UIBase->GetActivePromptWidget();
					Obs.PromptTop = TopPrompt ? TopPrompt->GetClass()->GetName() : FString();
					const UCommonActivatableWidget* TopBottomWidget = UIBase->GetActiveBottomWidget();
					Obs.BottomTop = TopBottomWidget ? TopBottomWidget->GetClass()->GetName() : FString();
				}
			}
			if (const APawn* Pawn = PC ? PC->GetPawn() : nullptr)
			{
				if (const UPlacementHandler* Handler = Pawn->FindComponentByClass<UPlacementHandler>())
				{
					Obs.bPlacement = Handler->HasActivePlacement();
				}
			}
			if (const UProjectOperationManager* Operations = Instance ? Instance->GetSubsystem<UProjectOperationManager>() : nullptr)
			{
				Obs.OperationCount = Operations->GetActiveOperationCount();
			}
			ReadResources(Instance, Obs.Res);
			if (const UOfficeManager* Office = GameWorld->GetSubsystem<UOfficeManager>())
			{
				Obs.PlacedDesks = Office->GetPlacedWorkstations().Num();
			}
			if (USaveLoadManager* SaveMgr = Instance ? Instance->GetSubsystem<USaveLoadManager>() : nullptr)
			{
				Obs.HQLevel = SaveMgr->GetHQLevel();
			}
			if (const ACGGameModeBase* Mode = GameWorld ? GameWorld->GetAuthGameMode<ACGGameModeBase>() : nullptr)
			{
				Obs.bCityOpening = Mode->IsCityOpeningActive();
			}
			for (TObjectIterator<UPanelIntroOverlayWidget> It; It; ++It)
			{
				if (IsValid(*It) && It->GetWorld() == GameWorld && It->IsInViewport() && It->IsVisible())
				{
					Obs.bPanelIntro = true;
					break;
				}
			}
			for (TObjectIterator<UMissionGuideOverlayWidget> It; It; ++It)
			{
				if (IsValid(*It) && It->GetWorld() == GameWorld && It->IsInViewport())
				{
					Obs.bOverlay = true;
					Obs.bOverlaySettled = It->IsGuideTransitionSettled();
					Obs.bTooltip = It->IsVisible() && It->GetVisibleTooltipRect(Obs.TooltipRect);
					break;
				}
			}

			UMissionManagerSubsystem* Mgr = Instance ? Instance->GetSubsystem<UMissionManagerSubsystem>() : nullptr;
			BindManager(Mgr);
			if (Mgr)
			{
				Obs.bHasManager = true;
				Obs.bTutorialCompleted = Mgr->IsTutorialCompleted();
				Obs.MissionID = Mgr->GetActiveMissionID();
				FMissionTable Row;
				const bool bRow = Mgr->GetActiveMission(Row);
				if (bRow)
				{
					Obs.MissionTitle = Row.Title.ToString();
					Obs.NextMissionID = Row.NextMissionID;
					Obs.bHardGate = Row.bHardGate;
					Obs.Condition = Row.ConditionType;
					Obs.ShortfallLine = Row.ShortfallLine.ToString();
				}
				Mgr->GetMissionSequence(Obs.SeqCurrent, Obs.SeqTotal);
				Obs.GuideLine = Mgr->GetCurrentGuideLine().ToString();
				Obs.MentorLine = Mgr->GetCurrentMentorLine().ToString();
				Obs.PhaseIndex = bRow ? Mgr->GetGuidePhase() : INDEX_NONE;
				if (const UPanelIntroSubsystem* IntroMgr = Instance ? Instance->GetSubsystem<UPanelIntroSubsystem>() : nullptr)
				{
					Obs.bIntroPlaying = IntroMgr->IsIntroPlaying();
				}
				if (Mgr->IsDerivedGuideActive())
				{
					const FGoalGuideStep Raw = Mgr->GetDerivedStep();
					Obs.DerivedRaw = FString::Printf(TEXT("idx=%d anchor=%s/%s spot=%s%s"), Raw.Index,
						Raw.Anchor ? *GetNameSafe(Raw.Anchor->GetTypedOuter<UUserWidget>()) : TEXT("-"),
						*GetNameSafe(Raw.Anchor), *GetNameSafe(Raw.Spot), Raw.bObserve ? TEXT(" observe") : TEXT(""));
				}
				Obs.bGated = Mgr->IsInputGated();
				Obs.bObserve = Mgr->IsGuideObserveStep();
				Obs.bExplain = Mgr->IsExplainPhaseActive();
				Obs.ExplainStep = Obs.bExplain ? Mgr->GetExplainStep() : INDEX_NONE;
				Obs.bReadyToClaim = Mgr->IsReadyToClaim();
				Obs.bClaimPresentable = Obs.bReadyToClaim && Mgr->IsClaimPresentable();
				if (Obs.bClaimPresentable)
				{
					Obs.bClaimRect = WidgetAbsRect(Mgr->GetClaimSpotlightTarget(), Obs.ClaimRect);
				}
				Obs.bRecruitPresentation = Mgr->IsRecruitmentPresentationActive();
				Obs.bRewardSplash = Mgr->IsRewardSplashShown();
				Obs.bRewardSplashRevealed = Obs.bRewardSplash && Mgr->IsRewardSplashRevealComplete();
				if (const UUIManagerSubsystem* ToastUI = Instance ? Instance->GetSubsystem<UUIManagerSubsystem>() : nullptr)
				{
					Obs.bRewardToast = ToastUI->IsRewardToastShown();
				}
				int64 Current = 0;
				int64 Goal = 0;
				if (Mgr->GetMissionProgress(Current, Goal))
				{
					Obs.ProgressCurrent = Current;
					Obs.ProgressTarget = Goal;
				}

				if (UWidget* Target = Mgr->GetCurrentHighlightTarget())
				{
					Obs.Target = Target;
					Obs.TargetName = Target->GetName();
					Obs.TargetClass = Target->GetClass()->GetName();
					const UUserWidget* OuterWidget = Target->GetTypedOuter<UUserWidget>();
					Obs.TargetOuter = OuterWidget ? OuterWidget->GetName() : FString();
					Obs.bTargetActionable = Mgr->IsGuideTargetActionable(Target, Obs.TargetReason);
					Obs.bTargetRect = WidgetAbsRect(Target, Obs.TargetRect);
					// 오버레이와 같은 판정 ― 최상단 프롬프트 밖이면서 UIBase 안이면 가려진 것이다
					Obs.bTargetCovered = UIBase && Obs.PromptCount > 0
						&& !(IsSlateDescendantOf(Target, TopPrompt) || !IsSlateDescendantOf(Target, UIBase));
					if (const UCostActionButtonWidget* CostButton = Cast<UCostActionButtonWidget>(Target))
					{
						Obs.bTargetCostButton = CostButton->GetCurrentCost() > 0;
						Obs.bTargetAffordable = CostButton->CanAfford();
						Obs.TargetCost = CostButton->GetCurrentCost();
						Obs.TargetCostResource = EconomyIndexOf(CostButton->GetCurrentCostResource());
					}
				}
				if (AActor* Spot = Mgr->GetCurrentSpotlightActor())
				{
					Obs.Spotlight = Spot;
					Obs.SpotlightName = Spot->GetName();
					Obs.bSpotlightScreen = ProjectActorAbsolute(PC, Spot, Obs.SpotlightAbs);
				}
				Obs.Gesture = Mgr->GetCurrentGesture();
				if (AActor* Anchor = Mgr->GetCurrentGestureAnchorActor())
				{
					Obs.GestureAnchor = Anchor;
					Obs.GestureAnchorName = Anchor->GetName();
					Obs.bAnchorScreen = ProjectActorAbsolute(PC, Anchor, Obs.AnchorAbs);
				}
			}

			// 살 버튼이 꺼졌거나(비용 부족) 부족 문구로 바뀌었다 ― 진짜 플레이어처럼 누르지 않고 수익을 기다린다
			Obs.bFundWait = !Obs.MissionID.IsNone() && !Obs.bReadyToClaim && !Obs.bExplain && !Obs.bPanelIntro
				&& ((Obs.bTargetCostButton && !Obs.bTargetAffordable)
					|| (!Obs.ShortfallLine.IsEmpty() && Obs.GuideLine == Obs.ShortfallLine));
			if (Obs.bFundWait)
			{
				Obs.FundResource = Obs.bTargetCostButton ? Obs.TargetCostResource : EconomyIndexFromShortfall(Obs.ShortfallLine);
				Obs.FundNeeded = Obs.bTargetCostButton ? Obs.TargetCost : -1;
			}

			if (Obs.bReadyToClaim)
			{
				Obs.PhaseToken = TEXT("claim");
			}
			else if (Obs.bExplain)
			{
				Obs.PhaseToken = FString::Printf(TEXT("e%d"), Obs.ExplainStep);
			}
			else
			{
				Obs.PhaseToken = Obs.PhaseIndex >= 0 ? FString::Printf(TEXT("p%d"), Obs.PhaseIndex) : FString(TEXT("px"));
			}
			if (Obs.Target.IsValid())
			{
				Obs.TargetId = FString::Printf(TEXT("W:%s/%s"), *Obs.TargetOuter, *Obs.TargetName);
			}
			else if (Obs.HasActorTarget())
			{
				Obs.TargetId = FString::Printf(TEXT("A:%s:%s"),
					Obs.GestureAnchor.IsValid() ? *Obs.GestureAnchorName : *Obs.SpotlightName, *GestureName(Obs.Gesture));
			}
			else
			{
				Obs.TargetId = TEXT("-");
			}
			Obs.WaitReason = ComputeWaitReason(Obs);
			Obs.Key = FString::Printf(TEXT("%s|p%d|%s|e%d|c%d|i%d"),
				*Obs.MissionID.ToString(), Obs.PhaseIndex, *Obs.TargetId, Obs.ExplainStep,
				Obs.bReadyToClaim ? 1 : 0, Obs.bPanelIntro ? 1 : 0);
		}

		static FString ComputeWaitReason(const FObservation& Obs)
		{
			if (!Obs.bHasManager)
			{
				return TEXT("no-mission-manager");
			}
			// 패널 소개는 단계를 밖에서 못 읽는다 ― 누르는 중이어도 기다림으로 분류해 진전 없음 오판을 피한다
			if (Obs.bPanelIntro)
			{
				return TEXT("panel-intro");
			}
			// 첫 실행 헬기 오프닝은 공장 스포트라이트를 먼저 걸어 두고 끝나면 비춘다 ― 그동안의 화면 밖은 정상
			if (Obs.bCityOpening)
			{
				return TEXT("city-opening");
			}
			if (Obs.bLevelTransition)
			{
				return TEXT("level-transition");
			}
			if (Obs.bObserve)
			{
				return TEXT("observe");
			}
			if (Obs.bExplain)
			{
				return FString();
			}
			if (Obs.bReadyToClaim)
			{
				if (Obs.bClaimPresentable)
				{
					return Obs.bClaimRect ? FString() : FString(TEXT("claim-target-missing"));
				}
				if (!Obs.PromptTop.IsEmpty())
				{
					return TEXT("claim-deferred:prompt:") + Obs.PromptTop;
				}
				return Obs.bPlacement ? FString(TEXT("claim-deferred:placement")) : FString(TEXT("claim-deferred"));
			}
			if (Obs.MissionID.IsNone())
			{
				return Obs.bTutorialCompleted ? FString(TEXT("completed")) : FString(TEXT("no-active-mission"));
			}
			// 수익이 쌓이길 기다리는 정상 대기 ― 길이는 경제 기록(fund-wait)이 따로 잰다
			if (Obs.bFundWait)
			{
				return FString(TEXT("fund-wait:")) + EconomyKeyName(Obs.FundResource);
			}
			// 채용 연출 중엔 오버레이가 숨지만 [확인] 같은 대상은 그대로 지목된다 ― 누를 수 있으면 행동, 아니면 연출 기다림
			if (Obs.bRecruitPresentation && !(Obs.Target.IsValid() && Obs.bTargetActionable && Obs.bTargetRect && !Obs.bTargetCovered))
			{
				return TEXT("recruit-presentation");
			}
			if (Obs.Target.IsValid())
			{
				if (Obs.bTargetCovered)
				{
					return TEXT("prompt:") + Obs.PromptTop;
				}
				if (!Obs.bTargetActionable)
				{
					return TEXT("target-not-actionable:") + Obs.TargetReason;
				}
				return FString();
			}
			if (Obs.HasActorTarget())
			{
				if (Obs.PromptCount > 0 && !Obs.PromptTop.IsEmpty())
				{
					return TEXT("prompt:") + Obs.PromptTop;
				}
				return Obs.Gesture == EGestureHintKind::Drag ? FString(TEXT("drag-gesture")) : FString();
			}
			if (Obs.bPlacement)
			{
				return TEXT("placement");
			}
			if (!Obs.PromptTop.IsEmpty())
			{
				return TEXT("prompt:") + Obs.PromptTop;
			}
			if (Obs.OperationCount > 0)
			{
				return FString::Printf(TEXT("operation-running(%d)"), Obs.OperationCount);
			}
			return TEXT("unknown");
		}

		static bool IsReasonedWait(const FString& Reason)
		{
			return !Reason.IsEmpty() && Reason != TEXT("unknown");
		}

		// 클릭 효과로 인정하는 화면 변화 ― 진행도 수치는 넣지 않는다(벽돌처럼 저절로 오르는 값이 효과를 가린다)
		FString BuildFingerprint(const FObservation& Obs) const
		{
			return FString::Printf(TEXT("%d|%s|%s|%s|%d|%d|%d"),
				PhaseBroadcasts, *Obs.GuideLine, *Obs.MentorLine,
				*Obs.PromptTop, Obs.PromptCount, Obs.bPlacement ? 1 : 0, Obs.bClaimPresentable ? 1 : 0);
		}

		FString CurrentActionPrint() const
		{
			return LastFingerprint + TEXT("#") + CommittedKey;
		}

		// ===== 상태 커밋 =====
		void UpdateState(const FObservation& Obs, double Now)
		{
			const FString Fingerprint = BuildFingerprint(Obs);
			if (Fingerprint != LastFingerprint)
			{
				const bool bFirst = LastFingerprint.IsEmpty();
				LastFingerprint = Fingerprint;
				if (!bFirst)
				{
					// 키는 그대로인데 문구·화면이 바뀌었다 ― 진전·클릭 효과로 보고 다음 클릭은 다시 안정 대기
					LastProgressAt = Now;
					bStuckReported = false;
					bAwaitingEffect = false;
					AttemptCount = 0;
					ActNotBefore = FMath::Max(ActNotBefore, Now + ActStableSeconds);
					TooltipVisibleSince = Obs.bTooltip ? Now : 0.0;
				}
			}
			const FString ProgressPrint = FString::Printf(TEXT("%d|%lld|%d"), ProgressBroadcasts, Obs.ProgressCurrent, Obs.PlacedDesks);
			if (ProgressPrint != LastProgressPrint)
			{
				const bool bFirst = LastProgressPrint.IsEmpty();
				LastProgressPrint = ProgressPrint;
				if (!bFirst)
				{
					LastProgressAt = Now;
					bStuckReported = false;
					// 연속 배치처럼 같은 걸음이 여러 번 쌓이는 미션은 화면 키가 그대로다 ― 클릭 뒤 진행도가 오르면 먹힌 것.
					// 클릭 대기 중일 때만 본다: 벽돌처럼 저절로 오르는 진행도에 매번 안정 대기를 걸면 영영 누르지 않는다
					if (bAwaitingEffect)
					{
						bAwaitingEffect = false;
						AttemptCount = 0;
						ActNotBefore = FMath::Max(ActNotBefore, Now + ActStableSeconds);
					}
				}
			}

			const bool bMissionChanged = Obs.MissionID != CommittedMission;
			if (Obs.Key != CommittedKey)
			{
				if (Obs.Key != CandidateKey)
				{
					CandidateKey = Obs.Key;
					CandidateSince = Now;
					CandidateHits = 0;
				}
				++CandidateHits;
				// 한 번 스친 과도 상태는 버린다(왕복 오판 방지) ― 미션 전환은 즉시 기록
				if (CandidateHits >= 2 || bMissionChanged || CommittedKey.IsEmpty())
				{
					Commit(Obs, bMissionChanged ? Now : CandidateSince, Now);
				}
			}
			else
			{
				CandidateKey.Reset();
				CandidateHits = 0;
			}

			// 세이브 로드 전 기본값(false)을 미완료로 오인하지 않게 활성 미션이 보일 때만 센다
			if (!Obs.bTutorialCompleted && !Obs.MissionID.IsNone())
			{
				bSawIncomplete = true;
			}
			if (!Obs.GuideLine.IsEmpty() || Obs.bTooltip || Obs.bPanelIntro)
			{
				bMissionShowedGuide = true;
			}

			// 대상 사각형 안정성 ― 들어오는 애니메이션 중에 누르면 빗나간다
			const bool bRectNow = Obs.Target.IsValid() && Obs.bTargetRect;
			const bool bSameRect = bRectNow && bLastTargetRect
				&& FMath::Abs(Obs.TargetRect.Left - LastTargetRect.Left) <= 2.f
				&& FMath::Abs(Obs.TargetRect.Top - LastTargetRect.Top) <= 2.f
				&& FMath::Abs(Obs.TargetRect.Right - LastTargetRect.Right) <= 2.f
				&& FMath::Abs(Obs.TargetRect.Bottom - LastTargetRect.Bottom) <= 2.f;
			if (!bSameRect)
			{
				RectStableSince = Now;
			}
			bLastTargetRect = bRectNow;
			LastTargetRect = Obs.TargetRect;

			if (!Obs.bTooltip)
			{
				TooltipVisibleSince = 0.0;
			}
			else if (TooltipVisibleSince <= 0.0)
			{
				TooltipVisibleSince = Now;
			}

			if (bSettledShotPending && Obs.Key == CommittedKey && Obs.bOverlaySettled && Now - LastCommitAt >= SettledShotDelay)
			{
				bSettledShotPending = false;
				FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotsDir, FString::Printf(TEXT("%04d_%s_%s_s.png"), CommittedSeq,
					*SanitizeToken(Obs.MissionID.ToString()), *SanitizeToken(Obs.PhaseToken))), true, false);
			}
		}

		void Commit(const FObservation& Obs, double Since, double Now)
		{
			const FString PreviousKey = CommittedKey;
			if (Obs.MissionID != CommittedMission)
			{
				OnMissionChanged(Obs, Now);
			}
			CommittedKey = Obs.Key;
			CandidateKey.Reset();
			CandidateHits = 0;
			StateSince = Since;
			LastCommitAt = Now;
			ActNotBefore = Since + ActStableSeconds;
			LastProgressAt = Now;
			bStuckReported = false;
			bAwaitingEffect = false;
			AttemptCount = 0;
			ClickCount = 0;
			GatedNoTargetSince = 0.0;
			OffscreenSince = 0.0;
			PopupSince = 0.0;
			TooltipVisibleSince = Obs.bTooltip ? Now : 0.0;
			LastWaitReason.Reset();
			WaitSince = Now;

			++Seq;
			CommittedSeq = Seq;
			const FString ShotName = FString::Printf(TEXT("%04d_%s_%s.png"), Seq,
				*SanitizeToken(Obs.MissionID.ToString()), *SanitizeToken(Obs.PhaseToken));
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotsDir, ShotName), true, false);
			bSettledShotPending = true;
			WriteStateLine(Obs, Now, ShotName);

			if (!PreviousKey.IsEmpty())
			{
				const FString PairKey = PreviousKey < Obs.Key ? PreviousKey + TEXT(" <> ") + Obs.Key : Obs.Key + TEXT(" <> ") + PreviousKey;
				const int32 Transitions = ++PairTransitions.FindOrAdd(PairKey);
				if (Transitions >= PingPongTransitions)
				{
					ReportOnce(TEXT("Block"), TEXT("ping-pong"), PairKey,
						FString::Printf(TEXT("같은 두 걸음 사이를 %d번 오갔습니다: %s"), Transitions, *PairKey), Obs);
				}
			}
		}

		void OnMissionChanged(const FObservation& Obs, double Now)
		{
			if (!LastRealMission.IsNone() && CommittedMission == LastRealMission)
			{
				FMissionRecord& Record = MissionRecords.AddDefaulted_GetRef();
				Record.MissionID = LastRealMission;
				Record.Seconds = Now - MissionStartedAt;
				Record.bShowedGuide = bMissionShowedGuide;
				if (!bMissionShowedGuide)
				{
					AddIssue(TEXT("Warn"), TEXT("mission-no-guide"),
						FString::Printf(TEXT("%s 이(가) 안내 문구를 한 번도 띄우지 않고 통과했습니다."), *LastRealMission.ToString()));
				}
			}
			if (!Obs.MissionID.IsNone())
			{
				if (!LastRealMission.IsNone() && LastRealMission != Obs.MissionID && LastRealNext != Obs.MissionID)
				{
					AddIssue(TEXT("Block"), TEXT("mission-order"),
						FString::Printf(TEXT("%s 다음은 DT NextMissionID=%s 인데 %s 이(가) 활성화됐습니다."),
							*LastRealMission.ToString(), *LastRealNext.ToString(), *Obs.MissionID.ToString()));
				}
				LastRealMission = Obs.MissionID;
				LastRealNext = Obs.NextMissionID;
			}
			CommittedMission = Obs.MissionID;
			MissionStartedAt = Now;
			bMissionShowedGuide = false;
		}

		// ===== 판정 =====
		void Detect(const FObservation& Obs, double Now)
		{
			const bool bReasoned = IsReasonedWait(Obs.WaitReason);

			if (!bReasoned && !bStuckReported && Now - LastProgressAt > StuckSeconds)
			{
				bStuckReported = true;
				AddIssue(TEXT("Block"), TEXT("stuck"),
					FString::Printf(TEXT("%.0f초 동안 진전 없음(기다림 사유 없음). 대상=%s 사유=%s 마지막 행동=%s"),
						Now - LastProgressAt, *Obs.TargetId, *Obs.WaitReason, *ActionDesc));
			}

			if (Obs.bGated && !Obs.Target.IsValid() && !Obs.Spotlight.IsValid())
			{
				if (GatedNoTargetSince <= 0.0)
				{
					GatedNoTargetSince = Now;
				}
				else if (Now - GatedNoTargetSince >= TransientSeconds)
				{
					ReportOnce(TEXT("Block"), TEXT("gated-no-target"), CommittedKey,
						TEXT("입력 게이트가 켜졌는데 누를 대상(위젯/액터)이 없습니다."), Obs);
				}
			}
			else
			{
				GatedNoTargetSince = 0.0;
			}

			// 같은 걸음에서 반복 구매(인부 여러 명 고용 등)는 화면 키가 그대로다 ― 재화가 줄었으면 클릭이 먹힌 것
			if (bAwaitingEffect && ActionRes.bValid && Obs.Res.bValid)
			{
				for (int32 ResIndex = 0; ResIndex < EconomyKeyCount; ++ResIndex)
				{
					if (Obs.Res.Values[ResIndex] < ActionRes.Values[ResIndex])
					{
						bAwaitingEffect = false;
						LastProgressAt = Now;
						break;
					}
				}
			}
			if (bAwaitingEffect && Now - LastActionAt > ClickEffectSeconds)
			{
				bAwaitingEffect = false;
				AddIssue(TEXT("Block"), TEXT("click-no-effect"),
					FString::Printf(TEXT("눌렀지만 %.0f초 안에 상태가 바뀌지 않았습니다(시도 %d): %s"),
						ClickEffectSeconds, AttemptCount, *ActionDesc));
			}

			FString OffMessage;
			if (Obs.Target.IsValid() && !Obs.bTargetCovered)
			{
				if (Obs.TargetReason.Contains(TEXT("ZeroSize")) || (Obs.bTargetActionable && !Obs.bTargetRect))
				{
					OffMessage = TEXT("대상 위젯 크기가 0입니다: ") + Obs.TargetId;
				}
				else if (Obs.bTargetActionable && Obs.bViewport && !RectContains(Obs.ViewportRect, RectCenter(Obs.TargetRect)))
				{
					OffMessage = TEXT("대상 위젯 중심이 화면 밖입니다: ") + Obs.TargetId;
				}
			}
			else if (!Obs.Target.IsValid() && Obs.HasActorTarget() && Obs.PromptCount == 0 && !Obs.bCityOpening && !Obs.ActorOnScreen())
			{
				OffMessage = TEXT("대상 액터가 화면 밖이거나 투영되지 않습니다: ") + Obs.TargetId;
			}
			if (!OffMessage.IsEmpty())
			{
				if (OffscreenSince <= 0.0)
				{
					OffscreenSince = Now;
				}
				else if (Now - OffscreenSince >= TransientSeconds)
				{
					ReportOnce(TEXT("Block"), TEXT("target-offscreen"), CommittedKey, OffMessage, Obs);
				}
			}
			else
			{
				OffscreenSince = 0.0;
			}

			if ((Obs.Target.IsValid() || Obs.HasActorTarget()) && Obs.GuideLine.IsEmpty() && !Obs.MissionID.IsNone()
				&& !Obs.bRecruitPresentation && !Obs.bRewardToast && !Obs.bReadyToClaim && !Obs.bExplain && !Obs.bPanelIntro
				&& Now - StateSince >= ActStableSeconds)
			{
				ReportOnce(TEXT("Block"), TEXT("empty-guide-line"), CommittedKey,
					FString::Printf(TEXT("대상이 있는 걸음인데 GuideLines 문구가 비어 있습니다(%s %s)."), *Obs.PhaseToken, *Obs.TargetId), Obs);
			}

			if (Obs.bTooltip && Obs.bViewport && TooltipVisibleSince > 0.0 && Now - TooltipVisibleSince >= TooltipJudgeSeconds)
			{
				if (Obs.Target.IsValid() && Obs.bTargetRect)
				{
					bool bOverlap = false;
					const FSlateRect Overlap = Obs.TooltipRect.IntersectionWith(Obs.TargetRect, bOverlap);
					if (bOverlap && Overlap.GetArea() > 4.f)
					{
						ReportOnce(TEXT("Warn"), TEXT("tooltip-overlap"), CommittedKey,
							FString::Printf(TEXT("말풍선이 대상과 %.0fpx² 겹칩니다(%s)."), Overlap.GetArea(), *Obs.TargetId), Obs);
					}
				}
				if (!RectInside(Obs.TooltipRect, Obs.ViewportRect, 1.f))
				{
					ReportOnce(TEXT("Warn"), TEXT("tooltip-offscreen"), CommittedKey, TEXT("말풍선이 화면 밖으로 나갔습니다."), Obs);
				}
			}

			if (bReasoned)
			{
				if (Obs.WaitReason != LastWaitReason)
				{
					LastWaitReason = Obs.WaitReason;
					WaitSince = Now;
				}
				else if (Now - WaitSince > WaitWarnSeconds)
				{
					ReportOnce(TEXT("Warn"), TEXT("wait-long"), CommittedKey + TEXT("|") + Obs.WaitReason,
						FString::Printf(TEXT("기다림이 %.0f초를 넘었습니다(사유=%s)."), Now - WaitSince, *Obs.WaitReason), Obs);
				}
			}
			else
			{
				LastWaitReason.Reset();
			}

			// 안내가 기반 화면(월드/도크/트래커)을 가리키는데 프롬프트가 덮고 있거나, 프롬프트 위에서 안내가 아예 없다
			const bool bNoGuidance = !Obs.Target.IsValid() && !Obs.HasActorTarget() && Obs.GuideLine.IsEmpty();
			const bool bExpectsBase = !Obs.MissionID.IsNone() && !Obs.bPanelIntro && !Obs.bRecruitPresentation && (
				(Obs.Target.IsValid() && Obs.bTargetCovered)
				|| (!Obs.Target.IsValid() && Obs.HasActorTarget())
				|| (Obs.bReadyToClaim && !Obs.bClaimPresentable)
				|| (!Obs.bReadyToClaim && !Obs.bExplain && bNoGuidance));
			if (bExpectsBase && !Obs.PromptTop.IsEmpty())
			{
				if (PopupSince <= 0.0 || PopupClass != Obs.PromptTop)
				{
					PopupSince = Now;
					PopupClass = Obs.PromptTop;
				}
				else if (Now - PopupSince > PopupSeconds)
				{
					const FString OnceKey = TEXT("esc|") + CommittedKey + TEXT("|") + Obs.PromptTop;
					if (!ReportedOnce.Contains(OnceKey))
					{
						ReportedOnce.Add(OnceKey);
						AddIssue(TEXT("Warn"), TEXT("unexpected-popup"), TEXT("예상 밖 팝업 ") + Obs.PromptTop);
						bWantEscape = true;
					}
				}
			}
			else
			{
				PopupSince = 0.0;
			}
		}

		// ===== 행동 =====
		void Act(UWorld* GameWorld, const FObservation& Obs, double Now)
		{
			if (Pending != EPendingInput::None || !FSlateApplication::IsInitialized() || !Obs.bViewport)
			{
				return;
			}
			const FVector2D ViewportCenter = RectCenter(Obs.ViewportRect);

			if (Obs.bPanelIntro)
			{
				if (Now - StateSince >= ActStableSeconds && Now - LastIntroClickAt >= IntroClickInterval)
				{
					LastIntroClickAt = Now;
					ClickWidget(GameWorld, Obs, ViewportCenter, nullptr, TEXT("intro"), Now, false);
				}
				return;
			}
			if (bWantEscape)
			{
				bWantEscape = false;
				SendEscape(Obs, Now);
				return;
			}
			if (bAwaitingEffect || AttemptCount >= MaxAttemptsPerState || Now < ActNotBefore)
			{
				return;
			}
			if (ClickCount >= MaxClicksPerState)
			{
				ReportOnce(TEXT("Block"), TEXT("stuck"), TEXT("click-loop|") + CommittedKey,
					FString::Printf(TEXT("같은 걸음에서 %d번 눌렀지만 걸음이 바뀌지 않습니다."), ClickCount), Obs);
				return;
			}

			if (Obs.bReadyToClaim)
			{
				if (Obs.bClaimPresentable && Obs.bClaimRect && Obs.bOverlaySettled)
				{
					ClickWidget(GameWorld, Obs, RectCenter(Obs.ClaimRect), nullptr, TEXT("claim"), Now, true);
				}
				return;
			}
			if (Obs.bExplain)
			{
				if (Obs.bTooltip && Obs.bOverlaySettled && TooltipVisibleSince > 0.0 && Now - TooltipVisibleSince >= ActStableSeconds)
				{
					ClickWidget(GameWorld, Obs, ViewportCenter, nullptr, TEXT("explain"), Now, true);
				}
				return;
			}
			// 채용 연출 중엔 오버레이가 숨어 틱이 멈추므로 구멍 이동 완료 값을 믿지 않는다
			if (Obs.MissionID.IsNone() || (!Obs.bOverlaySettled && !Obs.bRecruitPresentation))
			{
				return;
			}
			// 못 사는 버튼을 연타하지 않는다 ― 처음 하는 플레이어도 비용이 모이길 기다린다
			if (Obs.bFundWait)
			{
				return;
			}

			// 관찰 걸음의 대상은 말풍선 자리일 뿐이다 ― 누르지 않고 상태가 바뀌길 기다린다. 전환 막이 덮은 동안도 같다
			if (Obs.bObserve || Obs.bLevelTransition)
			{
				return;
			}
			// 말풍선 확인은 누를 수 있는 대상이 정해진 뒤에만 ― 프롬프트에 가려 안내가 물러난 동안은 말풍선이 없는 게 정상이다
			if (UWidget* Target = Obs.Target.Get())
			{
				if (Obs.bTargetActionable && Obs.bTargetRect && !Obs.bTargetCovered
					&& RectContains(Obs.ViewportRect, RectCenter(Obs.TargetRect))
					&& Now - RectStableSince >= ActStableSeconds
					&& IsTooltipReady(Obs, Now))
				{
					ClickWidget(GameWorld, Obs, RectCenter(Obs.TargetRect), Target, TEXT("click"), Now, true);
				}
				return;
			}
			if (Obs.HasActorTarget() && Obs.PromptCount == 0 && Obs.Gesture != EGestureHintKind::Drag && Obs.ActorOnScreen()
				&& IsTooltipReady(Obs, Now))
			{
				PressWorld(GameWorld, Obs, Obs.ActorPoint(), Obs.Gesture == EGestureHintKind::Hold, Now);
			}
		}

		bool IsTooltipReady(const FObservation& Obs, double Now)
		{
			if (Obs.GuideLine.IsEmpty())
			{
				return true;
			}
			if (Obs.bTooltip && TooltipVisibleSince > 0.0)
			{
				return Now - TooltipVisibleSince >= ActStableSeconds;
			}
			if (Now - StateSince > TooltipGraceSeconds)
			{
				ReportOnce(TEXT("Warn"), TEXT("tooltip-missing"), CommittedKey,
					FString::Printf(TEXT("안내 문구가 있는데 %.0f초 동안 말풍선이 보이지 않습니다: %s"), TooltipGraceSeconds, *Obs.GuideLine), Obs);
				return true;
			}
			return false;
		}

		static TSharedPtr<FGenericWindow> GetNativeWindow(UWorld* GameWorld)
		{
			UGameViewportClient* ViewportClient = GameWorld ? GameWorld->GetGameViewport() : nullptr;
			if (!ViewportClient)
			{
				return TSharedPtr<FGenericWindow>();
			}
			const TSharedPtr<SWindow> GameWindow = ViewportClient->GetWindow();
			if (!GameWindow.IsValid())
			{
				return TSharedPtr<FGenericWindow>();
			}
			return GameWindow->GetNativeWindow();
		}

		void SendMove(const FVector2D& Pos, bool bHeld)
		{
			FSlateApplication& App = FSlateApplication::Get();
			if (CVarTutorialAutoMoveOSCursor.GetValueOnGameThread() != 0)
			{
				App.SetCursorPos(Pos);
			}
			const FPointerEvent Event(FSlateApplicationBase::CursorUserIndex, FSlateApplicationBase::CursorPointerIndex,
				Pos, LastPointerPos, bHeld ? LeftButton() : NoButtons(), EKeys::Invalid, 0.f, FModifierKeysState());
			App.ProcessMouseMoveEvent(Event);
			LastPointerPos = Pos;
		}

		void SendDown(UWorld* GameWorld, const FVector2D& Pos)
		{
			const FPointerEvent Event(FSlateApplicationBase::CursorUserIndex, FSlateApplicationBase::CursorPointerIndex,
				Pos, Pos, LeftButton(), EKeys::LeftMouseButton, 0.f, FModifierKeysState());
			FSlateApplication::Get().ProcessMouseButtonDownEvent(GetNativeWindow(GameWorld), Event);
		}

		void SendUp(const FVector2D& Pos)
		{
			const FPointerEvent Event(FSlateApplicationBase::CursorUserIndex, FSlateApplicationBase::CursorPointerIndex,
				Pos, Pos, NoButtons(), EKeys::LeftMouseButton, 0.f, FModifierKeysState());
			FSlateApplication::Get().ProcessMouseButtonUpEvent(Event);
		}

		// 누르기 전 그 자리의 히트 경로 ― 게이트 구멍이 실제로 대상까지 뚫렸는지 기록한다
		static FString DescribeHit(const FVector2D& Pos, const UWidget* Target, bool& bOutHitsTarget)
		{
			bOutHitsTarget = false;
			FSlateApplication& App = FSlateApplication::Get();
			const TArray<TSharedRef<SWindow>> TopWindows = App.GetInteractiveTopLevelWindows();
			const FWidgetPath Path = App.LocateWindowUnderMouse(Pos, TopWindows, false, static_cast<int32>(FSlateApplicationBase::CursorUserIndex));
			if (!Path.IsValid())
			{
				return TEXT("none");
			}
			if (Target)
			{
				const TSharedPtr<SWidget> TargetSlate = Target->GetCachedWidget();
				bOutHitsTarget = TargetSlate.IsValid() && Path.ContainsWidget(TargetSlate.Get());
			}
			const TSharedRef<SWidget> Leaf = Path.GetLastWidget();
			if (const TSharedPtr<FReflectionMetaData> Meta = FReflectionMetaData::GetWidgetOrParentMetaData(&Leaf.Get()))
			{
				const UObject* Source = Meta->SourceObject.Get();
				const UUserWidget* OwnerWidget = Source ? Source->GetTypedOuter<UUserWidget>() : nullptr;
				return FString::Printf(TEXT("%s/%s"), OwnerWidget ? *OwnerWidget->GetName() : TEXT("-"), *Meta->Name.ToString());
			}
			return Leaf->GetTypeAsString();
		}

		// bCounted = 걸음을 넘기려는 시도(효과 판정·시도 상한 대상). 소개 넘김·ESC 는 세지 않는다
		void BeginAction(const FObservation& Obs, const FString& Type, const FVector2D& Pos, const UWidget* Target, double Now, bool bCounted)
		{
			bool bHitsTarget = false;
			const FString Hit = DescribeHit(Pos, Target, bHitsTarget);
			if (bCounted)
			{
				++AttemptCount;
				++ClickCount;
				LastActionAt = Now;
				ActionPrint = CurrentActionPrint();
				ActNotBefore = Now + ActStableSeconds;
				ActionRes = Obs.Res;
			}
			ActionDesc = FString::Printf(TEXT("%s %s @(%.0f,%.0f) hit=%s hit_target=%d"), *Type, *Obs.TargetId,
				Pos.X - Obs.ViewportRect.Left, Pos.Y - Obs.ViewportRect.Top, *Hit, bHitsTarget ? 1 : 0);

			const TSharedRef<FJsonObject> Line = MakeShared<FJsonObject>();
			Line->SetStringField(TEXT("kind"), TEXT("action"));
			Line->SetNumberField(TEXT("seq"), ++Seq);
			Line->SetNumberField(TEXT("state_seq"), CommittedSeq);
			Line->SetField(TEXT("t"), JsonNum(Now - StartedAt));
			Line->SetStringField(TEXT("type"), Type);
			Line->SetStringField(TEXT("target"), Obs.TargetId);
			Line->SetField(TEXT("pos"), PointJson(Pos, Obs.ViewportRect));
			Line->SetStringField(TEXT("hit"), Hit);
			Line->SetBoolField(TEXT("hit_target"), bHitsTarget);
			Line->SetNumberField(TEXT("attempt"), AttemptCount);
			Line->SetObjectField(TEXT("res"), ResJson(Obs.Res));
			Line->SetNumberField(TEXT("hq_level"), Obs.HQLevel);
			AppendTimeline(Line);
		}

		void ClickWidget(UWorld* GameWorld, const FObservation& Obs, const FVector2D& Pos, const UWidget* Target, const TCHAR* Type,
			double Now, bool bExpectEffect)
		{
			const float FactoryHold = Obs.MissionID == TEXT("M2_UpgradeFactory")
				? FMath::Clamp(CVarTutorialAutoFactoryHoldSeconds.GetValueOnGameThread(), 0.f, 5.f) : 0.f;
			if (FactoryHold > 0.f)
			{
				BeginAction(Obs, TEXT("factory-ui-hold-down"), Pos, Target, Now, bExpectEffect);
				SendMove(Pos, false);
				SendDown(GameWorld, Pos);
				Pending = EPendingInput::HoldUp;
				PendingPos = Pos;
				PendingUpAt = Now + FactoryHold;
				PendingDownFrame = GFrameCounter;
				return;
			}
			BeginAction(Obs, Type, Pos, Target, Now, bExpectEffect);
			// 이동·누름·뗌을 한 프레임에 ― 사이에 Slate 합성 이동이 끼면 버튼 호버가 풀려 클릭이 무효가 된다
			SendMove(Pos, false);
			SendDown(GameWorld, Pos);
			SendUp(Pos);
			bAwaitingEffect = bExpectEffect;
		}

		void PressWorld(UWorld* GameWorld, const FObservation& Obs, const FVector2D& Pos, bool bHold, double Now)
		{
			BeginAction(Obs, bHold ? TEXT("hold-down") : TEXT("tap-down"), Pos, nullptr, Now, true);
			// 월드 입력은 다음 프레임 컨트롤러 틱에서 읽힌다 ― 같은 프레임에 떼면 누름이 사라질 수 있다
			SendMove(Pos, false);
			SendDown(GameWorld, Pos);
			Pending = bHold ? EPendingInput::HoldUp : EPendingInput::TapUp;
			PendingPos = Pos;
			PendingUpAt = Now + (bHold ? HoldSeconds : TapUpSeconds);
			PendingDownFrame = GFrameCounter;
		}

		void PumpInput(UWorld* GameWorld, double Now)
		{
			if (Pending == EPendingInput::None || !FSlateApplication::IsInitialized())
			{
				return;
			}
			if (Now < PendingUpAt || GFrameCounter < PendingDownFrame + 2)
			{
				if (Pending == EPendingInput::HoldUp && CVarTutorialAutoMoveOSCursor.GetValueOnGameThread() == 0)
				{
					SendMove(PendingPos, true);
				}
				return;
			}
			const bool bHold = Pending == EPendingInput::HoldUp;
			Pending = EPendingInput::None;
			SendUp(PendingPos);
			LastActionAt = Now;
			ActNotBefore = FMath::Max(ActNotBefore, Now + ActStableSeconds);
			// 누르는 동안 이미 걸음·화면이 바뀌었으면 효과는 확인된 것이다
			bAwaitingEffect = CurrentActionPrint() == ActionPrint;

			const TSharedRef<FJsonObject> Line = MakeShared<FJsonObject>();
			Line->SetStringField(TEXT("kind"), TEXT("action"));
			Line->SetNumberField(TEXT("seq"), ++Seq);
			Line->SetNumberField(TEXT("state_seq"), CommittedSeq);
			Line->SetField(TEXT("t"), JsonNum(Now - StartedAt));
			Line->SetStringField(TEXT("type"), bHold ? TEXT("hold-up") : TEXT("tap-up"));
			Line->SetBoolField(TEXT("world"), GameWorld != nullptr);
			Line->SetObjectField(TEXT("res"), ResJson(LastObs.Res));
			Line->SetNumberField(TEXT("hq_level"), LastObs.HQLevel);
			AppendTimeline(Line);
		}

		void ReleasePendingInput(const TCHAR* Why)
		{
			if (Pending == EPendingInput::None || !FSlateApplication::IsInitialized())
			{
				Pending = EPendingInput::None;
				return;
			}
			Pending = EPendingInput::None;
			SendUp(PendingPos);
			UE_LOG(LogTemp, Display, TEXT("[TutorialAuto] 누르던 입력을 놓았습니다(%s)."), Why);
		}

		void SendEscape(const FObservation& Obs, double Now)
		{
			BeginAction(Obs, TEXT("escape"), RectCenter(Obs.ViewportRect), nullptr, Now, false);
			FSlateApplication& App = FSlateApplication::Get();
			const FKeyEvent Event(EKeys::Escape, FModifierKeysState(), FSlateApplicationBase::CursorUserIndex, false, 0, 0x1B);
			App.ProcessKeyDownEvent(Event);
			App.ProcessKeyUpEvent(Event);
		}

		// 보상 카드가 다 나타난 뒤 한 장 ― 걸음 캡처는 등장 도중이라 앞 카드 몇 장만 찍힌다
		void CaptureRewardSplash(const FObservation& Obs)
		{
			if (!Obs.bRewardSplash)
			{
				bRewardShotTaken = false;
				return;
			}
			if (bRewardShotTaken || !Obs.bRewardSplashRevealed)
			{
				return;
			}
			bRewardShotTaken = true;
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotsDir, FString::Printf(TEXT("%04d_reward.png"), Seq)), true, false);
		}

		static FString WidgetText(UUserWidget* Owner, const TCHAR* Name)
		{
			const UTextBlock* Text = Owner ? Cast<UTextBlock>(Owner->GetWidgetFromName(FName(Name))) : nullptr;
			return Text ? Text->GetText().ToString() : FString();
		}

		// 화면에 보이는 공지 행 중 위에서 두 번째를 누른다 ― 없으면 false
		bool ClickSecondNotice(const FObservation& Obs, double Now)
		{
			UWorld* GameWorld = FindGameWorld();
			if (!GameWorld)
			{
				return false;
			}
			TArray<UUserWidget*> Found;
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GameWorld, Found, UNoticeRowWidget::StaticClass(), false);
			TArray<TPair<float, UUserWidget*>> Rows;
			for (UUserWidget* Row : Found)
			{
				FSlateRect Rect;
				if (Row && Row->IsVisible() && WidgetAbsRect(Row, Rect))
				{
					Rows.Emplace(Rect.Top, Row);
				}
			}
			Rows.Sort([](const TPair<float, UUserWidget*>& A, const TPair<float, UUserWidget*>& B) { return A.Key < B.Key; });
			if (Rows.Num() < 2)
			{
				AddIssue(TEXT("Info"), TEXT("notice-click"), FString::Printf(TEXT("보이는 공지 행 %d개 ― 두 번째 공지 검사 생략"), Rows.Num()), &Obs);
				return false;
			}
			UUserWidget* Second = Rows[1].Value;
			ProbeNoticeExpect = WidgetText(Second, TEXT("TitleText"));
			FSlateRect Rect;
			WidgetAbsRect(Second, Rect);
			ClickWidget(GameWorld, Obs, FVector2D((Rect.Left + Rect.Right) * 0.5f, (Rect.Top + Rect.Bottom) * 0.5f), Second, TEXT("click"), Now, false);
			return true;
		}

		void CheckNoticeBody(const FObservation& Obs)
		{
			UWorld* GameWorld = FindGameWorld();
			UGameInstance* GI = GameWorld ? GameWorld->GetGameInstance() : nullptr;
			UUIManagerSubsystem* UIMgr = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
			UUIBase* Base = UIMgr ? UIMgr->GetUIBase() : nullptr;
			const FString Shown = WidgetText(Base ? Base->GetActivePromptWidget() : nullptr, TEXT("NoticeTitleText"));
			const bool bMatch = !ProbeNoticeExpect.IsEmpty() && Shown == ProbeNoticeExpect;
			AddIssue(bMatch ? TEXT("Info") : TEXT("Warn"), TEXT("notice-click"),
				FString::Printf(TEXT("두 번째 공지 「%s」를 누른 뒤 본문 제목 「%s」 ― %s"), *ProbeNoticeExpect, *Shown,
					bMatch ? TEXT("일치") : TEXT("어긋남")), &Obs);
		}

		// 튜토리얼을 마친 세이브 ― 접속 자동 창이 어떤 순서로 뜨는지 기록하고 하나씩 닫는다
		void ProbeEntryPopups(const FObservation& Obs, double Now)
		{
			if (bBackProbe)
			{
				RunBackProbe(Obs, Now);
				return;
			}
			bProbeActive = true;
			if (ProbeStartedAt <= 0.0)
			{
				ProbeStartedAt = Now;
			}
			if (!Obs.PromptTop.IsEmpty())
			{
				ProbeIdleSince = 0.0;
				if (Obs.PromptTop != ProbeCurrent)
				{
					ProbeCurrent = Obs.PromptTop;
					ProbeSeenAt = Now;
					bProbeShotTaken = false;
					ProbeEscCount = 0;
					bProbeNoticeClicked = false;
					ProbeNoticeClickAt = 0.0;
					ProbeOrder.Add(Obs.PromptTop);
					AddIssue(TEXT("Info"), TEXT("entry-popup"),
						FString::Printf(TEXT("%d번째 자동 창: %s"), ProbeOrder.Num(), *Obs.PromptTop), &Obs);
				}
				if (!bProbeShotTaken && Now - ProbeSeenAt >= 1.5)
				{
					bProbeShotTaken = true;
					FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotsDir,
						FString::Printf(TEXT("%04d_entry%d_%s.png"), ++Seq, ProbeOrder.Num(), *SanitizeToken(Obs.PromptTop))), true, false);
				}
				else if (bProbeShotTaken && !bProbeNoticeClicked && Obs.PromptTop.Contains(TEXT("NewsPanel")))
				{
					// 소식 창이면 두 번째 공지를 한 번 눌러 본문이 따라오는지 본다(목록 선택 ↔ 본문 어긋남 검사)
					bProbeNoticeClicked = true;
					if (ClickSecondNotice(Obs, Now))
					{
						ProbeNoticeClickAt = Now;
					}
				}
				else if (ProbeNoticeClickAt > 0.0)
				{
					if (Now - ProbeNoticeClickAt >= 1.0)
					{
						ProbeNoticeClickAt = 0.0;
						CheckNoticeBody(Obs);
						FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotsDir,
							FString::Printf(TEXT("%04d_entry%d_notice2.png"), ++Seq, ProbeOrder.Num())), true, false);
						ProbeSeenAt = Now;
					}
				}
				else if (bProbeShotTaken && Now - ProbeSeenAt >= 3.0 && Now - ProbeLastEscAt >= 2.0)
				{
					ProbeLastEscAt = Now;
					// 뒤로가기(ESC)가 세 번 무시되면 결함으로 남기고 창을 직접 닫는다 ― 사람이 눌러 줄 때까지 멈춰 있지 않게
					if (++ProbeEscCount > 3)
					{
						if (ProbeEscCount == 4)
						{
							AddIssue(TEXT("Warn"), TEXT("back-ignored"),
								FString::Printf(TEXT("%s 이(가) 뒤로가기(ESC) 3번에 닫히지 않았습니다."), *Obs.PromptTop), &Obs);
						}
						UWorld* ProbeWorld = FindGameWorld();
						UGameInstance* ProbeGI = ProbeWorld ? ProbeWorld->GetGameInstance() : nullptr;
						UUIManagerSubsystem* ProbeUI = ProbeGI ? ProbeGI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
						UUIBase* ProbeBase = ProbeUI ? ProbeUI->GetUIBase() : nullptr;
						if (UCommonActivatableWidget* ProbeTop = ProbeBase ? ProbeBase->GetActivePromptWidget() : nullptr)
						{
							ProbeTop->DeactivateWidget();
						}
					}
					else
					{
						SendEscape(Obs, Now);
					}
				}
				return;
			}
			ProbeCurrent.Reset();
			if (ProbeIdleSince <= 0.0)
			{
				ProbeIdleSince = Now;
			}
			if (Now - ProbeIdleSince >= 8.0 || Now - ProbeStartedAt >= CVarTutorialAutoEntryProbe.GetValueOnGameThread())
			{
				AddIssue(TEXT("Info"), TEXT("entry-probe"),
					FString::Printf(TEXT("자동 창 %d개: %s"), ProbeOrder.Num(), *FString::Join(ProbeOrder, TEXT(" → "))));
				if (CVarTutorialAutoBackProbe.GetValueOnGameThread() > 0)
				{
					bBackProbe = true;
					BackPhaseAt = Now;
					return;
				}
				Finish(TEXT("entry-probe"), true);
			}
		}

		// ===== 뒤로가기 시험 ― 창을 하나씩 열고 ESC 로 닫히는지(겹치면 위의 것부터) 본다 =====
		enum class EBackKind : uint8
		{
			Prompt,
			BuildingSheet,
			EnterOffice,
			OfficeBottom,
			OfficeExit,
		};

		struct FBackStep
		{
			const TCHAR* Name;
			EWidgetType Panel;
			bool bDialog;
			EBackKind Kind = EBackKind::Prompt;
		};

		UUIBase* ProbeUIBase(UUIManagerSubsystem*& OutUI) const
		{
			UWorld* World = FindGameWorld();
			UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
			OutUI = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
			return OutUI ? OutUI->GetUIBase() : nullptr;
		}

		int32 ProbePromptCount() const
		{
			UUIManagerSubsystem* UI = nullptr;
			const UUIBase* Base = ProbeUIBase(UI);
			return Base ? Base->GetPromptStackCount() : -1;
		}

		bool IsBackDialogOpen() const
		{
			return BackDialog.IsValid() && BackDialog->IsInViewport();
		}

		bool OpenBackPanel(EWidgetType Type)
		{
			UUIManagerSubsystem* UI = nullptr;
			UUIBase* Base = ProbeUIBase(UI);
			UWorld* World = FindGameWorld();
			UTableManagerSubsystem* Tables = World ? World->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
			const TSubclassOf<UUserWidget> Class = Tables ? Tables->GetWidgetClass(Type) : nullptr;
			if (!Base || !Class || Base->GetPromptStackCount() > 0
				|| !Base->PushPromptClass(TSubclassOf<UCommonActivatableWidget>(Class)))
			{
				return false;
			}
			if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(World->GetFirstPlayerController()))
			{
				PC->GoToUIMode();
			}
			return true;
		}

		void BackShot(const FBackStep& Step, const TCHAR* Suffix)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotsDir,
				FString::Printf(TEXT("%04d_back_%s_%s.png"), ++Seq, Step.Name, Suffix)), true, false);
		}

		void BackVerdict(const FBackStep& Step, bool bPass, const FString& Detail, const FObservation& Obs)
		{
			if (bPass)
			{
				++BackPass;
			}
			else
			{
				BackFails.Add(Step.Name);
			}
			AddIssue(bPass ? TEXT("Info") : TEXT("Warn"), TEXT("back-nav"),
				FString::Printf(TEXT("%s: %s ― %s"), Step.Name, *Detail, bPass ? TEXT("통과") : TEXT("실패")), &Obs);
		}

		void RunBackProbe(const FObservation& Obs, double Now)
		{
			static const FBackStep Steps[] = {
				{TEXT("Menu"), EWidgetType::MenuPanel, false},
				{TEXT("Shop"), EWidgetType::ShopPanel, false},
				{TEXT("HQ"), EWidgetType::HQManagePanel, false},
				{TEXT("Build"), EWidgetType::BuildPanel, false},
				{TEXT("Ranking"), EWidgetType::RankingPanel, false},
				{TEXT("Settings"), EWidgetType::SettingsPanel, false},
				{TEXT("Codex"), EWidgetType::CodexPanel, false},
				{TEXT("Quest"), EWidgetType::QuestPanel, false},
				{TEXT("Mailbox"), EWidgetType::MailboxPanel, false},
				{TEXT("News"), EWidgetType::NewsPanel, false},
				{TEXT("Attendance"), EWidgetType::AttendancePanel, false},
				{TEXT("Dialog"), EWidgetType::None, true},
				{TEXT("MenuThenDialog"), EWidgetType::MenuPanel, true},
				{TEXT("NothingOpen"), EWidgetType::None, false},
				{TEXT("BuildingSheet"), EWidgetType::None, false, EBackKind::BuildingSheet},
				{TEXT("BuildingSheetThenDialog"), EWidgetType::None, true, EBackKind::BuildingSheet},
				{TEXT("EnterOffice"), EWidgetType::None, false, EBackKind::EnterOffice},
				{TEXT("OfficeDecoration"), EWidgetType::OfficeDecorationPanel, false, EBackKind::OfficeBottom},
				{TEXT("OfficeExit"), EWidgetType::None, false, EBackKind::OfficeExit},
			};
			if (BackStep >= UE_ARRAY_COUNT(Steps))
			{
				AddIssue(BackFails.Num() > 0 ? TEXT("Warn") : TEXT("Info"), TEXT("back-probe"),
					FString::Printf(TEXT("뒤로가기 시험 %d/%d 통과%s%s"), BackPass, int32(UE_ARRAY_COUNT(Steps)),
						BackFails.Num() > 0 ? TEXT(" ― 실패: ") : TEXT(""), *FString::Join(BackFails, TEXT(", "))));
				Finish(TEXT("back-probe"), BackFails.Num() == 0);
				return;
			}
			const FBackStep& Step = Steps[BackStep];
			const double Since = Now - BackPhaseAt;
			auto NextPhase = [this, Now](int32 Phase) { BackPhase = Phase; BackPhaseAt = Now; };
			auto NextStep = [this, Now]() { ++BackStep; BackPhase = 0; BackPhaseAt = Now; BackDialog.Reset(); BackRetry = 0; };
			if (Step.Kind == EBackKind::BuildingSheet)
			{
				// 건물을 누른 것처럼 아래 시트를 연다(+ 그 위 확인 창) ― 위의 것부터 닫혀야 한다
				UUIManagerSubsystem* UI = nullptr;
				UUIBase* Base = ProbeUIBase(UI);
				UWorld* World = FindGameWorld();
				AMainMapPlayerController* PC = World ? Cast<AMainMapPlayerController>(World->GetFirstPlayerController()) : nullptr;
				switch (BackPhase)
				{
				case 0:
				{
					if (Since < 1.0 || !Base || !PC)
					{
						return;
					}
					ABuildingBaseActor* Building = nullptr;
					for (TActorIterator<ABuildingBaseActor> It(World); It && !Building; ++It)
					{
						Building = *It;
					}
					if (!Building)
					{
						BackVerdict(Step, false, TEXT("누를 건물이 없음"), Obs);
						NextStep();
						return;
					}
					BackBottomBefore = Base->GetBottomStackCount();
					Building->OpenBuildingUI(PC);
					NextPhase(1);
					return;
				}
				case 1:
					if (Since < 1.5)
					{
						return;
					}
					if (Step.bDialog && UI)
					{
						BackDialog = UI->ShowConfirmDialog(FText::FromString(TEXT("뒤로가기 시험")),
							FText::FromString(TEXT("ESC 로 이 확인 창부터 닫혀야 합니다.")), FSimpleDelegate(), FSimpleDelegate());
					}
					NextPhase(2);
					return;
				case 2:
					if (Since < 1.2 || !Base)
					{
						return;
					}
					// 카메라가 건물로 날아간 뒤 시트가 오르는 경우가 있다 ― 6초까지 기다린 뒤에 판정한다
					if (Base->GetBottomStackCount() <= BackBottomBefore && Since < 6.0)
					{
						return;
					}
					BackShot(Step, TEXT("open"));
					if (Base->GetBottomStackCount() <= BackBottomBefore)
					{
						BackVerdict(Step, false, TEXT("건물 관리 시트가 6초 안에 열리지 않음"), Obs);
						NextStep();
						return;
					}
					NextPhase(3);
					return;
				case 3:
					if (Since < 0.4)
					{
						return;
					}
					SendEscape(Obs, Now);
					NextPhase(4);
					return;
				case 4:
				{
					if (Since < 1.5 || !Base)
					{
						return;
					}
					BackShot(Step, TEXT("esc1"));
					const int32 Bottom = Base->GetBottomStackCount();
					const bool bDialogOpen = IsBackDialogOpen();
					if (Step.bDialog)
					{
						if (bDialogOpen || Bottom <= BackBottomBefore)
						{
							BackVerdict(Step, false, FString::Printf(TEXT("ESC 1회 뒤 확인 창 %s, 시트 %s"), bDialogOpen ? TEXT("남음") : TEXT("닫힘"),
								Bottom > BackBottomBefore ? TEXT("남음") : TEXT("닫힘")), Obs);
							NextStep();
							return;
						}
						NextPhase(5);
						return;
					}
					BackVerdict(Step, Bottom == BackBottomBefore, FString::Printf(TEXT("ESC 1회 뒤 아래 시트 %d개(열기 전 %d개)"), Bottom, BackBottomBefore), Obs);
					NextStep();
					return;
				}
				case 5:
					if (Since < 0.4)
					{
						return;
					}
					SendEscape(Obs, Now);
					NextPhase(6);
					return;
				default:
				{
					if (Since < 1.5 || !Base)
					{
						return;
					}
					BackShot(Step, TEXT("esc2"));
					const int32 Bottom = Base->GetBottomStackCount();
					BackVerdict(Step, Bottom == BackBottomBefore, FString::Printf(TEXT("ESC 2회 뒤 아래 시트 %d개(열기 전 %d개)"), Bottom, BackBottomBefore), Obs);
					NextStep();
					return;
				}
				}
			}
			if (Step.Kind == EBackKind::EnterOffice)
			{
				if (BackPhase == 0)
				{
					if (Since < 1.0)
					{
						return;
					}
					UWorld* World = FindGameWorld();
					UCGGameInstance* GI = World ? Cast<UCGGameInstance>(World->GetGameInstance()) : nullptr;
					ABuildingBaseActor* Building = nullptr;
					for (TActorIterator<ABuildingBaseActor> It(World); It && !Building; ++It)
					{
						Building = *It;
					}
					if (!GI || !Building)
					{
						BackVerdict(Step, false, TEXT("들어갈 건물이 없음 ― 사무실 단계 생략"), Obs);
						BackStep = UE_ARRAY_COUNT(Steps);
						return;
					}
					GI->SetCurrentManagedBuilding(Building);
					GI->SetOfficeMode(EOfficeMode::Normal);
					GI->TransitionToLevel(TEXT("OfficeMap"));
					NextPhase(1);
					return;
				}
				if (BackPhase == 1)
				{
					if (Obs.MapName.Contains(TEXT("Office")))
					{
						NextPhase(2);
					}
					else if (Since > 60.0)
					{
						BackVerdict(Step, false, TEXT("60초 안에 사무실로 들어가지 못함"), Obs);
						BackStep = UE_ARRAY_COUNT(Steps);
					}
					return;
				}
				// 사무실 화면이 자리 잡을 때까지
				if (Since < 6.0)
				{
					return;
				}
				BackShot(Step, TEXT("open"));
				BackVerdict(Step, true, TEXT("사무실 진입"), Obs);
				NextStep();
				return;
			}
			if (Step.Kind == EBackKind::OfficeBottom)
			{
				UUIManagerSubsystem* UI = nullptr;
				UUIBase* Base = ProbeUIBase(UI);
				UWorld* World = FindGameWorld();
				UTableManagerSubsystem* Tables = World ? World->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
				if (BackPhase == 0)
				{
					if (Since < 1.5 || !Base || !Tables)
					{
						return;
					}
					BackBottomBefore = Base->GetBottomStackCount();
					const TSubclassOf<UUserWidget> Class = Tables->GetWidgetClass(Step.Panel);
					if (!Class || !Base->PushBottomClass(TSubclassOf<UCommonActivatableWidget>(Class)))
					{
						BackVerdict(Step, false, TEXT("창을 열지 못함"), Obs);
						NextStep();
						return;
					}
					NextPhase(1);
					return;
				}
				if (BackPhase == 1)
				{
					if (Since < 1.5 || !Base)
					{
						return;
					}
					BackShot(Step, TEXT("open"));
					if (Base->GetBottomStackCount() <= BackBottomBefore)
					{
						BackVerdict(Step, false, TEXT("아래 시트가 열리지 않음"), Obs);
						NextStep();
						return;
					}
					NextPhase(3);
					return;
				}
				if (BackPhase == 3)
				{
					if (Since >= 0.4)
					{
						SendEscape(Obs, Now);
						NextPhase(2);
					}
					return;
				}
				if (Since < 1.5 || !Base)
				{
					return;
				}
				BackShot(Step, TEXT("esc1"));
				const int32 Bottom = Base->GetBottomStackCount();
				const bool bStillOffice = Obs.MapName.Contains(TEXT("Office"));
				BackVerdict(Step, Bottom == BackBottomBefore && bStillOffice,
					FString::Printf(TEXT("ESC 1회 뒤 아래 시트 %d개(열기 전 %d개), 사무실 %s"), Bottom, BackBottomBefore, bStillOffice ? TEXT("유지") : TEXT("벗어남")), Obs);
				NextStep();
				return;
			}
			if (Step.Kind == EBackKind::OfficeExit)
			{
				if (BackPhase == 0)
				{
					if (Since < 2.0)
					{
						return;
					}
					BackShot(Step, TEXT("open"));
					NextPhase(2);
					return;
				}
				if (BackPhase == 2)
				{
					if (Since >= 0.4)
					{
						SendEscape(Obs, Now);
						NextPhase(1);
					}
					return;
				}
				if (Obs.MapName.Contains(TEXT("City")))
				{
					if (Since >= 4.0)
					{
						BackShot(Step, TEXT("after"));
						BackVerdict(Step, true, TEXT("열린 창 없이 ESC → 도시로 돌아옴"), Obs);
						NextStep();
					}
					return;
				}
				if (Since > 30.0)
				{
					BackShot(Step, TEXT("after"));
					BackVerdict(Step, false, TEXT("열린 창 없이 ESC 뒤 30초 동안 사무실에 머묾"), Obs);
					NextStep();
				}
				return;
			}
			switch (BackPhase)
			{
			case 0:
				if (Since < 1.0)
				{
					return;
				}
				if (ProbePromptCount() != 0 || IsBackDialogOpen())
				{
					// 앞 단계가 창을 남겼으면 직접 치우고 넘어간다 ― 다음 단계가 영영 시작 못 하지 않게
					if (Since >= 10.0)
					{
						UUIManagerSubsystem* UI = nullptr;
						if (UUIBase* Base = ProbeUIBase(UI))
						{
							if (UCommonActivatableWidget* Left = Base->GetActivePromptWidget())
							{
								Left->DeactivateWidget();
							}
						}
						if (UI)
						{
							UI->CloseCurrentDialog();
						}
						BackPhaseAt = Now;
					}
					return;
				}
				if (Step.Panel != EWidgetType::None && !OpenBackPanel(Step.Panel))
				{
					BackVerdict(Step, false, TEXT("창을 열지 못함"), Obs);
					NextStep();
					return;
				}
				NextPhase(1);
				return;
			case 1:
				if (Since < 1.2)
				{
					return;
				}
				if (Step.bDialog)
				{
					UUIManagerSubsystem* UI = nullptr;
					ProbeUIBase(UI);
					BackDialog = UI ? UI->ShowConfirmDialog(FText::FromString(TEXT("뒤로가기 시험")),
						FText::FromString(TEXT("ESC 로 이 확인 창부터 닫혀야 합니다.")), FSimpleDelegate(), FSimpleDelegate()) : nullptr;
				}
				NextPhase(2);
				return;
			case 2:
			{
				if (Since < 1.2)
				{
					return;
				}
				// 맨 위가 이 단계가 연 창이 아니면(아무것도 안 연 단계는 창이 생겼으면) 게임 쪽 창이 끼어든 것이다 ―
				// 그대로 ESC 를 재면 남의 창을 닫고 실패로 적힌다
				UUIManagerSubsystem* UI = nullptr;
				UUIBase* Base = ProbeUIBase(UI);
				const UCommonActivatableWidget* Top = Base ? Base->GetActivePromptWidget() : nullptr;
				UWorld* World = FindGameWorld();
				UTableManagerSubsystem* Tables = World ? World->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
				const TSubclassOf<UUserWidget> Expected = Tables && Step.Panel != EWidgetType::None ? Tables->GetWidgetClass(Step.Panel) : nullptr;
				const bool bIntruded = Step.Panel != EWidgetType::None
					? (!Top || !Expected || !Top->GetClass()->IsChildOf(Expected))
					: ProbePromptCount() != 0;
				if (bIntruded)
				{
					const FString Intruder = Top ? Top->GetClass()->GetName() : FString(TEXT("없음"));
					if (BackRetry < 2)
					{
						++BackRetry;
						AddIssue(TEXT("Info"), TEXT("back-nav"), FString::Printf(TEXT("%s: 맨 위가 %s ― 게임 쪽 창 간섭, 치우고 다시(%d회)"),
							Step.Name, *Intruder, BackRetry), &Obs);
						while (UCommonActivatableWidget* Left = Base ? Base->GetActivePromptWidget() : nullptr)
						{
							Left->DeactivateWidget();
							if (Base->GetActivePromptWidget() == Left)
							{
								break;
							}
						}
						if (UI)
						{
							UI->CloseCurrentDialog();
						}
						BackDialog.Reset();
						NextPhase(0);
						return;
					}
					BackShot(Step, TEXT("open"));
					BackVerdict(Step, false, FString::Printf(TEXT("맨 위가 %s ― 게임 쪽 창이 계속 끼어듦"), *Intruder), Obs);
					NextStep();
					return;
				}
				BackShot(Step, TEXT("open"));
				NextPhase(5);
				return;
			}
			case 5:
				// 사진은 프레임 끝에 찍힌다 ― 같은 프레임에 ESC 를 보내면 닫힌 뒤가 찍힌다
				if (Since < 0.4)
				{
					return;
				}
				SendEscape(Obs, Now);
				NextPhase(3);
				return;
			case 3:
			{
				if (Since < 1.5)
				{
					return;
				}
				BackShot(Step, TEXT("esc1"));
				const int32 Prompts = ProbePromptCount();
				const bool bDialogOpen = IsBackDialogOpen();
				if (Step.bDialog && Step.Panel != EWidgetType::None)
				{
					// 위의 확인 창만 닫히고 아래 창은 남아야 한다
					if (bDialogOpen || Prompts != 1)
					{
						BackVerdict(Step, false, FString::Printf(TEXT("ESC 1회 뒤 확인 창 %s, 아래 창 %d개"), bDialogOpen ? TEXT("남음") : TEXT("닫힘"), Prompts), Obs);
						NextStep();
						return;
					}
					NextPhase(6);
					return;
				}
				const bool bPass = !bDialogOpen && Prompts == 0;
				BackVerdict(Step, bPass, FString::Printf(TEXT("ESC 1회 뒤 창 %d개, 확인 창 %s"), Prompts, bDialogOpen ? TEXT("남음") : TEXT("없음")), Obs);
				NextStep();
				return;
			}
			case 6:
				if (Since < 0.4)
				{
					return;
				}
				SendEscape(Obs, Now);
				NextPhase(4);
				return;
			case 4:
			{
				if (Since < 1.5)
				{
					return;
				}
				BackShot(Step, TEXT("esc2"));
				const int32 Prompts = ProbePromptCount();
				BackVerdict(Step, Prompts == 0, FString::Printf(TEXT("ESC 2회 뒤 창 %d개"), Prompts), Obs);
				NextStep();
				return;
			}
			default:
				NextStep();
				return;
			}
		}

		// ===== 종료 =====
		void CheckEnd(const FObservation& Obs, double Now)
		{
			if (Obs.bTutorialCompleted)
			{
				if (!bSawIncomplete)
				{
					if (CVarTutorialAutoEntryProbe.GetValueOnGameThread() > 0)
					{
						ProbeEntryPopups(Obs, Now);
						return;
					}
					AddIssue(TEXT("Block"), TEXT("already-completed"), TEXT("시작 시점에 이미 튜토리얼이 완료돼 있습니다(새 게임이 아님)."));
					Finish(TEXT("already-completed-at-start"), false);
					return;
				}
				// 마지막 보상 창이 닫힌 뒤 화면(뒤로 미룬 알림·성장 도움말)까지 끝 장면으로 남긴다
				if (CompletedSeenAt <= 0.0)
				{
					CompletedSeenAt = Now;
				}
				const bool bWithinGrace = Now - CompletedSeenAt < FinalSceneMaxSeconds;
				if (Obs.bRewardSplash)
				{
					RewardClosedAt = 0.0;
					if (bWithinGrace)
					{
						return;
					}
				}
				else
				{
					if (RewardClosedAt <= 0.0)
					{
						RewardClosedAt = Now;
					}
					if (!Obs.PromptTop.IsEmpty() && FinalPromptSeenAt <= 0.0)
					{
						FinalPromptSeenAt = Now;
					}
					const bool bSettled = FinalPromptSeenAt > 0.0
						? Now - FinalPromptSeenAt >= FinalSceneSettleSeconds
						: Now - RewardClosedAt >= FinalSceneEntryWaitSeconds;
					if (bWithinGrace && !bSettled)
					{
						return;
					}
				}
				Finish(TEXT("tutorial-completed"), true);
				return;
			}
			const double Idle = Now - LastProgressAt;
			if (!IsReasonedWait(Obs.WaitReason) && bStuckReported && Idle > StuckEndSeconds)
			{
				Finish(TEXT("stuck"), false);
			}
			else if (Now - LastCommitAt > StepEndSeconds)
			{
				// 저절로 오르는 진행도가 진전 없음 판정을 계속 미루는 경우의 마지막 안전판
				AddIssue(TEXT("Block"), TEXT("stuck"),
					FString::Printf(TEXT("%.0f초 동안 걸음이 바뀌지 않았습니다(사유=%s)."), Now - LastCommitAt, *Obs.WaitReason));
				Finish(TEXT("no-step-change"), false);
			}
			else if (IsReasonedWait(Obs.WaitReason) && Idle > WaitEndSeconds)
			{
				AddIssue(TEXT("Block"), TEXT("wait-timeout"),
					FString::Printf(TEXT("%.0f초 동안 기다림에서 벗어나지 못했습니다(사유=%s)."), Idle, *Obs.WaitReason));
				Finish(TEXT("wait-timeout"), false);
			}
		}

		void Finish(const FString& EndReason, bool bCompleted)
		{
			if (!bRunning)
			{
				return;
			}
			ReleasePendingInput(TEXT("finish"));
			const double Now = FPlatformTime::Seconds();
			DrainLog();
			if (!LastRealMission.IsNone() && CommittedMission == LastRealMission)
			{
				FMissionRecord& Record = MissionRecords.AddDefaulted_GetRef();
				Record.MissionID = LastRealMission;
				Record.Seconds = Now - MissionStartedAt;
				Record.bShowedGuide = bMissionShowedGuide;
			}
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotsDir, FString::Printf(TEXT("%04d_final.png"), Seq + 1)), true, false);

			const int32 BlockCount = CountIssues(TEXT("Block"));
			const int32 WarnCount = CountIssues(TEXT("Warn"));
			const TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
			Summary->SetStringField(TEXT("result"), (bCompleted && BlockCount == 0) ? TEXT("pass") : TEXT("fail"));
			Summary->SetStringField(TEXT("end_reason"), EndReason);
			Summary->SetBoolField(TEXT("tutorial_completed"), bCompleted);
			Summary->SetNumberField(TEXT("block_count"), BlockCount);
			Summary->SetNumberField(TEXT("warn_count"), WarnCount);
			Summary->SetField(TEXT("total_seconds"), JsonNum(Now - StartedAt));
			Summary->SetNumberField(TEXT("time_scale"), TimeScale);
			Summary->SetNumberField(TEXT("timeline_lines"), Seq);
			Summary->SetNumberField(TEXT("log_error_lines"), LogErrorLines);
			TArray<TSharedPtr<FJsonValue>> Missions;
			for (const FMissionRecord& Record : MissionRecords)
			{
				const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetStringField(TEXT("mission"), Record.MissionID.ToString());
				Item->SetField(TEXT("seconds"), JsonNum(Record.Seconds));
				Item->SetBoolField(TEXT("showed_guide"), Record.bShowedGuide);
				Missions.Add(MakeShared<FJsonValueObject>(Item));
			}
			Summary->SetArrayField(TEXT("missions"), Missions);
			Summary->SetObjectField(TEXT("economy"), BuildEconomySummary(Now));
			Summary->SetObjectField(TEXT("last_state"), ObservationJson(LastObs));
			Summary->SetStringField(TEXT("finished"), FDateTime::Now().ToIso8601());
			WriteIssues();
			FFileHelper::SaveStringToFile(JsonToString(Summary, true), *FPaths::Combine(OutDir, TEXT("summary.json")),
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

			const TSharedRef<FJsonObject> EndLine = MakeShared<FJsonObject>();
			EndLine->SetStringField(TEXT("kind"), TEXT("end"));
			EndLine->SetNumberField(TEXT("seq"), ++Seq);
			EndLine->SetField(TEXT("t"), JsonNum(Now - StartedAt));
			EndLine->SetStringField(TEXT("end_reason"), EndReason);
			AppendTimeline(EndLine);

			if (GLog && LogCapture)
			{
				GLog->RemoveOutputDevice(LogCapture);
			}
			UnbindManager();
			FCoreDelegates::OnEnginePreExit.Remove(PreExitHandle);
			PreExitHandle.Reset();
			// 틱 안에서는 false 반환으로 스스로 빠진다
			if (!bInTick && TickerHandle.IsValid())
			{
				FTSTicker::RemoveTicker(TickerHandle);
			}
			TickerHandle.Reset();
			bRunning = false;
			UE_LOG(LogTemp, Display, TEXT("[TutorialAuto] 종료 reason=%s result=%s Block=%d Warn=%d -> %s"),
				*EndReason, (bCompleted && BlockCount == 0) ? TEXT("pass") : TEXT("fail"), BlockCount, WarnCount, *OutDir);
		}

		// ===== 경제 =====
		void TrackEconomy(const FObservation& Obs, double Now)
		{
			const double StepSeconds = LastEconomyAt > 0.0 ? FMath::Min(Now - LastEconomyAt, EconomyMaxStepSeconds) : 0.0;
			LastEconomyAt = Now;

			if (Obs.Res.bValid && LastEconomyRes.bValid)
			{
				for (int32 ResIndex = 0; ResIndex < EconomyKeyCount; ++ResIndex)
				{
					const int64 Before = LastEconomyRes.Values[ResIndex];
					const int64 After = Obs.Res.Values[ResIndex];
					if (After < Before)
					{
						MissionSpent[ResIndex] += Before - After;
						RunSpent[ResIndex] += Before - After;
						WriteSpendLine(ResIndex, Before, After, Now);
					}
				}
			}
			if (Obs.Res.bValid)
			{
				LastEconomyRes = Obs.Res;
			}

			if (Obs.bHasManager && Obs.MissionID != EconomyMission)
			{
				EndFundWait(Obs, Now);
				CloseEconomyMission(Obs.Res, Now);
				EconomyMission = Obs.MissionID;
				EconomyMissionStartedAt = Now;
				EconomyStartRes = Obs.Res;
				for (int32 ResIndex = 0; ResIndex < EconomyKeyCount; ++ResIndex)
				{
					MissionSpent[ResIndex] = 0;
				}
				MissionFundWait = 0.0;
				MissionAffordable.Reset();
				AffordWaitSince = 0.0;
				// 본사 Lv2 비용(MoneyCost) 보정용 ― 레벨업 미션에 들어선 순간의 자금
				if (Obs.Condition == EMissionConditionType::ReachHQLevel && HQStepMoney < 0 && Obs.Res.bValid)
				{
					HQStepMoney = Obs.Res.Values[0];
					HQStepMission = Obs.MissionID.ToString();
				}
			}

			if (Obs.bFundWait)
			{
				MissionFundWait += StepSeconds;
				RunFundWait += StepSeconds;
				if (FundWaitSince <= 0.0)
				{
					FundWaitSince = Now;
					FundWaitResource = Obs.FundResource;
					FundWaitNeeded = Obs.FundNeeded;
					FundWaitOwnedStart = Obs.Res.bValid && Obs.FundResource >= 0 ? Obs.Res.Values[Obs.FundResource] : -1;
					FundWaitMission = Obs.MissionID.ToString();
					FundWaitPhase = Obs.PhaseToken;
					FundWaitTarget = Obs.TargetId;
					bFundWaitWarned = false;
					if (AffordWaitSince <= 0.0)
					{
						AffordWaitSince = Now;
					}
				}
				const double WaitedGameSeconds = (Now - FundWaitSince) * TimeScale;
				if (!bFundWaitWarned && WaitedGameSeconds > FundWaitWarnSeconds)
				{
					bFundWaitWarned = true;
					const int64 Owned = Obs.Res.bValid && Obs.FundResource >= 0 ? Obs.Res.Values[Obs.FundResource] : -1;
					FString Detail = Obs.FundNeeded >= 0
						? FString::Printf(TEXT("필요 %lld / 보유 %lld"), Obs.FundNeeded, Owned)
						: FString::Printf(TEXT("필요 미상 / 보유 %lld"), Owned);
					if (Obs.FundNeeded >= 0 && Owned >= Obs.FundNeeded)
					{
						Detail += TEXT(" (재화는 충분 ― 다른 조건 미달)");
					}
					ReportOnce(TEXT("Warn"), TEXT("fund-wait-long"), FundWaitMission + TEXT("|") + FundWaitPhase,
						FString::Printf(TEXT("%s 부족으로 게임 시간 %.0f초 넘게 기다렸습니다(%s, 대상=%s)."),
							EconomyKeyName(Obs.FundResource), WaitedGameSeconds, *Detail, *Obs.TargetId), Obs);
				}
			}
			else
			{
				EndFundWait(Obs, Now);
			}

			// 안내된 구매 버튼이 처음 살 수 있게 된 순간 ― 「기본 비용이 자연스러운가」 지표(공장 M1 인부·증축 포함)
			if (Obs.bTargetCostButton && Obs.bTargetAffordable && !Obs.bReadyToClaim)
			{
				const FString AffordKey = Obs.MissionID.ToString() + TEXT("|") + Obs.PhaseToken + TEXT("|") + Obs.TargetId;
				if (!AffordReported.Contains(AffordKey))
				{
					AffordReported.Add(AffordKey);
					const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
					Item->SetStringField(TEXT("mission"), Obs.MissionID.ToString());
					Item->SetStringField(TEXT("phase"), Obs.PhaseToken);
					Item->SetStringField(TEXT("target"), Obs.TargetId);
					Item->SetStringField(TEXT("resource"), EconomyKeyName(Obs.TargetCostResource));
					Item->SetNumberField(TEXT("cost"), static_cast<double>(Obs.TargetCost));
					Item->SetNumberField(TEXT("owned"), Obs.Res.bValid && Obs.TargetCostResource >= 0
						? static_cast<double>(Obs.Res.Values[Obs.TargetCostResource]) : -1.0);
					Item->SetField(TEXT("since_mission_start_game_s"), JsonNum((Now - EconomyMissionStartedAt) * TimeScale));
					Item->SetField(TEXT("waited_game_s"), JsonNum(AffordWaitSince > 0.0 ? (Now - AffordWaitSince) * TimeScale : 0.0));
					MissionAffordable.Add(MakeShared<FJsonValueObject>(Item));

					const TSharedRef<FJsonObject> Line = MakeShared<FJsonObject>();
					Line->SetStringField(TEXT("kind"), TEXT("affordable"));
					Line->SetNumberField(TEXT("seq"), ++Seq);
					Line->SetNumberField(TEXT("state_seq"), CommittedSeq);
					Line->SetField(TEXT("t"), JsonNum(Now - StartedAt));
					Line->SetObjectField(TEXT("detail"), Item);
					AppendTimeline(Line);
				}
				AffordWaitSince = 0.0;
			}
		}

		void WriteSpendLine(int32 ResIndex, int64 Before, int64 After, double Now)
		{
			const TSharedRef<FJsonObject> Line = MakeShared<FJsonObject>();
			Line->SetStringField(TEXT("kind"), TEXT("spend"));
			Line->SetNumberField(TEXT("seq"), ++Seq);
			Line->SetNumberField(TEXT("state_seq"), CommittedSeq);
			Line->SetField(TEXT("t"), JsonNum(Now - StartedAt));
			Line->SetStringField(TEXT("resource"), EconomyKeyName(ResIndex));
			Line->SetNumberField(TEXT("amount"), static_cast<double>(Before - After));
			Line->SetNumberField(TEXT("balance_before"), static_cast<double>(Before));
			Line->SetNumberField(TEXT("balance_after"), static_cast<double>(After));
			Line->SetStringField(TEXT("mission"), LastObs.MissionID.ToString());
			Line->SetStringField(TEXT("phase"), LastObs.PhaseToken);
			Line->SetStringField(TEXT("target"), LastObs.TargetId);
			Line->SetNumberField(TEXT("hq_level"), LastObs.HQLevel);
			AppendTimeline(Line);
		}

		void EndFundWait(const FObservation& Obs, double Now)
		{
			if (FundWaitSince <= 0.0)
			{
				return;
			}
			const TSharedRef<FJsonObject> Line = MakeShared<FJsonObject>();
			Line->SetStringField(TEXT("kind"), TEXT("fund-wait"));
			Line->SetNumberField(TEXT("seq"), ++Seq);
			Line->SetNumberField(TEXT("state_seq"), CommittedSeq);
			Line->SetField(TEXT("t"), JsonNum(Now - StartedAt));
			Line->SetStringField(TEXT("mission"), FundWaitMission);
			Line->SetStringField(TEXT("phase"), FundWaitPhase);
			Line->SetStringField(TEXT("target"), FundWaitTarget);
			Line->SetStringField(TEXT("resource"), EconomyKeyName(FundWaitResource));
			Line->SetNumberField(TEXT("needed"), static_cast<double>(FundWaitNeeded));
			Line->SetNumberField(TEXT("owned_start"), static_cast<double>(FundWaitOwnedStart));
			Line->SetNumberField(TEXT("owned_end"), Obs.Res.bValid && FundWaitResource >= 0
				? static_cast<double>(Obs.Res.Values[FundWaitResource]) : -1.0);
			Line->SetField(TEXT("seconds"), JsonNum(Now - FundWaitSince));
			Line->SetField(TEXT("game_seconds"), JsonNum((Now - FundWaitSince) * TimeScale));
			AppendTimeline(Line);
			FundWaitSince = 0.0;
		}

		void CloseEconomyMission(const FResourceSnapshot& EndRes, double Now)
		{
			if (EconomyMission.IsNone())
			{
				return;
			}
			const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(TEXT("mission"), EconomyMission.ToString());
			Item->SetField(TEXT("seconds"), JsonNum(Now - EconomyMissionStartedAt));
			Item->SetField(TEXT("game_seconds"), JsonNum((Now - EconomyMissionStartedAt) * TimeScale));
			Item->SetObjectField(TEXT("start_res"), ResJson(EconomyStartRes));
			Item->SetObjectField(TEXT("end_res"), ResJson(EndRes));
			Item->SetObjectField(TEXT("spent"), AmountsJson(MissionSpent));
			Item->SetField(TEXT("fund_wait_seconds"), JsonNum(MissionFundWait));
			Item->SetField(TEXT("fund_wait_game_seconds"), JsonNum(MissionFundWait * TimeScale));
			Item->SetArrayField(TEXT("affordable"), MissionAffordable);
			EconomyMissions.Add(MakeShared<FJsonValueObject>(Item));
		}

		TSharedRef<FJsonObject> BuildEconomySummary(double Now)
		{
			EndFundWait(LastObs, Now);
			CloseEconomyMission(LastObs.Res, Now);
			EconomyMission = NAME_None;
			const TSharedRef<FJsonObject> Totals = MakeShared<FJsonObject>();
			Totals->SetObjectField(TEXT("spent"), AmountsJson(RunSpent));
			Totals->SetField(TEXT("fund_wait_seconds"), JsonNum(RunFundWait));
			Totals->SetField(TEXT("fund_wait_game_seconds"), JsonNum(RunFundWait * TimeScale));
			Totals->SetObjectField(TEXT("end_res"), ResJson(LastObs.Res));
			Totals->SetNumberField(TEXT("end_hq_level"), LastObs.HQLevel);

			const TSharedRef<FJsonObject> Economy = MakeShared<FJsonObject>();
			Economy->SetNumberField(TEXT("time_scale"), TimeScale);
			Economy->SetField(TEXT("fund_wait_warn_game_seconds"), JsonNum(FundWaitWarnSeconds));
			Economy->SetArrayField(TEXT("missions"), EconomyMissions);
			Economy->SetObjectField(TEXT("totals"), Totals);
			Economy->SetNumberField(TEXT("hq_step_money"), static_cast<double>(HQStepMoney));
			Economy->SetStringField(TEXT("hq_step_mission"), HQStepMission);
			return Economy;
		}

		// ===== 기록 =====
		void DrainLog()
		{
			if (!LogCapture)
			{
				return;
			}
			TArray<FCapturedLine> Lines;
			LogCapture->Drain(Lines);
			const double Now = FPlatformTime::Seconds();
			for (const FCapturedLine& Line : Lines)
			{
				if (Line.Text.Contains(TEXT("[MissionGuide]")))
				{
					// 숫자(초·단계)만 다른 같은 경고는 한 건으로 묶는다
					FString Normalized = Line.Text;
					for (TCHAR& Ch : Normalized)
					{
						if (FChar::IsDigit(Ch))
						{
							Ch = TEXT('#');
						}
					}
					// 정보 로그(래치 기록 등)는 진행 기록일 뿐 ― 경고 이상만 문제로, 「이미 충족」 건너뛰기는 설명 생략이라 Warn
					if (Line.Verbosity <= ELogVerbosity::Warning)
					{
						ReportOnce(TEXT("Block"), TEXT("missionguide-log"), Normalized, Line.Text, LastObs);
					}
					else if (Line.Text.Contains(TEXT("건너뜁니다")))
					{
						ReportOnce(TEXT("Warn"), TEXT("mission-skipped"), Normalized, Line.Text, LastObs);
					}
				}
				else if (Line.Text.Contains(TEXT("Ensure condition failed")) || Line.Text.Contains(TEXT("Assertion failed")))
				{
					ReportOnce(TEXT("Block"), TEXT("ensure"), Line.Text.Left(160), Line.Text.Left(600), LastObs);
				}
				if (LogErrorLines < MaxLogErrorLines)
				{
					++LogErrorLines;
					FFileHelper::SaveStringToFile(
						FString::Printf(TEXT("[%.1f][%s][%s] %s\n"), Now - StartedAt, *Line.Category.ToString(),
							::ToString(Line.Verbosity), *Line.Text),
						*FPaths::Combine(OutDir, TEXT("log_errors.txt")),
						FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append | FILEWRITE_AllowRead);
				}
			}
		}

		void ReportOnce(const TCHAR* Severity, const TCHAR* Code, const FString& OnceKey, const FString& Message, const FObservation& Obs)
		{
			const FString Full = FString::Printf(TEXT("%s|%s"), Code, *OnceKey);
			if (ReportedOnce.Contains(Full))
			{
				return;
			}
			ReportedOnce.Add(Full);
			AddIssue(Severity, Code, Message, &Obs);
		}

		void AddIssue(const TCHAR* Severity, const TCHAR* Code, const FString& Message, const FObservation* Obs = nullptr)
		{
			FIssue& Issue = Issues.AddDefaulted_GetRef();
			Issue.Severity = Severity;
			Issue.Code = Code;
			Issue.Mission = Obs && !Obs->MissionID.IsNone() ? Obs->MissionID.ToString() : CommittedMission.ToString();
			Issue.Phase = Obs ? Obs->PhaseToken : LastObs.PhaseToken;
			Issue.Message = Message;
			Issue.TimelineSeq = CommittedSeq;
			WriteIssues();
			UE_LOG(LogTemp, Warning, TEXT("[TutorialAuto] %s %s (%s %s): %s"),
				Severity, Code, *Issue.Mission, *Issue.Phase, *Message);
		}

		void WriteIssues() const
		{
			TArray<TSharedPtr<FJsonValue>> Values;
			for (const FIssue& Issue : Issues)
			{
				const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetStringField(TEXT("severity"), Issue.Severity);
				Item->SetStringField(TEXT("code"), Issue.Code);
				Item->SetStringField(TEXT("mission"), Issue.Mission);
				Item->SetStringField(TEXT("phase"), Issue.Phase);
				Item->SetStringField(TEXT("message"), Issue.Message);
				Item->SetNumberField(TEXT("timeline_seq"), Issue.TimelineSeq);
				Values.Add(MakeShared<FJsonValueObject>(Item));
			}
			FString Out;
			const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
				TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
			FJsonSerializer::Serialize(Values, Writer);
			FFileHelper::SaveStringToFile(Out, *FPaths::Combine(OutDir, TEXT("issues.json")),
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}

		void AppendTimeline(const TSharedRef<FJsonObject>& Line) const
		{
			// 한 줄씩 열고 닫는다 ― 프로세스가 죽어도 마지막 줄까지 남는다
			FFileHelper::SaveStringToFile(JsonToString(Line, false) + TEXT("\n"), *FPaths::Combine(OutDir, TEXT("timeline.jsonl")),
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append | FILEWRITE_AllowRead);
		}

		TSharedRef<FJsonObject> ObservationJson(const FObservation& Obs) const
		{
			const TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
			Out->SetStringField(TEXT("map"), Obs.MapName);
			Out->SetField(TEXT("game_t"), JsonNum(Obs.GameTime));
			Out->SetStringField(TEXT("mission"), Obs.MissionID.ToString());
			Out->SetStringField(TEXT("title"), Obs.MissionTitle);
			Out->SetNumberField(TEXT("seq_n"), Obs.SeqCurrent);
			Out->SetNumberField(TEXT("seq_t"), Obs.SeqTotal);
			Out->SetNumberField(TEXT("phase"), Obs.PhaseIndex);
			Out->SetStringField(TEXT("phase_token"), Obs.PhaseToken);
			Out->SetNumberField(TEXT("phase_broadcasts"), PhaseBroadcasts);
			Out->SetBoolField(TEXT("hard_gate"), Obs.bHardGate);
			Out->SetBoolField(TEXT("gated"), Obs.bGated);
			if (Obs.Target.IsValid())
			{
				const TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
				Target->SetStringField(TEXT("name"), Obs.TargetName);
				Target->SetStringField(TEXT("class"), Obs.TargetClass);
				Target->SetStringField(TEXT("outer"), Obs.TargetOuter);
				if (Obs.bTargetRect)
				{
					Target->SetField(TEXT("rect"), RectJson(Obs.TargetRect, Obs.ViewportRect));
				}
				Target->SetBoolField(TEXT("actionable"), Obs.bTargetActionable);
				Target->SetStringField(TEXT("reason"), Obs.TargetReason);
				Target->SetBoolField(TEXT("covered_by_prompt"), Obs.bTargetCovered);
				Out->SetObjectField(TEXT("target"), Target);
			}
			if (Obs.Spotlight.IsValid())
			{
				const TSharedRef<FJsonObject> Spot = MakeShared<FJsonObject>();
				Spot->SetStringField(TEXT("name"), Obs.SpotlightName);
				if (Obs.bSpotlightScreen)
				{
					Spot->SetField(TEXT("screen"), PointJson(Obs.SpotlightAbs, Obs.ViewportRect));
				}
				Out->SetObjectField(TEXT("spotlight"), Spot);
			}
			Out->SetStringField(TEXT("gesture"), GestureName(Obs.Gesture));
			if (Obs.GestureAnchor.IsValid())
			{
				const TSharedRef<FJsonObject> Anchor = MakeShared<FJsonObject>();
				Anchor->SetStringField(TEXT("name"), Obs.GestureAnchorName);
				if (Obs.bAnchorScreen)
				{
					Anchor->SetField(TEXT("screen"), PointJson(Obs.AnchorAbs, Obs.ViewportRect));
				}
				Out->SetObjectField(TEXT("gesture_anchor"), Anchor);
			}
			Out->SetStringField(TEXT("guide_line"), Obs.GuideLine);
			Out->SetStringField(TEXT("mentor_line"), Obs.MentorLine);
			Out->SetBoolField(TEXT("explain"), Obs.bExplain);
			Out->SetNumberField(TEXT("explain_step"), Obs.ExplainStep);
			Out->SetBoolField(TEXT("ready_to_claim"), Obs.bReadyToClaim);
			Out->SetBoolField(TEXT("claim_presentable"), Obs.bClaimPresentable);
			if (Obs.bClaimRect)
			{
				Out->SetField(TEXT("claim_rect"), RectJson(Obs.ClaimRect, Obs.ViewportRect));
			}
			if (Obs.ProgressTarget >= 0)
			{
				Out->SetStringField(TEXT("progress"), FString::Printf(TEXT("%lld/%lld"), Obs.ProgressCurrent, Obs.ProgressTarget));
			}
			Out->SetStringField(TEXT("prompt_top"), Obs.PromptTop);
			Out->SetStringField(TEXT("bottom_top"), Obs.BottomTop);
			Out->SetBoolField(TEXT("intro_playing"), Obs.bIntroPlaying);
			Out->SetBoolField(TEXT("city_opening"), Obs.bCityOpening);
			Out->SetStringField(TEXT("derived_raw"), Obs.DerivedRaw);
			Out->SetNumberField(TEXT("prompt_count"), Obs.PromptCount);
			Out->SetBoolField(TEXT("placement"), Obs.bPlacement);
			Out->SetBoolField(TEXT("panel_intro"), Obs.bPanelIntro);
			Out->SetBoolField(TEXT("recruit_presentation"), Obs.bRecruitPresentation);
			Out->SetNumberField(TEXT("operations"), Obs.OperationCount);
			Out->SetBoolField(TEXT("overlay"), Obs.bOverlay);
			Out->SetBoolField(TEXT("overlay_settled"), Obs.bOverlaySettled);
			if (Obs.bTooltip)
			{
				Out->SetField(TEXT("tooltip_rect"), RectJson(Obs.TooltipRect, Obs.ViewportRect));
			}
			if (Obs.bViewport)
			{
				Out->SetField(TEXT("viewport_abs"), RectJson(Obs.ViewportRect, FSlateRect(0.f, 0.f, 0.f, 0.f)));
			}
			Out->SetStringField(TEXT("wait_reason"), Obs.WaitReason);
			Out->SetObjectField(TEXT("res"), ResJson(Obs.Res));
			Out->SetNumberField(TEXT("hq_level"), Obs.HQLevel);
			if (Obs.bFundWait)
			{
				Out->SetStringField(TEXT("fund_wait"), EconomyKeyName(Obs.FundResource));
			}
			Out->SetBoolField(TEXT("tutorial_completed"), Obs.bTutorialCompleted);
			Out->SetStringField(TEXT("key"), Obs.Key);
			return Out;
		}

		void WriteStateLine(const FObservation& Obs, double Now, const FString& ShotName) const
		{
			const TSharedRef<FJsonObject> Line = ObservationJson(Obs);
			Line->SetStringField(TEXT("kind"), TEXT("state"));
			Line->SetNumberField(TEXT("seq"), Seq);
			Line->SetStringField(TEXT("wall"), FDateTime::Now().ToIso8601());
			Line->SetField(TEXT("t"), JsonNum(Now - StartedAt));
			Line->SetStringField(TEXT("shot"), FString(TEXT("shots/")) + ShotName);
			AppendTimeline(Line);
		}
	};

	// 맵 이동·엔진 종료 순서와 무관하게 살아 있어야 해서 일부러 해제하지 않는다
	FPlayer& GetPlayer()
	{
		static FPlayer* Player = new FPlayer();
		return *Player;
	}

	void HandleStartCommand(const TArray<FString>& Args)
	{
		if (Args.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[TutorialAuto] 사용법: CGR.TutorialAuto.Start <OutDir> [TimeScale]"));
			return;
		}
		// 콘솔 인자는 공백으로 쪼개진다 ― 마지막 숫자 토큰만 배속으로 떼고 나머지는 경로로 다시 붙인다
		TArray<FString> PathTokens = Args;
		float Scale = 1.f;
		if (PathTokens.Num() >= 2 && PathTokens.Last().IsNumeric())
		{
			Scale = FCString::Atof(*PathTokens.Last());
			PathTokens.RemoveAt(PathTokens.Num() - 1);
		}
		FString Dir = FString::Join(PathTokens, TEXT(" "));
		Dir.TrimStartAndEndInline();
		Dir.TrimQuotesInline();
		if (Dir.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[TutorialAuto] OutDir 가 비었습니다."));
			return;
		}
		GetPlayer().Start(Dir, Scale);
	}
}

static FAutoConsoleCommand GTutorialAutoStartCommand(
	TEXT("CGR.TutorialAuto.Start"),
	TEXT("튜토리얼 자동 플레이 시작. CGR.TutorialAuto.Start <OutDir> [TimeScale=1]"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&CGRTutorialAuto::HandleStartCommand));

static FAutoConsoleCommand GTutorialAutoStopCommand(
	TEXT("CGR.TutorialAuto.Stop"),
	TEXT("튜토리얼 자동 플레이 중지(summary.json 을 fail 로 남긴다)."),
	FConsoleCommandDelegate::CreateLambda([]() { CGRTutorialAuto::GetPlayer().Stop(TEXT("stopped")); }));

static FAutoConsoleCommand GTutorialAutoStatusCommand(
	TEXT("CGR.TutorialAuto.Status"),
	TEXT("튜토리얼 자동 플레이 상태를 로그로 출력."),
	FConsoleCommandDelegate::CreateLambda([]() { CGRTutorialAuto::GetPlayer().PrintStatus(); }));

#endif // !UE_BUILD_SHIPPING
