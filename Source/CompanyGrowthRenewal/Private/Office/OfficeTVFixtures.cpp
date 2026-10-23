#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Core/CGGameInstance.h"
#include "Data/EmployeeTypes.h"
#include "Dom/JsonObject.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Manager/EmployeeManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ShaderCompiler.h"
#include "Slate/WidgetRenderer.h"
#include "Table/ProjectDataTable.h"
#include "TextureResource.h"
#include "UI/Element/Office/OfficeTVScreenWidget.h"

// Renders every programme with stress data so layouts can be checked without playing to each state.
namespace OfficeTVFixtures
{
	struct FFixture
	{
		FString Name;
		FOfficeTVBroadcastFrame Frame;
		float MotionSeconds = 6.f;
	};

	FText T(const TCHAR* Text) { return FText::FromString(Text); }

	FOfficeTVBroadcastEntry Entry(const TCHAR* Title, const TCHAR* Value = TEXT(""), const TCHAR* Caption = TEXT(""))
	{
		FOfficeTVBroadcastEntry Result;
		Result.Title = T(Title);
		Result.Value = T(Value);
		Result.Caption = T(Caption);
		return Result;
	}

	void BuildFixtures(UTableManagerSubsystem* Tables, const TArray<int32>& Staff, TArray<FFixture>& Out)
	{
		// In the editor there is no game instance; the first ten game projects are named here instead.
		static const TCHAR* FallbackCodes[] = {TEXT("101"), TEXT("102"), TEXT("103"), TEXT("001"), TEXT("104"),
			TEXT("002"), TEXT("105"), TEXT("003"), TEXT("009"), TEXT("004")};
		static const TCHAR* FallbackNames[] = {TEXT("탭탭코인"), TEXT("컬러코드"), TEXT("두더지팡"), TEXT("점프왕"),
			TEXT("브릭브레이크"), TEXT("젬크러쉬"), TEXT("스네이크런"), TEXT("플립러너"), TEXT("파티팡팡"), TEXT("스페이스건")};
		TArray<TSoftObjectPtr<UTexture2D>> Covers;
		TArray<FText> Names;
		for (int32 Index = 1; Index <= 10; ++Index)
		{
			bool bFound = false;
			const FProjectData Project = Tables ? Tables->GetProjectData(ECompanyType::Game, Index, bFound) : FProjectData();
			const FString Code = FallbackCodes[Index - 1];
			Covers.Add(bFound ? Project.Icon : TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString::Printf(
				TEXT("/Game/CompanyGrowth/UI/Textures/ProjectCover/T_ProjectCover_%s.T_ProjectCover_%s"), *Code, *Code))));
			Names.Add(bFound ? Project.ProjectName : T(FallbackNames[Index - 1]));
		}
		const FText LongTitle = T(TEXT("울트라 메가 탭탭코인 디럭스 에디션"));
		auto Person = [&Staff](int32 Index) { return Staff.IsEmpty() ? INDEX_NONE : Staff[Index % Staff.Num()]; };
		const FLinearColor Common(0.4f, 0.47f, 0.56f, 1.f);
		const FLinearColor Legendary(0.89f, 0.54f, 0.07f, 1.f);
		auto Add = [&Out](const TCHAR* Name, const FOfficeTVBroadcastFrame& Frame, float Motion = 6.f)
		{
			Out.Add({Name, Frame, Motion});
		};

		for (const float Progress : {0.f, 0.5f, 1.f})
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::Development;
			Frame.Artwork = Covers[0];
			Frame.Progress = Progress;
			Frame.Value = FText::AsPercent(Progress);
			Frame.Headline = Names[0];
			Add(*FString::Printf(TEXT("A1_dev_%03d"), FMath::RoundToInt(Progress * 100)), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::Development;
			Frame.Artwork = Covers[0];
			Frame.Progress = 0.5f;
			Frame.Value = FText::AsPercent(0.5f);
			Frame.Headline = LongTitle;
			Add(TEXT("A1_dev_long_title"), Frame);
		}
		for (const int32 Disciplines : {4, 6})
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::DevRace;
			const float Ratios[] = {1.05f, 1.2f, 0.7f, 0.45f, 1.f, 0.15f};
			const TCHAR* Labels[] = {TEXT("기획"), TEXT("개발"), TEXT("그래픽"), TEXT("사운드"), TEXT("서버"), TEXT("QA")};
			for (int32 Discipline = 0; Discipline < Disciplines; ++Discipline)
			{
				FOfficeTVBroadcastEntry& Bar = Frame.Entries.Add_GetRef(Entry(Labels[Discipline]));
				Bar.Slot = Discipline;
				Bar.Ratio = Ratios[Discipline];
				Bar.bHighlighted = Ratios[Discipline] >= 1.f;
			}
			Frame.Headline = FText::Format(INVTEXT("{0} 목표 도달"), Names[4]);
			Frame.HeadlineValue = T(Disciplines == 4 ? TEXT("2/4") : TEXT("3/6"));
			Add(*FString::Printf(TEXT("A3_race_%d"), Disciplines), Frame, 3.f);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::DevForecast;
			Frame.Grade = TEXT("A");
			Frame.GradeLabel = T(TEXT("수작"));
			Frame.Progress = 0.59f;
			for (const TCHAR* Zone : {TEXT("범작"), TEXT("양작"), TEXT("수작"), TEXT("명작")})
			{
				Frame.Entries.Add(Entry(Zone));
			}
			Frame.Headline = FText::Format(INVTEXT("{0} 예상 등급"), Names[0]);
			Add(TEXT("A4_forecast"), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::CriticalFlash;
			Frame.PortraitEmployeeId = Person(0);
			Frame.PortraitBadge = T(TEXT("Lv.12"));
			Frame.Accent = Common;
			Frame.IconSlot = 1;
			Frame.Value = T(TEXT("+128"));
			Frame.Headline = T(TEXT("김척척 대리"));
			Add(TEXT("A5_critical"), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::Fever;
			Frame.Artwork = Covers[0];
			Frame.Value = T(TEXT("x1.5"));
			Frame.Headline = T(TEXT("작업 점수가 1.5배로 올라갑니다"));
			Add(TEXT("A6_fever"), Frame, 1.f);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::IssueResult;
			Frame.Title = T(TEXT("클라이언트 수정 요청"));
			Frame.Subtitle = T(TEXT("무시하고 진행"));
			Frame.Value = T(TEXT("전 직원 속도 +20% (30초)"));
			Frame.Headline = FText::Format(INVTEXT("{0} 돌발 상황"), LongTitle);
			Add(TEXT("A7_issue_long"), Frame);
		}
		for (const int32 Left : {7, 2})
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::LaunchCountdown;
			Frame.Artwork = Covers[0];
			Frame.Count = Left;
			Frame.Total = 10;
			Frame.Value = FText::FromString(FString::Printf(TEXT("00:%02d"), Left));
			Frame.Headline = Names[0];
			Add(*FString::Printf(TEXT("A8_countdown_%d"), Left), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::EmployeeHighlight;
			Frame.PortraitEmployeeId = Person(0);
			Frame.PortraitBadge = T(TEXT("Lv.4"));
			Frame.Accent = Common;
			Frame.Count = 128;
			Frame.Value = T(TEXT("128"));
			Frame.Subtitle = T(TEXT("개발에 집중 중"));
			Frame.Headline = T(TEXT("남궁나천재 인턴"));
			Add(TEXT("B1_employee_long"), Frame);
		}
		for (const int32 Size : {2, 5})
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::Crew;
			for (int32 Index = 0; Index < Size; ++Index)
			{
				FOfficeTVBroadcastEntry& Member = Frame.Entries.Add_GetRef(Entry(TEXT("직원"), Index == 0 ? TEXT("128") : TEXT("8")));
				Member.PortraitEmployeeId = Person(Index);
				Member.Accent = Index == 1 ? Legendary : Common;
				Member.Caption = T(TEXT("Lv.4"));
			}
			Frame.Headline = FText::Format(INVTEXT("{0} 제작진"), LongTitle);
			Frame.HeadlineValue = FText::Format(INVTEXT("{0}명"), FText::AsNumber(Size + 7));
			Add(*FString::Printf(TEXT("B2_crew_%d"), Size), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::WorkFeed;
			const TCHAR* Values[] = {TEXT("+128"), TEXT("+6"), TEXT("+3")};
			for (int32 Index = 0; Index < 3; ++Index)
			{
				FOfficeTVBroadcastEntry& Row = Frame.Entries.Add_GetRef(Entry(Index == 0 ? TEXT("남궁나천재 인턴") : TEXT("김척척 사원"), Values[Index]));
				Row.PortraitEmployeeId = Person(Index);
				Row.Slot = Index;
				Row.bHighlighted = Index == 0;
			}
			Frame.Headline = T(TEXT("방금 들어온 작업"));
			Add(TEXT("B3_feed"), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::Mentoring;
			for (int32 Index = 0; Index < 2; ++Index)
			{
				FOfficeTVBroadcastEntry& Member = Frame.Entries.Add_GetRef(Entry(TEXT("직원"), TEXT(""), Index == 0 ? TEXT("Lv.13") : TEXT("Lv.4")));
				Member.PortraitEmployeeId = Person(Index);
				Member.Accent = Index == 0 ? Legendary : Common;
			}
			Frame.Headline = T(TEXT("남궁나천재 과장 → 김척척 인턴"));
			Add(TEXT("B4_mentoring"), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::Promotion;
			Frame.PortraitEmployeeId = Person(1);
			Frame.Accent = Common;
			Frame.Subtitle = T(TEXT("인턴"));
			Frame.Title = T(TEXT("사원"));
			Frame.Value = T(TEXT("Lv.5"));
			Frame.Headline = T(TEXT("전꼼꼼 사원"));
			Add(TEXT("B5_promotion"), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::NewHire;
			Frame.PortraitEmployeeId = Person(2);
			Frame.Accent = Legendary;
			Frame.Title = T(TEXT("남궁나천재"));
			Frame.Subtitle = T(TEXT("인턴 · 전설"));
			Frame.Headline = T(TEXT("새 식구가 들어왔습니다"));
			Add(TEXT("B6_newhire"), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::Anniversary;
			Frame.PortraitEmployeeId = Person(0);
			Frame.PortraitBadge = T(TEXT("Lv.13"));
			Frame.Accent = Common;
			Frame.Value = T(TEXT("365"));
			Frame.Headline = T(TEXT("남궁나천재 대리 근속 365일"));
			Add(TEXT("B7_anniversary"), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::CriticScores;
			const TCHAR* Critics[] = {TEXT("게임평론"), TEXT("유저평점"), TEXT("스트리머"), TEXT("어워드")};
			const TCHAR* Scores[] = {TEXT("10"), TEXT("8"), TEXT("8"), TEXT("7")};
			for (int32 Index = 0; Index < 4; ++Index)
			{
				Frame.Entries.Add_GetRef(Entry(Critics[Index], Scores[Index])).bHighlighted = Index == 0;
			}
			Frame.Headline = FText::Format(INVTEXT("{0} 평점"), LongTitle);
			Frame.HeadlineValue = T(TEXT("33/40"));
			Add(TEXT("C1_critics"), Frame);
		}
		for (const bool bLong : {false, true})
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::LaunchCelebration;
			Frame.Artwork = Covers[0];
			Frame.Grade = TEXT("S");
			Frame.GradeLabel = T(TEXT("명작"));
			Frame.Headline = FText::Format(INVTEXT("{0} 출시했어요!"), bLong ? LongTitle : Names[0]);
			Add(bLong ? TEXT("C2_launch_long") : TEXT("C2_launch"), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::Reactions;
			FOfficeTVBroadcastEntry& Newest = Frame.Entries.Add_GetRef(Entry(TEXT("UI/UX에서 정성이 느껴진다 정말 오래 기억될 결과물"), TEXT(""), TEXT("@눈높은사람")));
			Newest.bHighlighted = false;
			FOfficeTVBroadcastEntry& Critic = Frame.Entries.Add_GetRef(Entry(TEXT("흠잡을 곳이 드물다"), TEXT("9"), TEXT("게임평론")));
			Critic.bHighlighted = true;
			Frame.Headline = FText::Format(INVTEXT("{0} 반응"), Names[0]);
			Add(TEXT("C3_reactions"), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::FirstDiscovery;
			FOfficeTVBroadcastEntry& Genre = Frame.Entries.Add_GetRef(Entry(TEXT("방치RPG")));
			Genre.Accent = FLinearColor(0.32f, 0.15f, 0.79f, 1.f);
			Frame.Entries.Add(Entry(TEXT("판타지")));
			Frame.Headline = FText::Format(INVTEXT("{0} 첫 발견"), Names[0]);
			Add(TEXT("C4_discovery"), Frame);
		}
		for (const float Motion : {0.5f, 6.f})
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::Credits;
			Frame.Title = LongTitle;
			Frame.SecondaryValue = T(TEXT("2분 30초"));
			for (int32 Index = 0; Index < 6; ++Index)
			{
				FOfficeTVBroadcastEntry& Member = Frame.Entries.Add_GetRef(Entry(Index == 0 ? TEXT("남궁나천재 과장") : TEXT("김척척 사원"), Index == 0 ? TEXT("128") : TEXT("6")));
				Member.PortraitEmployeeId = Person(Index);
			}
			Add(*FString::Printf(TEXT("C5_credits_%d"), FMath::RoundToInt(Motion * 10)), Frame, Motion);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::RevenueTicker;
			Frame.Value = T(TEXT("+9990만"));
			Frame.SecondaryValue = T(TEXT("1.23억"));
			for (int32 Index = 0; Index < 24; ++Index)
			{
				FOfficeTVBroadcastEntry& Bar = Frame.Entries.AddDefaulted_GetRef();
				Bar.Ratio = FMath::Pow(0.5f, Index / 23.f * 1.4f);
				Bar.bHighlighted = Index == 23;
			}
			Frame.Headline = FText::Format(INVTEXT("{0} 운영 중"), LongTitle);
			Add(TEXT("D1_revenue_big"), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::Operations;
			Frame.Value = T(TEXT("9990만"));
			Frame.SecondaryValue = T(TEXT("12:59"));
			Frame.Progress = 0.6f;
			Frame.Headline = Names[0];
			Add(TEXT("D2_operations"), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::Advertisement;
			Frame.Artwork = Covers[0];
			Frame.Title = LongTitle;
			Frame.Subtitle = T(TEXT("숨겨진 색의 순서를 찾는 퍼즐 끝까지 도전"));
			Frame.Label = T(TEXT("아케이드"));
			Frame.Grade = TEXT("S");
			Frame.GradeLabel = T(TEXT("명작"));
			Add(TEXT("D3_ad_long"), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::HotTime;
			Frame.Value = T(TEXT("x1.5"));
			Frame.Headline = T(TEXT("13시까지 수익이 1.5배입니다"));
			Add(TEXT("D4_hottime"), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::TierCodex;
			for (int32 Index = 0; Index < 10; ++Index)
			{
				FOfficeTVBroadcastEntry& Cell = Frame.Entries.Add_GetRef(Entry(*Names[Index].ToString()));
				Cell.Artwork = Covers[Index];
				Cell.bConcealed = Index != 0 && Index != 3 && Index != 9;
				Cell.Grade = Index == 0 || Index == 3 ? FName(TEXT("S")) : Index == 9 ? FName(TEXT("A")) : NAME_None;
			}
			Frame.Count = 3;
			Frame.Total = 7;
			Frame.Headline = T(TEXT("T2 승급까지"));
			Frame.HeadlineValue = T(TEXT("4개"));
			Add(TEXT("E1_codex"), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::CompanyNews;
			Frame.Entries.Add(Entry(TEXT("업계를 이끄는 게임회사"), TEXT("프로젝트 산출량 · 바이럴"), TEXT("손꼽히는 게임회사")));
			Frame.Headline = T(TEXT("승급까지 출시"));
			Frame.HeadlineValue = T(TEXT("10개"));
			Add(TEXT("E2_nexttitle_long"), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::TierUp;
			Frame.Grade = TEXT("T10");
			Frame.Title = T(TEXT("업계를 이끄는 게임회사"));
			Frame.Value = T(TEXT("프로젝트 산출량"));
			Frame.Headline = T(TEXT("T10 승급을 축하합니다"));
			Add(TEXT("E3_tierup"), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::HallOfFame;
			const TCHAR* Grades[] = {TEXT("S"), TEXT("S"), TEXT("A"), TEXT("A"), TEXT("B")};
			for (int32 Index = 0; Index < 5; ++Index)
			{
				FOfficeTVBroadcastEntry& Shelf = Frame.Entries.Add_GetRef(Entry(*Names[Index].ToString()));
				Shelf.Artwork = Covers[Index];
				Shelf.Grade = Grades[Index];
			}
			Frame.Subtitle = T(TEXT("명작 2 · 수작 2 · 양작 1"));
			Frame.Headline = T(TEXT("명예의 전당"));
			Frame.HeadlineValue = T(TEXT("12편"));
			Add(TEXT("E4_hall"), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::TopRevenue;
			const TCHAR* Values[] = {TEXT("9990억"), TEXT("8450만"), TEXT("2310만")};
			for (int32 Index = 0; Index < 3; ++Index)
			{
				FOfficeTVBroadcastEntry& Row = Frame.Entries.Add_GetRef(Entry(Index == 0 ? *LongTitle.ToString() : *Names[Index].ToString(), Values[Index]));
				Row.Artwork = Covers[Index];
				Row.Slot = Index;
			}
			Frame.Headline = T(TEXT("역대 매출"));
			Frame.HeadlineValue = T(TEXT("TOP 3"));
			Add(TEXT("E5_top"), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::Upcoming;
			Frame.Artwork = Covers[5];
			Frame.Title = LongTitle;
			Frame.Label = T(TEXT("방치RPG"));
			Frame.Accent = FLinearColor(0.32f, 0.15f, 0.79f, 1.f);
			Frame.Headline = T(TEXT("보석을 맞춰 터뜨리는 매치 퍼즐 끝까지 즐기는 법"));
			Add(TEXT("E6_upcoming"), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::StaffGap;
			Frame.IconSlot = 2;
			Frame.Title = T(TEXT("UI/UX"));
			Frame.Value = T(TEXT("0"));
			Frame.Headline = T(TEXT("이번 작품에 UI/UX가 필요합니다"));
			Add(TEXT("F1_staffgap"), Frame);
		}
		{
			FOfficeTVBroadcastFrame Frame;
			Frame.Program = EOfficeTVProgram::ProductionLine;
			Frame.Title = T(TEXT("초음파 가습기 프로"));
			Frame.Count = 3200;
			Frame.Total = 5000;
			Frame.Value = T(TEXT("3,200"));
			Frame.SecondaryValue = T(TEXT("/5,000"));
			Frame.Grade = TEXT("B");
			Frame.GradeLabel = T(TEXT("고급품"));
			Frame.Headline = T(TEXT("초음파 가습기 프로"));
			Add(TEXT("F3_production"), Frame, 1.f);
		}
	}

	void CollectTextFindings(UUserWidget* Root, const FString& Owner, TArray<TSharedPtr<FJsonValue>>& Out)
	{
		if (!Root || !Root->WidgetTree)
		{
			return;
		}
		Root->WidgetTree->ForEachWidget([&Out, &Owner](UWidget* Widget)
		{
			if (!Widget || !Widget->IsVisible())
			{
				return;
			}
			if (UUserWidget* Nested = Cast<UUserWidget>(Widget))
			{
				CollectTextFindings(Nested, Owner + TEXT("/") + Nested->GetName(), Out);
				return;
			}
			if (const UImage* Picture = Cast<UImage>(Widget))
			{
				// Artwork roles that are visible yet empty or zero-sized draw nothing on the TV.
				const FString Name = Picture->GetName();
				const bool bArtwork = Name == TEXT("CoverImage") || Name == TEXT("MosaicImage") || Name == TEXT("RevealImage")
					|| Name == TEXT("PortraitImage");
				const FVector2D ImageSize = Picture->GetCachedGeometry().GetLocalSize();
				const bool bEmpty = Picture->GetBrush().GetResourceObject() == nullptr;
				if (bArtwork && (bEmpty || ImageSize.X < 2.f || ImageSize.Y < 2.f))
				{
					TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
					Item->SetStringField(TEXT("widget"), Owner + TEXT("/") + Name);
					Item->SetBoolField(TEXT("image_empty"), bEmpty);
					Item->SetNumberField(TEXT("slot_w"), ImageSize.X);
					Item->SetNumberField(TEXT("slot_h"), ImageSize.Y);
					Out.Add(MakeShared<FJsonValueObject>(Item));
				}
				return;
			}
			const UCommonTextBlock* Text = Cast<UCommonTextBlock>(Widget);
			if (!Text || Text->GetText().IsEmpty())
			{
				return;
			}
			const FGeometry Geometry = Text->GetCachedGeometry();
			const FVector2D Desired = Text->GetDesiredSize();
			const FVector2D Size = Geometry.GetLocalSize();
			const FVector2D Min = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
			const FVector2D Max = Geometry.LocalToAbsolute(Size);
			const bool bOverflow = Desired.X > Size.X + 1.f;
			const bool bClippedHeight = Desired.Y > Size.Y + 1.f;
			const bool bOutside = Min.X < -1.f || Min.Y < -1.f || Max.X > 1025.f || Max.Y > 577.f;
			if (!bOverflow && !bClippedHeight && !bOutside)
			{
				return;
			}
			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(TEXT("widget"), Owner + TEXT("/") + Widget->GetName());
			Item->SetStringField(TEXT("text"), Text->GetText().ToString());
			Item->SetNumberField(TEXT("desired_w"), Desired.X);
			Item->SetNumberField(TEXT("slot_w"), Size.X);
			Item->SetNumberField(TEXT("desired_h"), Desired.Y);
			Item->SetNumberField(TEXT("slot_h"), Size.Y);
			Item->SetBoolField(TEXT("overflow"), bOverflow);
			Item->SetBoolField(TEXT("clipped_h"), bClippedHeight);
			Item->SetBoolField(TEXT("outside"), bOutside);
			Out.Add(MakeShared<FJsonValueObject>(Item));
		});
	}

	void Run(const TArray<FString>& Args, UWorld* World)
	{
		UCGGameInstance* Game = World ? Cast<UCGGameInstance>(World->GetGameInstance()) : nullptr;
		UTableManagerSubsystem* Tables = Game ? Game->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		TSubclassOf<UUserWidget> ScreenClass = Tables ? Tables->GetWidgetClass(EWidgetType::OfficeTVScreen) : nullptr;
		if (!ScreenClass)
		{
			ScreenClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(
				TEXT("/Game/CompanyGrowth/UI/Elements/Office/UIE_OfficeTVScreen.UIE_OfficeTVScreen_C"))).LoadSynchronous();
		}
		if (!ScreenClass || !ScreenClass->IsChildOf(UOfficeTVScreenWidget::StaticClass()))
		{
			UE_LOG(LogTemp, Error, TEXT("[OfficeTVFixtures] Screen class unavailable"));
			return;
		}
		const FString OutDir = Args.Num() > 0 ? Args[0] : FPaths::ProjectSavedDir() / TEXT("OfficeTV/Fixtures");
		IFileManager::Get().MakeDirectory(*OutDir, true);

		TArray<int32> Staff;
		if (const UEmployeeManager* Employees = Game ? Game->GetSubsystem<UEmployeeManager>() : nullptr)
		{
			for (const FEmployeeInstance& Employee : Employees->GetAllEmployeesRef())
			{
				Staff.Add(Employee.EmployeeID);
			}
		}
		if (Staff.IsEmpty())
		{
			TArray<FString> Portraits;
			IFileManager::Get().FindFiles(Portraits, *(FPaths::ProjectSavedDir() / TEXT("Portraits/*.png")), true, false);
			for (const FString& File : Portraits)
			{
				const FString Id = FPaths::GetBaseFilename(File);
				if (Id.IsNumeric())
				{
					Staff.Add(FCString::Atoi(*Id));
				}
			}
			Staff.Sort();
		}
		TArray<FFixture> Fixtures;
		BuildFixtures(Tables, Staff, Fixtures);
		const FString Only = Args.Num() > 1 ? Args[1] : FString();

		UOfficeTVScreenWidget* Screen = CreateWidget<UOfficeTVScreenWidget>(World, ScreenClass);
		// Fixtures use game projects, so the screen wears the game industry's accent and medals.
		Screen->ApplyIndustry(ECompanyType::Game);
		UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>();
		Target->ClearColor = FLinearColor::Black;
		// The widget renderer already writes gamma-corrected values; an sRGB target would encode them twice.
		Target->InitCustomFormat(1024, 576, PF_B8G8R8A8, true);
		Target->UpdateResourceImmediate(true);
		FWidgetRenderer Renderer(true, false);
		const TSharedRef<SWidget> Slate = Screen->TakeWidget();

		// A material's first draw can precede its shaders; one silent pass lets every programme's materials load.
		uint32 Serial = 1000;
		for (const FFixture& Fixture : Fixtures)
		{
			Screen->SetBroadcast(Fixture.Frame, ++Serial);
			Renderer.DrawWidget(Target, Slate, FVector2D(1024.f, 576.f), 0.f, false);
		}
#if WITH_EDITOR
		if (GShaderCompilingManager)
		{
			GShaderCompilingManager->FinishAllCompilation();
		}
#endif
		FlushRenderingCommands();

		TArray<TSharedPtr<FJsonValue>> Report;
		for (const FFixture& Fixture : Fixtures)
		{
			if (!Only.IsEmpty() && !Fixture.Name.Contains(Only))
			{
				continue;
			}
			Screen->SetBroadcast(Fixture.Frame, ++Serial);
			Screen->SettleMotionForCapture(Fixture.MotionSeconds);
			for (int32 Pass = 0; Pass < 3; ++Pass)
			{
				Renderer.DrawWidget(Target, Slate, FVector2D(1024.f, 576.f), 0.f, false);
			}
			FlushRenderingCommands();
			TArray<FColor> Pixels;
			if (FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource())
			{
				Resource->ReadPixels(Pixels);
			}
			for (FColor& Pixel : Pixels)
			{
				Pixel.A = 255;
			}
			TArray64<uint8> Png;
			FImageUtils::PNGCompressImageArray(1024, 576, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
			FFileHelper::SaveArrayToFile(Png, *(OutDir / Fixture.Name + TEXT(".png")));

			TArray<TSharedPtr<FJsonValue>> Findings;
			CollectTextFindings(Screen, TEXT("Screen"), Findings);
			TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("fixture"), Fixture.Name);
			Entry->SetNumberField(TEXT("pixels"), Pixels.Num());
			Entry->SetArrayField(TEXT("findings"), Findings);
			Report.Add(MakeShared<FJsonValueObject>(Entry));
			UE_LOG(LogTemp, Display, TEXT("[OfficeTVFixtures] %s findings=%d"), *Fixture.Name, Findings.Num());
		}
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Report, Writer);
		FFileHelper::SaveStringToFile(Json, *(OutDir / TEXT("fixtures.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		UE_LOG(LogTemp, Display, TEXT("[OfficeTVFixtures] Done: %d fixtures -> %s"), Report.Num(), *OutDir);
	}

	static FAutoConsoleCommandWithWorldAndArgs RenderCommand(
		TEXT("CGR.OfficeTV.RenderFixtures"),
		TEXT("Render every office TV programme with stress data. Args: [OutDir] [NameFilter]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Run));
}

#endif
