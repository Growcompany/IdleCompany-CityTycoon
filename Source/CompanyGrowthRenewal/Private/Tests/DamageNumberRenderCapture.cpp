#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Core/CGGameInstance.h"
#include "UI/Element/Effects/ScoreOrbContainerWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Enum/WidgetType.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"

// 익명 ns 금지 — 유니티 TU 병합 시 재정의 충돌 (CodexRailRenderValidation.cpp 과 같은 규약)
namespace DamageNumberRenderCapture
{
	// 설계 캔버스 2560 폭을 그대로 쓴다 — 게임에서 DPI 배율이 걸리기 전 좌표계라 글자 크기(58/96)가 그 단위다
	static constexpr int32 CaptureWidth = 1100;
	static constexpr int32 RowHeight = 190;

	// 검수 표본 = 자릿수 구간 × 직능색 × 크리티컬
	struct FCase
	{
		const TCHAR* Label;
		int64 Amount;
		int32 Step;
		bool bCritical;
	};
	static const FCase Cases[] = {
		{ TEXT("1_847"),        847,       1, false },
		{ TEXT("2_1man2400"),   12400,     3, false },
		{ TEXT("3_28man3455"),  283455,    5, false },
		{ TEXT("4_4561man"),    45612345,  3, false },
		{ TEXT("5_crit_8800"),  8800,      3, true  },
		{ TEXT("6_crit_9eok"),  912341234, 3, true  },
	};

	static FString CaptureDirectory()
	{
		return FPaths::ProjectSavedDir() / TEXT("DamageNumberCapture");
	}

	static bool DrawToImage(const TSharedRef<SWidget>& Widget, FIntPoint Size, FImage& OutImage)
	{
		FWidgetRenderer Renderer(true);
		// raw target(false) — true target 은 하드웨어 sRGB 쓰기를 켜서 Slate 셰이더 감마와 겹친다
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, false));
		if (!Target.IsValid())
		{
			return false;
		}
		const FVector2D DrawSize(Size.X, Size.Y);
		// 첫 패스는 레이아웃만 세운다 — 지오메트리가 잡힌 다음 패스라야 자식이 제자리 크기를 보고한다
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			Renderer.DrawWidget(Target.Get(), Widget, 1.f, DrawSize, Pass == 0 ? 1.f : 0.f);
			FlushRenderingCommands();
		}
		if (!FImageUtils::GetRenderTargetImage(Target.Get(), OutImage) || OutImage.Format != ERawImageFormat::BGRA8)
		{
			return false;
		}
		// FWidgetRenderer(true) 는 linear 로 태그된 타겟에 이미 감마 인코딩된 바이트를 써 둔다
		OutImage.GammaSpace = EGammaSpace::sRGB;
		return true;
	}

	static bool SavePNG(const FImage& Image, const FString& Path)
	{
		TArray64<uint8> Bytes;
		return FImageUtils::CompressImage(Bytes, TEXT("png"), Image) && FFileHelper::SaveArrayToFile(Bytes, *Path);
	}

	// 그려진 픽셀이 실제로 있는지 — "렌더는 돌았는데 전부 투명" 을 통과로 읽지 않기 위한 단언.
	// ⚠ -nullrhi 에서는 전부 0 이 나오므로 이 테스트는 실제 RHI 가 있는 에디터에서만 유효하다.
	static int32 CountOpaquePixels(const FImage& Image)
	{
		int32 Count = 0;
		TArrayView64<const FColor> Pixels = Image.AsBGRA8();
		for (const FColor& Pixel : Pixels)
		{
			if (Pixel.A > 32)
			{
				++Count;
			}
		}
		return Count;
	}

	class FViewportShotCommand final : public IAutomationLatentCommand
	{
	public:
		FViewportShotCommand(FAutomationTestBase* InTest, UScoreOrbContainerWidget* InContainer,
			IConsoleVariable* InFreeze, int32 InPreviousFreeze, FString InPath)
			: Test(InTest), Container(InContainer), Freeze(InFreeze), PreviousFreeze(InPreviousFreeze),
			  Path(MoveTemp(InPath)), StartTime(FPlatformTime::Seconds()), StartFrame(GFrameCounter)
		{
		}

		virtual ~FViewportShotCommand() override
		{
			FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ProcessedHandle);
			if (Container.IsValid()) { Container->RemoveFromParent(); }
			if (Freeze != nullptr) { Freeze->Set(PreviousFreeze, ECVF_SetByConsole); }
		}

		virtual bool Update() override
		{
			const double Elapsed = FPlatformTime::Seconds() - StartTime;
			if (Elapsed > 30.0)
			{
				Test->AddError(TEXT("데미지 숫자 뷰포트 캡처가 30초 안에 완료되지 않았습니다"));
				return true;
			}
			if (!bRequested)
			{
				// 새 viewport slot의 레이아웃과 첫 표시가 끝난 뒤 촬영한다.
				if (Elapsed < 1.0 || GFrameCounter < StartFrame + 3
					|| (GShaderCompilingManager != nullptr && GShaderCompilingManager->IsCompiling()))
				{
					return false;
				}
				PreviousTimestamp = IFileManager::Get().GetTimeStamp(*Path);
				ProcessedHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddLambda(
					[this]() { bProcessed = true; });
				FScreenshotRequest::RequestScreenshot(Path, true, false);
				bRequested = true;
				return false;
			}
			if (!bProcessed) { return false; }

			const bool bSaved = IFileManager::Get().FileSize(*Path) > 0
				&& IFileManager::Get().GetTimeStamp(*Path) > PreviousTimestamp;
			Test->TestTrue(TEXT("현재 실행의 뷰포트 PNG 저장 완료"), bSaved);
			if (bSaved) { Test->AddInfo(FString::Printf(TEXT("뷰포트 캡처 저장: %s"), *Path)); }
			return true;
		}

	private:
		FAutomationTestBase* Test;
		TStrongObjectPtr<UScoreOrbContainerWidget> Container;
		IConsoleVariable* Freeze;
		int32 PreviousFreeze;
		FString Path;
		double StartTime;
		uint64 StartFrame;
		FDateTime PreviousTimestamp;
		FDelegateHandle ProcessedHandle;
		bool bRequested = false;
		bool bProcessed = false;
	};
}

// 확정안(한국식 전체 표기 · BowlbyOne+Jua · 들쭉날쭉 · 버스트)이 실제로 어떻게 그려지는지 찍는다.
// 판정이 아니라 **눈으로 볼 그림을 만드는** 하네스다 — 자동 단언은 "비어 있지 않다" 까지만 한다.
// ⚠ EditorContext 만 주면 `-game` 스탠드얼론에서 "No automation tests matched" 로 아예 안 잡힌다.
// 이 하네스는 플레이 월드가 필요하고 PIE 는 자동화로 못 띄우므로, -game 에서 도는 게 정상 경로다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDamageNumberRenderCapture, "CGR.Tools.DamageNumberRenderCapture",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FDamageNumberRenderCapture::RunTest(const FString& Parameters)
{
	using namespace DamageNumberRenderCapture;

	// 위젯 생성과 TableManager 조회에 게임 인스턴스가 필요하다 — 빈 커맨드렛에는 없다.
	// PIE 우선 2패스 (CodexRailRenderValidation.cpp 과 같은 패턴) — 둘 다 떠 있을 때 어느 쪽을 잡을지 확정한다.
	UWorld* PlayWorld = nullptr;
	if (GEngine != nullptr)
	{
		for (EWorldType::Type Wanted : { EWorldType::PIE, EWorldType::Game })
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == Wanted && Context.World() != nullptr)
				{
					PlayWorld = Context.World();
					break;
				}
			}
			if (PlayWorld != nullptr)
			{
				break;
			}
		}
	}
	UGameInstance* GameInstance = PlayWorld != nullptr ? PlayWorld->GetGameInstance() : nullptr;
	if (GameInstance == nullptr)
	{
		AddError(TEXT("플레이 월드가 없습니다 — -game 스탠드얼론으로 띄우거나 PIE 를 시작한 뒤 실행해야 합니다"));
		return false;
	}
	UTableManagerSubsystem* TableManager = GameInstance->GetSubsystem<UTableManagerSubsystem>();
	if (!TestNotNull(TEXT("TableManager"), TableManager))
	{
		return false;
	}
	TSubclassOf<UUserWidget> OrbClass = TableManager->GetWidgetClass(EWidgetType::ScoreOrbContainer);
	if (!TestTrue(TEXT("DT_WidgetClass 에 ScoreOrbContainer 가 등록돼 있다"), OrbClass != nullptr))
	{
		return false;
	}

	// 숫자는 1.2초(크리티컬 1.6초) 만에 사라진다 — 멈춰 둬야 찍힌다
	IConsoleVariable* Freeze = IConsoleManager::Get().FindConsoleVariable(TEXT("cg.DamageNumber.Freeze"));
	if (!TestNotNull(TEXT("cg.DamageNumber.Freeze CVar"), Freeze))
	{
		return false;
	}
	const int32 PrevFreeze = Freeze->GetInt();
	Freeze->Set(1, ECVF_SetByConsole);

	const FString Directory = CaptureDirectory();
	IFileManager::Get().MakeDirectory(*Directory, true);

	// ⚠ 쿠킹 안 된 실행은 머티리얼 셰이더를 필요할 때 컴파일한다 — 준비 전에 그리면 Slate 가
	// 기본 머티리얼로 떨어져 글자가 흰색으로 찍힌다("머티리얼이 안 먹는다"로 오진하기 쉽다).
	if (GShaderCompilingManager != nullptr)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}

	// 갓 띄운 세션의 첫 드로우는 폰트 아틀라스 워밍업 전이라 판정에 쓰지 않는다 — 버리는 한 판을 먼저 그린다
	{
		TStrongObjectPtr<UScoreOrbContainerWidget> Warmup(
			CreateWidget<UScoreOrbContainerWidget>(GameInstance, OrbClass));
		if (Warmup.IsValid())
		{
			Warmup->SpawnScoreNumber(FVector2D(CaptureWidth * 0.5f, RowHeight * 0.5f), 12400, 3, false);
			FImage Discarded;
			DrawToImage(Warmup->TakeWidget(), FIntPoint(CaptureWidth, RowHeight), Discarded);
		}
	}

	for (const FCase& Case : Cases)
	{
		TStrongObjectPtr<UScoreOrbContainerWidget> Container(
			CreateWidget<UScoreOrbContainerWidget>(GameInstance, OrbClass));
		if (!TestTrue(FString::Printf(TEXT("%s: 컨테이너 생성"), Case.Label), Container.IsValid()))
		{
			continue;
		}
		Container->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		// 한 장에 하나만 띄운다 — 겹침 해소가 끼면 자리가 흔들려 비교가 안 된다
		Container->SpawnScoreNumber(FVector2D(CaptureWidth * 0.5f, RowHeight * 0.5f),
			Case.Amount, Case.Step, Case.bCritical);

		FImage Image;
		if (!TestTrue(FString::Printf(TEXT("%s: 렌더"), Case.Label),
			DrawToImage(Container->TakeWidget(), FIntPoint(CaptureWidth, RowHeight), Image)))
		{
			continue;
		}

		// 조립 결과를 그대로 적는다 — "그려졌는데 색이 다르다" 는 픽셀만 봐서는 원인을 못 가린다
		{
			TArray<UWidget*> All;
			Container->WidgetTree->GetAllWidgets(All);
			for (UWidget* W : All)
			{
				if (UTextBlock* T = Cast<UTextBlock>(W))
				{
					const FSlateFontInfo& F = T->GetFont();
					AddInfo(FString::Printf(
						TEXT("  [%s] '%s' font=%s size=%.0f outline=%d outlineColor=%s tint=%s mat=%s"),
						*T->GetName(), *T->GetText().ToString(),
						F.FontObject ? *F.FontObject->GetName() : TEXT("NULL"),
						F.Size, F.OutlineSettings.OutlineSize,
						*F.OutlineSettings.OutlineColor.ToString(),
						*T->GetColorAndOpacity().GetSpecifiedColor().ToString(),
						F.FontMaterial ? *F.FontMaterial->GetName() : TEXT("NULL")));
				}
				else if (UImage* Img = Cast<UImage>(W))
				{
					AddInfo(FString::Printf(TEXT("  [%s] image res=%s size=%s"),
						*Img->GetName(),
						Img->GetBrush().GetResourceObject() ? *Img->GetBrush().GetResourceObject()->GetName() : TEXT("NULL"),
						*Img->GetBrush().ImageSize.ToString()));
				}
			}
		}

		const int32 Opaque = CountOpaquePixels(Image);
		// 글자가 하나도 안 그려졌는데 "렌더 성공" 으로 통과하는 걸 막는다. 가장 짧은 표본(847)도
		// 1100x190 에서 수천 px 는 찍힌다 — 0 에 가까우면 폰트 로드나 머티리얼이 깨진 것이다.
		TestTrue(FString::Printf(TEXT("%s: 글자가 실제로 그려졌다 (불투명 %d px)"), Case.Label, Opaque),
			Opaque > 2000);

		const FString Path = Directory / FString::Printf(TEXT("%s.png"), Case.Label);
		TestTrue(FString::Printf(TEXT("%s: PNG 저장"), Case.Label), SavePNG(Image, Path));
		AddInfo(FString::Printf(TEXT("%s -> %s (불투명 %d px)"), Case.Label, *Path, Opaque));
	}

	Freeze->Set(PrevFreeze, ECVF_SetByConsole);
	AddInfo(FString::Printf(TEXT("캡처 폴더: %s"), *Directory));
	return true;
}


// 위의 오프스크린 캡처는 폰트 머티리얼을 못 살린다 — FWidgetRenderer 는 자체 Slate 렌더러로
// 그리는데 그 경로에서 머티리얼이 기본값으로 떨어져 글자가 흰색으로 찍힌다(2026-10-04 실측).
// 색을 판정하려면 **진짜 뷰포트**에 붙여 엔진 스크린샷을 받아야 한다. 구조(림·키라인·자간·
// 높이)는 위 하네스가 더 깔끔하게 보여주므로 둘을 같이 쓴다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDamageNumberViewportShot, "CGR.Tools.DamageNumberViewportShot",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FDamageNumberViewportShot::RunTest(const FString& Parameters)
{
	using namespace DamageNumberRenderCapture;

	UWorld* PlayWorld = nullptr;
	if (GEngine != nullptr)
	{
		for (EWorldType::Type Wanted : { EWorldType::PIE, EWorldType::Game })
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == Wanted && Context.World() != nullptr)
				{
					PlayWorld = Context.World();
					break;
				}
			}
			if (PlayWorld != nullptr)
			{
				break;
			}
		}
	}
	UGameInstance* GameInstance = PlayWorld != nullptr ? PlayWorld->GetGameInstance() : nullptr;
	if (GameInstance == nullptr)
	{
		AddError(TEXT("플레이 월드가 없습니다 — -game 스탠드얼론으로 띄워야 합니다"));
		return false;
	}
	UTableManagerSubsystem* TableManager = GameInstance->GetSubsystem<UTableManagerSubsystem>();
	TSubclassOf<UUserWidget> OrbClass = TableManager != nullptr
		? TableManager->GetWidgetClass(EWidgetType::ScoreOrbContainer) : nullptr;
	if (!TestTrue(TEXT("ScoreOrbContainer 위젯 클래스"), OrbClass != nullptr))
	{
		return false;
	}

	UScoreOrbContainerWidget* Container = CreateWidget<UScoreOrbContainerWidget>(GameInstance, OrbClass);
	if (!TestNotNull(TEXT("컨테이너 생성"), Container))
	{
		return false;
	}
	IConsoleVariable* Freeze = IConsoleManager::Get().FindConsoleVariable(TEXT("cg.DamageNumber.Freeze"));
	const int32 PreviousFreeze = Freeze != nullptr ? Freeze->GetInt() : 0;
	if (Freeze != nullptr) { Freeze->Set(1, ECVF_SetByConsole); }
	Container->SetVisibility(ESlateVisibility::HitTestInvisible);
	Container->AddToViewport(9000);

	FVector2D ViewSize(1600.f, 900.f);
	if (GEngine != nullptr && GEngine->GameViewport != nullptr)
	{
		GEngine->GameViewport->GetViewportSize(ViewSize);
	}
	// ⚠ 캔버스 좌표는 설계 좌표계다 — 뷰포트 픽셀을 그대로 쓰면 DPI 배율만큼 위로 몰린다
	const float DPIScale = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(PlayWorld));
	ViewSize /= DPIScale;
	const int32 RowCount = UE_ARRAY_COUNT(Cases);
	// 상단 HUD(재화 바)와 하단 도크를 피해 가운데 띠에 깐다 — 겹치면 판정이 안 된다
	const float Top = ViewSize.Y * 0.26f;
	const float Pitch = (ViewSize.Y * 0.52f) / RowCount;
	// 아래 행부터 만들면 같은 열의 겹침 해소가 위 행을 화면 위로 밀지 않는다.
	for (int32 i = RowCount - 1; i >= 0; --i)
	{
		// ⚠ 가로로도 벌려야 한다 — 같은 열이면 겹침 해소가 전부 위로 쌓아 화면 밖으로 밀어낸다
		const float X = ViewSize.X * ((i % 2 == 0) ? 0.30f : 0.70f);
		Container->SpawnScoreNumber(FVector2D(X, Top + Pitch * i),
			Cases[i].Amount, Cases[i].Step, Cases[i].bCritical);
	}

	// 머티리얼 프로브 — 같은 MIC 를 평범한 UImage 로도 띄운다.
	// 여기는 색이 나오는데 글자만 희면 범인은 머티리얼이 아니라 **폰트 머티리얼 경로**다.
	if (UMaterialInterface* Probe = Cast<UMaterialInterface>(
		StaticLoadObject(UMaterialInterface::StaticClass(), nullptr,
			TEXT("/Game/CompanyGrowth/UI/Materials/DamageNumber/MI_UI_DamageNumber_Step3.MI_UI_DamageNumber_Step3"))))
	{
		if (UImage* Swatch = NewObject<UImage>(Container))
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(Probe);
			Brush.ImageSize = FVector2D(320.f, 90.f);
			Swatch->SetBrush(Brush);
			Swatch->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UCanvasPanelSlot* ProbeSlot = Container->AddNumberProbeSwatch(Swatch))
			{
				ProbeSlot->SetAutoSize(true);
				ProbeSlot->SetPosition(FVector2D(40.f, ViewSize.Y - 140.f));
			}
		}
	}

	const FString ShotPath = FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("DamageNumberViewport.png");
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FViewportShotCommand>(
		this, Container, Freeze, PreviousFreeze, ShotPath));
	return true;
}

#endif
