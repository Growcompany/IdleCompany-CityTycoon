#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "CommonTextBlock.h"
#include "Containers/Ticker.h"
#include "Core/CGGameInstance.h"
#include "Data/EmployeeTypes.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Manager/DevPresetSeeder.h"
#include "Manager/EmployeeManager.h"
#include "Manager/EntityManager.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Enum/DevelopStartResult.h"
#include "Enum/ProjectDirection.h"
#include "Enum/ProjectLifecycle.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UI/Element/Office/OfficeRosterRowWidget.h"
#include "UI/Element/Office/OfficeRosterSummaryWidget.h"
#include "UnrealClient.h"

// 익명 ns 금지 ― 유니티 TU 병합 시 재정의 충돌
namespace OfficeRosterQA
{
	static UWorld* FindGameWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if ((Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game) && Context.World())
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	static TSharedRef<FJsonObject> Rect(const FGeometry& Geo, const FGeometry& ViewportGeo)
	{
		const FVector2D TopLeft = ViewportGeo.AbsoluteToLocal(Geo.GetAbsolutePosition());
		const FVector2D BottomRight = ViewportGeo.AbsoluteToLocal(Geo.LocalToAbsolute(Geo.GetLocalSize()));
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetNumberField(TEXT("left"), TopLeft.X);
		Out->SetNumberField(TEXT("top"), TopLeft.Y);
		Out->SetNumberField(TEXT("right"), BottomRight.X);
		Out->SetNumberField(TEXT("bottom"), BottomRight.Y);
		return Out;
	}

	// 사무실 메인 화면 안의 이름 붙은 위젯 하나 ― 연구소 버튼·개발 진행줄 위치 대조용
	static UWidget* FindNamedWidget(UWorld* World, const FName Name)
	{
		TArray<UUserWidget*> AllWidgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, AllWidgets, UUserWidget::StaticClass(), false);
		for (UUserWidget* Candidate : AllWidgets)
		{
			if (UWidget* Found = Candidate ? Candidate->GetWidgetFromName(Name) : nullptr)
			{
				return Found;
			}
		}
		return nullptr;
	}

	// MainMap 에서: 직원이 배정된 첫 소유 건물의 사무실로 간다(CGR.EquipmentQA.OpenOffice 와 같은 저장→전환 순서)
	static void OpenStaffedOffice(UWorld* World, UCGGameInstance* GI, UEmployeeManager* EmpMgr)
	{
		UEntityManager* Entities = World->GetSubsystem<UEntityManager>();
		USaveLoadManager* Save = GI->GetSubsystem<USaveLoadManager>();
		for (const FEmployeeInstance& Emp : EmpMgr->GetAllEmployeesRef())
		{
			ABuildingBaseActor* Building = (Entities && Emp.AssignedBuildingIndex > 0) ? Entities->GetBuildingByIndex(Emp.AssignedBuildingIndex) : nullptr;
			if (!IsValid(Building) || Building->GetCompanyType() == ECompanyType::None) { continue; }
			if (!Save || !Save->SaveGameData())
			{
				UE_LOG(LogTemp, Error, TEXT("[OfficeRosterQA] 저장 실패 ― 전환 취소"));
				return;
			}
			GI->SetCurrentManagedBuilding(Building);
			GI->SetCurrentBuildingCompanyType(Building->GetCompanyType());
			GI->SetOfficeMode(EOfficeMode::Normal);
			UE_LOG(LogTemp, Display, TEXT("[OfficeRosterQA] opening office %d"), Emp.AssignedBuildingIndex);
			GI->TransitionToLevel(TEXT("OfficeMap"));
			return;
		}
		UE_LOG(LogTemp, Error, TEXT("[OfficeRosterQA] 직원이 배정된 소유 건물이 MainMap 에 없다 ― Preset_Apply 먼저"));
	}

	// 이 건물 로스터를 덮어쓴다 ― 지금 세이브가 바뀐다(자동 모드는 격리 Saved 에서만 돌린다)
	static bool SeedRoster(UCGGameInstance* GI, UEmployeeManager* EmpMgr, int32 Count)
	{
		// INDEX_NONE 로 시드하면 소속 없는 대기 직원 전원이 지워진다 ― 건물 문맥이 있을 때만
		const int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();
		if (BuildingIndex == INDEX_NONE)
		{
			UE_LOG(LogTemp, Error, TEXT("[OfficeRosterQA] 현재 건물 번호가 없어 시드를 건너뛴다"));
			return false;
		}
		EmpMgr->SeedEmployeesForBuilding(BuildingIndex, Count, 5, 30, 0, 5, 1012);
		return true;
	}

	// OfficeMap 직행(격리 Saved)에는 건물 문맥이 없다 ― 자동 모드 전용으로 임시 번호·산업을 정한다
	static void EnsureBuildingContext(UCGGameInstance* GI)
	{
		if (GI->GetCurrentManagedBuildingIndex() != INDEX_NONE) { return; }
		GI->SetCurrentManagedBuildingIndex(1);
		GI->SetCurrentBuildingCompanyType(ECompanyType::Game);
		UE_LOG(LogTemp, Display, TEXT("[OfficeRosterQA] no building context ― using index 1 / Game for the auto run"));
	}

	// 정원·프리셋과 무관하게 판만 검증하는 QA 로스터. 그래픽(슬롯 2)은 비워 0명 줄을 만든다
	static void FabricateRoster(UCGGameInstance* GI, UEmployeeManager* EmpMgr, int32 Count)
	{
		const int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();
		if (BuildingIndex == INDEX_NONE) { return; }
		TArray<FEmployeeInstance> Roster;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FEmployeeInstance Emp;
			Emp.EmployeeID = 900000 + Index;
			Emp.EmployeeName = FString::Printf(TEXT("QA %d"), Index + 1);
			Emp.AssignedBuildingIndex = BuildingIndex;
			Emp.DisciplinePoints.Init(0, 6);
			const int32 Primary = (Index % 5 >= 2) ? (Index % 5) + 1 : (Index % 5);
			Emp.DisciplinePoints[Primary] = 900 + Index * 37;
			Emp.DisciplinePoints[(Primary + 1) % 6] = 120 + Index * 11;
			Roster.Add(Emp);
		}
		EmpMgr->SetEmployeesForBuilding(BuildingIndex, Roster);
		// SetEmployeesForBuilding 은 아무것도 브로드캐스트하지 않는다
		EmpMgr->OnEmployeeRosterChanged.Broadcast();
	}

	// 자기 가시성만 보면 부모(레일)가 접혀도 '보임'으로 읽힌다 ― Slate 조상까지 모두 본다
	static bool IsEffectivelyVisible(const UWidget* Widget)
	{
		TSharedPtr<SWidget> Slate = Widget ? Widget->GetCachedWidget() : nullptr;
		if (!Slate.IsValid()) { return false; }
		for (; Slate.IsValid(); Slate = Slate->GetParentWidget())
		{
			if (!Slate->GetVisibility().IsVisible()) { return false; }
		}
		return true;
	}

	static void WriteReport(UWorld* World, UCGGameInstance* GI, UEmployeeManager* EmpMgr, const FString& Label)
	{
		const int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();
		TArray<UUserWidget*> Found;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Found, UOfficeRosterSummaryWidget::StaticClass(), false);
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("label"), Label);
		Root->SetStringField(TEXT("map"), World->GetMapName());
		Root->SetNumberField(TEXT("instances"), Found.Num());
		Root->SetNumberField(TEXT("members"), EmpMgr->GetEmployeeCountInBuilding(BuildingIndex));
		FVector2D ViewportSize = FVector2D::ZeroVector;
		if (World->GetGameViewport()) { World->GetGameViewport()->GetViewportSize(ViewportSize); }
		Root->SetNumberField(TEXT("viewport_w"), ViewportSize.X);
		Root->SetNumberField(TEXT("viewport_h"), ViewportSize.Y);
		const FGeometry ViewportGeo = UWidgetLayoutLibrary::GetViewportWidgetGeometry(World);
		const FVector2D VirtualSize = ViewportGeo.GetLocalSize();
		Root->SetNumberField(TEXT("virtual_w"), VirtualSize.X);
		Root->SetNumberField(TEXT("virtual_h"), VirtualSize.Y);

		if (Found.Num() == 1)
		{
			const UOfficeRosterSummaryWidget* Panel = Cast<UOfficeRosterSummaryWidget>(Found[0]);
			Root->SetStringField(TEXT("visibility"), UEnum::GetValueAsString(Panel->GetVisibility()));
			Root->SetBoolField(TEXT("effectively_visible"), IsEffectivelyVisible(Panel));
			Root->SetObjectField(TEXT("panel"), Rect(Panel->GetCachedGeometry(), ViewportGeo));
			TArray<TSharedPtr<FJsonValue>> Rows;
			for (int32 Index = 0; Index < 6; ++Index)
			{
				const UOfficeRosterRowWidget* Row = Panel->GetRow(Index);
				if (!Row) { continue; }
				TSharedRef<FJsonObject> Line = MakeShared<FJsonObject>();
				Line->SetStringField(TEXT("count"), Row->GetCountTextForTest()->GetText().ToString());
				Line->SetStringField(TEXT("power"), Row->GetPowerTextForTest()->GetText().ToString());
				Rows.Add(MakeShared<FJsonValueObject>(Line));
			}
			Root->SetArrayField(TEXT("rows"), Rows);
		}
		if (const UWidget* Rail = FindNamedWidget(World, TEXT("TrainingCenterOpenButton")))
		{
			Root->SetObjectField(TEXT("rail_first_button"), Rect(Rail->GetCachedGeometry(), ViewportGeo));
			Root->SetBoolField(TEXT("rail_effectively_visible"), IsEffectivelyVisible(Rail));
		}
		if (const UWidget* Strip = FindNamedWidget(World, TEXT("StripRoot")))
		{
			Root->SetStringField(TEXT("strip_visibility"), UEnum::GetValueAsString(Strip->GetVisibility()));
			Root->SetBoolField(TEXT("strip_effectively_visible"), IsEffectivelyVisible(Strip));
			Root->SetObjectField(TEXT("strip"), Rect(Strip->GetCachedGeometry(), ViewportGeo));
		}

		const FString Dir = FPaths::ProjectSavedDir() / TEXT("OfficeRosterSummary/office");
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString Stamp = Label.IsEmpty() ? FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")) : Label;
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Root, Writer);
		FFileHelper::SaveStringToFile(Json, *(Dir / FString::Printf(TEXT("report-%s.json"), *Stamp)));
		FScreenshotRequest::RequestScreenshot(Dir / FString::Printf(TEXT("office-%s.png"), *Stamp), true, false);
		UE_LOG(LogTemp, Display, TEXT("[OfficeRosterQA] saved %s/report-%s.json"), *Dir, *Stamp);
	}

	static void Report(const TArray<FString>& Args)
	{
		UWorld* World = FindGameWorld();
		UCGGameInstance* GI = World ? Cast<UCGGameInstance>(World->GetGameInstance()) : nullptr;
		UEmployeeManager* EmpMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
		if (!EmpMgr)
		{
			UE_LOG(LogTemp, Error, TEXT("[OfficeRosterQA] PIE/게임 월드 또는 EmployeeManager 가 없다 ― 사무실에 들어간 뒤 실행"));
			return;
		}
		if (Args.Num() >= 1 && Args[0] == TEXT("open"))
		{
			OpenStaffedOffice(World, GI, EmpMgr);
			return;
		}
		if (Args.Num() >= 2 && Args[0] == TEXT("seed") && !SeedRoster(GI, EmpMgr, FCString::Atoi(*Args[1])))
		{
			return;
		}
		WriteReport(World, GI, EmpMgr, FString());
	}

	static FAutoConsoleCommand ReportCommand(TEXT("CGR.OfficeRosterQA.Report"),
		TEXT("사무실 직무 구성 목록의 위치·값을 JSON 과 스크린샷으로 남긴다. 'open' = MainMap 에서 직원 있는 사무실로 이동. 'seed N' = 이 건물 로스터를 N명으로 덮어쓴 뒤 남긴다(현재 세이브를 덮는다)."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Report));

	// ===== 자동 모드: 격리 Saved 의 -game 에서 사람 손 없이 상태를 돌며 남긴다 =====
	struct FAutoRun
	{
		FString Tag;
		int32 Step = 0;
		double NextTime = 0.0;
		FTSTicker::FDelegateHandle Handle;
	};
	static FAutoRun GAutoRun;

	static bool IsOnMap(const UWorld* World, const TCHAR* Name)
	{
		return World && World->GetMapName().Contains(Name);
	}

	static bool AutoTick(float)
	{
		const double Now = FPlatformTime::Seconds();
		if (Now < GAutoRun.NextTime) { return true; }
		UWorld* World = FindGameWorld();
		UCGGameInstance* GI = World ? Cast<UCGGameInstance>(World->GetGameInstance()) : nullptr;
		UEmployeeManager* EmpMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
		if (!EmpMgr)
		{
			GAutoRun.NextTime = Now + 1.0;
			return true;
		}
		const auto Advance = [Now](double WaitSeconds) { GAutoRun.NextTime = Now + WaitSeconds; ++GAutoRun.Step; };
		const FString& Tag = GAutoRun.Tag;
		const auto SetRes = [World](const TCHAR* Res)
		{
			// 가상 세로는 항상 1440 ― 창 비율만 바꾸면 2304(16:10)·3120(19.5:9) 가상 폭이 된다
			GEngine->Exec(World, *FString::Printf(TEXT("r.SetRes %s"), Res));
		};
		switch (GAutoRun.Step)
		{
		case 0:
			// OfficeMap 으로 바로 띄웠으면 프리셋·입장 단계를 건너뛴다(프리셋이 도시 맵에서 건물을 못 놓는 경우의 우회로)
			if (IsOnMap(World, TEXT("OfficeMap"))) { GAutoRun.Step = 3; GAutoRun.NextTime = Now; return true; }
			if (!IsOnMap(World, TEXT("MainMap"))) { GAutoRun.NextTime = Now + 1.0; return true; }
			Advance(5.0);
			return true;
		case 1:
			if (UDevPresetSeeder* Seeder = GI->GetSubsystem<UDevPresetSeeder>()) { Seeder->ReapplyPreset(TEXT("Mid")); }
			Advance(5.0);
			return true;
		case 2:
			OpenStaffedOffice(World, GI, EmpMgr);
			Advance(3.0);
			return true;
		case 3:
			if (!IsOnMap(World, TEXT("OfficeMap"))) { GAutoRun.NextTime = Now + 1.0; return true; }
			EnsureBuildingContext(GI);
			SetRes(TEXT("1280x800w"));
			Advance(8.0);
			return true;
		// 대기 중 ― 16:10
		case 4: WriteReport(World, GI, EmpMgr, Tag + TEXT("-1610-seed0")); Advance(2.0); return true;
		case 5: FabricateRoster(GI, EmpMgr, 4); Advance(3.0); return true;
		case 6: WriteReport(World, GI, EmpMgr, Tag + TEXT("-1610-seed4")); Advance(2.0); return true;
		case 7: FabricateRoster(GI, EmpMgr, 23); Advance(3.0); return true;
		case 8: WriteReport(World, GI, EmpMgr, Tag + TEXT("-1610-seed23")); Advance(2.0); return true;
		// 대기 중 ― 19.5:9
		case 9: SetRes(TEXT("1560x720w")); Advance(4.0); return true;
		case 10: WriteReport(World, GI, EmpMgr, Tag + TEXT("-195-seed23")); Advance(2.0); return true;
		case 11: FabricateRoster(GI, EmpMgr, 4); Advance(3.0); return true;
		case 12: WriteReport(World, GI, EmpMgr, Tag + TEXT("-195-seed4")); Advance(2.0); return true;
		case 13: FabricateRoster(GI, EmpMgr, 0); Advance(3.0); return true;
		case 14: WriteReport(World, GI, EmpMgr, Tag + TEXT("-195-seed0")); Advance(2.0); return true;
		// 개발 중 ― 기획 보드 착수와 같은 진입점
		case 15:
		{
			FabricateRoster(GI, EmpMgr, 9);
			UOfficeStageProgressManager* StageMgr = World->GetSubsystem<UOfficeStageProgressManager>();
			const EDevelopStartResult Result = StageMgr
				? StageMgr->StartSelfDevelopFromProject(1, EProjectDirection::Standard, FText::GetEmpty())
				: EDevelopStartResult::NoProjectRow;
			UE_LOG(LogTemp, Display, TEXT("[OfficeRosterQA] dev start result=%d lifecycle=%s"), static_cast<int32>(Result),
				StageMgr ? *LexToString(StageMgr->GetLifecycle()) : TEXT("none"));
			Advance(6.0);
			return true;
		}
		case 16: WriteReport(World, GI, EmpMgr, Tag + TEXT("-195-dev")); Advance(2.0); return true;
		case 17: SetRes(TEXT("1280x800w")); Advance(4.0); return true;
		case 18: WriteReport(World, GI, EmpMgr, Tag + TEXT("-1610-dev")); Advance(3.0); return true;
		default:
			UE_LOG(LogTemp, Display, TEXT("[OfficeRosterQA] auto run %s done"), *Tag);
			FPlatformMisc::RequestExit(false);
			return false;
		}
	}

	static void StartAuto(const TArray<FString>& Args)
	{
		// 끝에 프로세스를 종료하고 세이브를 덮는다 ― 공유 에디터 PIE 에서 돌면 사람의 에디터가 꺼진다
		if (GIsEditor)
		{
			UE_LOG(LogTemp, Error, TEXT("[OfficeRosterQA] Auto 는 격리 Saved 의 -game 전용이다 ― 에디터에서는 실행하지 않는다"));
			return;
		}
		if (GAutoRun.Handle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(GAutoRun.Handle);
		}
		GAutoRun = FAutoRun();
		GAutoRun.Tag = Args.Num() >= 1 ? Args[0] : TEXT("auto");
		GAutoRun.Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&AutoTick), 0.25f);
		UE_LOG(LogTemp, Display, TEXT("[OfficeRosterQA] auto run %s armed"), *GAutoRun.Tag);
	}

	static FAutoConsoleCommand AutoCommand(TEXT("CGR.OfficeRosterQA.Auto"),
		TEXT("격리 Saved 의 -game 전용: MainMap 대기 → Preset Mid → 사무실 입장 → 직원 preset/4/23/0명·개발 중을 차례로 보고하고 종료한다. 인자 = 파일 태그."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&StartAuto));
}
#endif
