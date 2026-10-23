#include "Misc/AutomationTest.h"
#include "ICommonInputModule.h"
#include "CommonInputSettings.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "UI/Element/LiveOps/AttendanceDayTileWidget.h"
#include "UI/Element/LiveOps/AttendanceMilestoneNodeWidget.h"
#include "UI/Panel/AttendancePanelWidget.h"
#include "Tests/WidgetDeepLookup.h"

#include "Blueprint/UserWidget.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Containers/ArrayView.h"
#include "Data/AttendanceRules.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "LevelEditorViewport.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Slate/WidgetRenderer.h"
#include "Styling/CoreStyle.h"
#include "Table/AttendanceTable.h"
#include "Table/ResourceInfo.h"
#include "Table/ShopItemTable.h"
#include "Table/UIVFXTexTable.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Cards/ItemCardSlotWidget.h"
#include "UI/Element/Common/RewardChipUtils.h"
#include "Utils/FWidgetAnimationUtils.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

// 익명 ns 금지 — 유니티 TU 병합 시 재정의 충돌 (LiveOpsMenuTileRenderValidation.cpp 와 같은 규약)
namespace LiveOpsAttendance
{
	static FString Signature(const FAttendanceTileVisibility& V)
	{
		return FString::Printf(TEXT("%d%d%d%d%d|%d%d%d|%d%d|%d%d"),
			V.bPlateNormal, V.bPlateToday, V.bPlateGold, V.bPlateDone, V.bPlateGoldDone,
			V.bBadgeDefault, V.bBadgeToday, V.bBadgeMilestone,
			V.bTodayRing, V.bTodaySweep,
			V.bSeal, V.bIconDimmed);
	}

	// 상태 잉크·곱색·배치 노브는 위젯 CDO 의 protected 저작값이다 — 테스트는 리플렉션으로 읽어 기대값을 코드에 두 벌 적지 않는다
	template <typename TValue>
	static const TValue* ReadWidgetProperty(const UObject& Widget, const TCHAR* Name)
	{
		const FProperty* Property = Widget.GetClass()->FindPropertyByName(FName(Name));
		return Property && Property->GetSize() == static_cast<int32>(sizeof(TValue))
			? Property->ContainerPtrToValuePtr<TValue>(&Widget) : nullptr;
	}

	// ==== 렌더 하네스 (CGR.LiveOps.AttendanceRender) =========================
	static const TCHAR* PanelPath =
		TEXT("/Game/CompanyGrowth/UI/Panels/MainMap/UI_AttendancePanel.UI_AttendancePanel_C");
	static const TCHAR* TilePath =
		TEXT("/Game/CompanyGrowth/UI/Elements/LiveOps/UIE_AttendanceDayTile.UIE_AttendanceDayTile_C");
	static const TCHAR* VFXTablePath = TEXT("/Game/CompanyGrowth/Table/UI/DT_UIVFXTexture.DT_UIVFXTexture");
	// 실물 픽스처 — TableManagerSubsystem 생성자와 같은 세 표. 패널 BuildView 와 같은 길(FillDayFromRow)로 28칸을 채운다
	static const TCHAR* AttendanceTablePath = TEXT("/Game/CompanyGrowth/Table/LiveOps/DT_Attendance.DT_Attendance");
	static const TCHAR* ShopItemTablePath = TEXT("/Game/CompanyGrowth/Table/Shop/DT_ShopItem.DT_ShopItem");
	static const TCHAR* ResourceTablePath = TEXT("/Game/CompanyGrowth/Table/DT_Resource.DT_Resource");
	// 단독 칸 픽스처가 얹을 실제 아이콘 — DT 없이도 칸 상태 10벌을 같은 그림으로 대조한다
	static const TCHAR* DiamondIconPath = TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/DiamondIcon.DiamondIcon");
	static const TCHAR* PremiumTicketPath =
		TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/RecruitTicketPremium.RecruitTicketPremium");
	// 래스터 대조군 두 벌. 트레이는 판정 대상과 **다른 마스터**라 "이 세션이 머티리얼 면을 그리는가"를 답하고,
	// 플레이트 마스터는 같은 마스터를 하네스가 제 브러시로 그려 "이 마스터가 이 패스에서 그려지는가"를 답한다.
	// 둘을 나눈 이유는 실측이다 — 한 패스 안에서 트레이는 알파 247, 플레이트 마스터는 46 이 나온 적이 있다
	static const TCHAR* ControlMaterialPath = TEXT("/Game/CompanyGrowth/UI/Materials/MI_UI_TrayBG.MI_UI_TrayBG");
	// 실제 판과 같은 마스터를 대조군으로 삼아 셰이더 문맥 실패와 위젯 회귀를 구분한다.
	static const TCHAR* PlateMasterPath =
		TEXT("/Game/CompanyGrowth/UI/Materials/LiveOps/M_UI_LiveOpsEntrySurface.M_UI_LiveOpsEntrySurface");

	static constexpr int32 FrameHeight = 1440;
	// 16:10(2304) ~ 19.5:9(3120) 커버 — 2560 만 재면 양 끝이 안 잡힌다
	static constexpr int32 FrameWidths[] = { 2304, 2560, 3120 };
	// 칸 단위 지오메트리는 기준 해상도 한 폭에서만 잰다 (같은 값이 3번 나올 자리다)
	static constexpr int32 MeasureWidth = 2560;

	// 아담한 판 계약 (설계 v5 §11 · 시안 「아담한 판」 · Tools/LiveOps/attendance_v5.py panel()). 판 좌표 = FrameStack 로컬(축소 전).
	// 기저 상수만 적고 나머지는 파생한다 — 매직 넘버를 두 벌로 적으면 격자를 고쳤을 때 테스트만 옛 값에 남는다
	static constexpr double ShellWidth = 1580.0, ShellHeight = 1380.0;
	static constexpr double GridLeft = 40.0, GridTop = 168.0;
	static constexpr double GridWidth = 1500.0, GridHeight = 972.0;
	static constexpr double WellPad = 8.0, SlotPad = 6.0;
	// 아래 한 줄 = 격자 아래 24 · 1500×176(받기 154 + 아래 두께 8 이 들어가는 높이). 그 안의 막대·받기 줄은 줄 기준 좌표다
	static constexpr double BottomRowGap = 24.0, BottomRowWidth = 1500.0, BottomRowHeight = 176.0;
	static constexpr double BarLeft = 44.0, BarTop = 72.0, BarWidth = 904.0, BarHeight = 28.0;
	static constexpr double CtaLeft = 984.0, CtaTop = 7.0, CtaWidth = 480.0, CtaHeight = 154.0;
	static constexpr double ClaimTextSize = 37.5;
	// 원판 = 지름 68. 위젯 상자는 굽기 캔버스(80)일 수 있어 중심만 판정하고 시안 대조용 사각형은 중심 ± 34 로 낸다
	static constexpr double NodePlateSize = 68.0;
	// 달력 고리 8개는 한 장(T_UI_AttV5_Rings 1580×110)에 구워져 있다 — 판 위로 −51(검은 테 5 포함), 고리 몸통 x 는 굽기 그대로
	static constexpr double RingsCanvasTop = -51.0, RingsCanvasHeight = 110.0, RingBodyTop = 5.0;
	static constexpr double RingWidth = 34.0, RingHeight = 96.0;
	static constexpr double RingX[] = { 140.0, 260.0, 380.0, 500.0, 1046.0, 1166.0, 1286.0, 1406.0 };
	// 판 전체는 그리기만 줄인다 — 레이아웃은 시안 치수 그대로라 패널 상자는 FrameStack 로컬에서 잰다
	static constexpr double PanelScale = 0.82;

	static constexpr int32 GridColumns = 7;
	static constexpr double CellPitchX = (GridWidth - 2.0 * WellPad) / 7.0;    // 212
	static constexpr double CellPitchY = (GridHeight - 2.0 * WellPad) / 4.0;   // 239
	static constexpr double TileWidth = CellPitchX - 2.0 * SlotPad;            // 200
	static constexpr double TileHeight = CellPitchY - 2.0 * SlotPad;           // 227

	// v5 칸 계약 (시안 .tile 198×226) — 칸 안은 위·아래 기준 시안 px 그대로, 남는 높이는 아이콘 상자와 수량 사이 틈이 먹는다
	static constexpr double StripHeight = 50.0;
	// v5 머리띠 「12일」 한 줄 = NEXON Bold 32px × 0.75 (스펙 ⑥)
	static constexpr float DateFontSize = 24.f;
	// 머리띠 글자(+왕관)는 칸 좌우 가장자리에서 이만큼은 떨어져야 이웃 칸으로 새지 않는다
	static constexpr double StripTextMinMargin = 8.0;
	// 칸 아이콘 상자 = 칸 CDO IconBoxWidth/Height. 보상 둘이면 폭 반 · 아이콘 높이 0.72, 카드 높이는 그대로 (스펙 ②·②-b)
	static constexpr double TileIconBoxWidth = 146.0, TileIconBoxHeight = 108.0;
	static constexpr double PairBoxHeightRatio = 0.72;
	// 보상 둘 줄(28일차)이 칸 좌우 가장자리에서 남겨야 할 최소 여백
	static constexpr double PairRowMinMargin = 8.0;
	// 아이콘 상자 중심 = 시안 112(.box top 58 + 54) + 시안보다 큰 칸 높이의 절반 (attendance_v5.py tile() DoneMark 자리)
	static constexpr double MockTileHeight = 226.0, MockIconCenterY = 112.0;
	static constexpr double IconCenterY = MockIconCenterY + (TileHeight - MockTileHeight) * 0.5;
	// 결재 도장 굽기 캔버스 160 안의 도장 지름 132 (T_UI_AttV5_Seal, 놓인 −14° 는 텍스처에 구워져 위젯 각도 0)
	static constexpr double SealCanvas = 160.0, SealInkDiameter = 132.0;
	// 카드 수량 라벨 (시안 38px 외곽 7 → 28.5pt · 외곽 3, attendance_v5.py reward())
	static constexpr double CardLabelFontSize = 28.5;
	static constexpr int32 CardLabelOutline = 3;
	// 수량 줄 = 칸 아래 16 · 줄 높이 38 (시안 .qty) — 잉크 중심은 줄 상자 가운데에 선다
	static constexpr double MockQtyBottom = 16.0, MockQtyLineHeight = 38.0;
	static constexpr double QtyInkCenterY = TileHeight - MockQtyBottom - MockQtyLineHeight * 0.5;
	static constexpr double QtyInkCenterTolerance = 4.0;
	// 탭 픽스처가 쓰는 일차 — 브로드캐스트가 날짜를 그대로 실어 보내는지 보려면 1 이 아니어야 한다 (R33)
	static constexpr int32 TapSampleDay = 12;
	// 마일스톤 간격은 열 수와 값이 같을 뿐 다른 축이다 (7열 격자 / 7일마다 특별 보상)
	static constexpr int32 MilestoneStride = 7;
	// 타일 픽스처 중 비마일스톤이 쓰는 날짜 — **두 자리**여야 한다.
	// 28칸 중 19칸이 두 자리라 머리띠 폭 판정은 여기서 나야 하고, 한 자리로 재면 라벨이 넘쳐도 초록이다
	static constexpr int32 PlainSampleDay = 26;

	// 판·링·특별 칸 꾸밈은 칸 사방 12 를 더 가진 굽기 캔버스다 — 슬롯 음수 패딩으로 칸 위에 정확히 겹친다 (attendance_v4.py PAD)
	static constexpr double PlateOutset = 12.0;
	// 구운 오늘 링 = 칸 밖 검은 테 5 + 파랑 4. 탭 플래시(흰 테 4)도 같은 띠에 두른다
	static constexpr double RingInkOutset = 9.0, HaloFlashThickness = 4.0;
	static_assert(RingInkOutset < 2.0 * SlotPad, "오늘 링은 이웃 칸 면에 닿으면 안 된다 ― 칸 사이 간격은 슬롯 패딩 두 벌이다");
	// 판 면 프로브 — 구운 머리띠·아이콘 상자와 보상 그림·수량 열을 모두 비킨 왼쪽 아래 맨 판 면을 잰다
	static constexpr int32 ProbeBlock = 4;
	static constexpr double ProbeMargin = 5.0;
	static constexpr double PlateProbeX = 20.0, PlateProbeY = 196.0;
	// 판 안쪽 테(특별 칸 금테 3)와 아래 두께 7 (att_v5_bake .tile)
	static constexpr double PlateRimWidth = 3.0, PlateLipHeight = 7.0;
	static_assert(PlateProbeX > PlateRimWidth, "판 프로브는 판 테 안쪽에 있어야 한다");
	// 맞춤 그림은 아이콘 상자를 넘지 않는다 — 상자 왼쪽이 곧 가장 넓은 그림의 왼쪽 끝이다
	static_assert(PlateProbeX + ProbeBlock < (TileWidth - TileIconBoxWidth) * 0.5,
		"판 프로브는 아이콘 상자 왼쪽에 있어야 한다");
	static_assert(PlateProbeY > IconCenterY + TileIconBoxHeight * 0.5 && PlateProbeY + ProbeBlock < TileHeight - PlateLipHeight,
		"판 프로브는 아이콘 상자 아래, 판 아래 두께 위에 있어야 한다");
	static constexpr double MinPlateWellLumaDelta = 8.0;
	// 단독 타일(뒤가 투명)의 플레이트 지점 알파 — 이 값을 넘으면 판 면이 실제로 그려진 것이다
	static constexpr double MinFaceAlpha = 150.0;
	// 대조군 기둥 폭 — 타일 왼쪽에 붙여 같은 패스에 그린다(위에서부터 단색 · 트레이 · 플레이트 마스터 3등분)
	static constexpr int32 ControlColumnWidth = 64;
	// 받은 칸으로 잴 때의 완화 하한 — 받은 칸은 한 단 어두운 판이라 밝기 차가 줄어든다.
	// 세 픽스처 모두 받지 않은 칸이 있어 쓰이지 않는다 — "전 칸이 받은 칸"인 픽스처가 생겼을 때의 안전망이다
	static constexpr double MinDimmedPlateWellLumaDelta = 3.0;

	static constexpr double Tolerance = 1.5;
	// 셸/격자 실측은 레이아웃 패스 반올림으로 1px 안팎 흔들린다 (R5)
	static constexpr double BoxTolerance = 2.0;

	struct FLayoutBox
	{
		bool bFound = false;
		double Top = 0.0, Left = 0.0, Width = 0.0, Height = 0.0;
		double Right() const { return Left + Width; }
		double Bottom() const { return Top + Height; }
		double CenterX() const { return Left + Width * 0.5; }
		double CenterY() const { return Top + Height * 0.5; }
	};

	struct FPixelSample
	{
		double R = 0.0, G = 0.0, B = 0.0, A = 0.0;   // 0..255 바이트 평균
		double Luma() const { return 0.2126 * R + 0.7152 * G + 0.0722 * B; }
	};

	struct FTextureBinding
	{
		const TCHAR* Widget;
		const TCHAR* Row;
	};

	// AttendancePanelWidget::NativeConstruct 의 주입 표와 같은 짝 — 여기가 어긋나면 버튼 빛줄기가 안 그려진다
	static const FTextureBinding PanelTextures[] = {
		{ TEXT("ClaimShine"), TEXT("CTAShine") },
	};

	// 칸·원판의 그림 층은 WBP 가 구운 텍스처를 소유한다(런타임 주입 없음) — 하네스가 덮지 않고 저작 리소스가 그대로인지 본다
	static const TCHAR* AttendanceTextureDir = TEXT("/Game/CompanyGrowth/UI/Textures/LiveOps/Attendance/V5/");
	struct FAuthoredLayer
	{
		const TCHAR* Widget;
		const TCHAR* Texture;
		bool bPlateCanvas;   // 칸 사방 PlateOutset 바깥 상자에 겹치는 굽기 캔버스인가
	};
	// 판 5벌 = 칸 사방 12 바깥 캔버스. 오늘 링(사방 40)·빛띠·도장은 캔버스가 달라 텍스처만 본다 (Task 3 굽기)
	static const FAuthoredLayer TileAuthoredLayers[] = {
		{ TEXT("TilePlate"), TEXT("T_UI_AttV5_PlateNormal"), true },
		{ TEXT("TilePlateToday"), TEXT("T_UI_AttV5_PlateToday"), true },
		{ TEXT("TilePlateGold"), TEXT("T_UI_AttV5_PlateGold"), true },
		{ TEXT("TilePlateDone"), TEXT("T_UI_AttV5_PlateDone"), true },
		{ TEXT("TilePlateGoldDone"), TEXT("T_UI_AttV5_PlateGoldDone"), true },
		{ TEXT("TodayRing"), TEXT("T_UI_AttV5_TodayRing"), false },
		{ TEXT("TodaySweep"), TEXT("T_UI_AttV5_TodaySweep"), false },
		{ TEXT("DoneMark"), TEXT("T_UI_AttV5_Seal"), false },
	};
	// 원판 판 3벌·흑백 선물 (Task 3 굽기 Node*·GiftGray). 컬러 선물은 공용 CasualPack 원본이라 경로를 박지 않는다
	static const TPair<const TCHAR*, const TCHAR*> NodeAuthoredLayers[] = {
		{ TEXT("PlateGot"), TEXT("T_UI_AttV5_NodeGot") },
		{ TEXT("PlateNext"), TEXT("T_UI_AttV5_NodeNext") },
		{ TEXT("PlatePending"), TEXT("T_UI_AttV5_NodePending") },
		{ TEXT("GiftGray"), TEXT("T_UI_AttV5_GiftGray") },
	};
	// 상시 발광 층과 옛 수량 알약은 철거됐다 (설계 v3 §0-3 · ⑥). v5 는 빛줄기·금 액자·옛 아이콘 자리·딤·잔광·「일」 글자·
	// v4 오늘 덧칠 층(오늘 면은 TilePlateToday)을 걷는다 (스펙 §6)
	static const TCHAR* const RetiredTileLayers[] = {
		TEXT("TodayHalo"), TEXT("TodaySparkle"), TEXT("ExtraRewardBoxBG"), TEXT("ExtraRewardBoxLine"),
		TEXT("MilestoneArc"), TEXT("MilestoneFrame"), TEXT("Backlight"), TEXT("BacklightGold"),
		TEXT("DimLayer"), TEXT("ClaimedGlow"), TEXT("DaySuffixText"), TEXT("MilestoneSuffixText"), TEXT("DayBand"),
		TEXT("TodayGlow") };

	static FString RenderDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("LiveOpsUI/render/attendance"));
	}

	// 위치를 Space 로컬로 되돌린다 — GetLocalSize 는 렌더 트랜스폼 전 크기라 위치도 같은 공간이어야 우·하단 비교가 맞는다.
	// Space 가 없으면 절대 좌표다(단독 타일은 원점·배율 1 이라 그대로 칸 로컬이다)
	static FVector2D OriginIn(const UWidget& Widget, const UWidget* Space)
	{
		const FVector2D Absolute = FVector2D(Widget.GetCachedGeometry().GetAbsolutePosition());
		return Space ? FVector2D(Space->GetCachedGeometry().AbsoluteToLocal(Absolute)) : Absolute;
	}

	static FLayoutBox MeasureWidget(const UWidget* Found, const UWidget* Space)
	{
		FLayoutBox Out;
		if (Found == nullptr)
		{
			return Out;
		}
		const FGeometry& Geometry = Found->GetCachedGeometry();
		const FVector2D Position = OriginIn(*Found, Space);
		const FVector2D Size = FVector2D(Geometry.GetLocalSize());
		Out.bFound = true;
		Out.Left = Position.X;
		Out.Top = Position.Y;
		Out.Width = Size.X;
		Out.Height = Size.Y;
		return Out;
	}

	static FLayoutBox Measure(UUserWidget& Root, const TCHAR* Name, const UWidget* Space = nullptr)
	{
		return MeasureWidget(FindWidgetDeep(&Root, FName(Name)), Space);
	}

	// 픽셀을 읽을 상자 — 위치·크기 모두 렌더 트랜스폼(판 축소 PanelScale)까지 들어간 프레임 픽셀 공간이다
	static FLayoutBox MeasurePixels(UUserWidget& Root, const TCHAR* Name)
	{
		FLayoutBox Out;
		const UWidget* Found = FindWidgetDeep(&Root, FName(Name));
		if (Found == nullptr)
		{
			return Out;
		}
		const FGeometry& Geometry = Found->GetCachedGeometry();
		const FVector2D Position = FVector2D(Geometry.GetAbsolutePosition());
		const FVector2D Size = FVector2D(Geometry.GetAbsoluteSize());
		Out.bFound = true;
		Out.Left = Position.X;
		Out.Top = Position.Y;
		Out.Width = Size.X;
		Out.Height = Size.Y;
		return Out;
	}

	// 위젯 로컬 점 → 프레임 픽셀. 판 축소(PanelScale)는 로컬 오프셋에도 걸린다
	static FIntPoint PixelOf(const UWidget& Widget, double LocalX, double LocalY)
	{
		const FVector2D Absolute = FVector2D(Widget.GetCachedGeometry().LocalToAbsolute(FVector2D(LocalX, LocalY)));
		return FIntPoint(FMath::RoundToInt32(Absolute.X), FMath::RoundToInt32(Absolute.Y));
	}

	// 경로까지 본다 — 이름만 보면 다른 폴더의 같은 이름 텍스처가 초록으로 통과한다
	static bool BrushUsesTexture(const FSlateBrush& Brush, const TCHAR* TextureName)
	{
		const UTexture2D* Texture = Cast<UTexture2D>(Brush.GetResourceObject());
		return Texture != nullptr
			&& Texture->GetPathName() == FString::Printf(TEXT("%s%s.%s"), AttendanceTextureDir, TextureName, TextureName);
	}

	static FString BrushResourceName(const FSlateBrush& Brush)
	{
		const UObject* Resource = Brush.GetResourceObject();
		return Resource ? Resource->GetPathName() : FString(TEXT("None"));
	}

	static TSharedPtr<FJsonObject> BoxJson(const FLayoutBox& Box)
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetBoolField(TEXT("found"), Box.bFound);
		Out->SetNumberField(TEXT("left"), Box.Left);
		Out->SetNumberField(TEXT("top"), Box.Top);
		Out->SetNumberField(TEXT("width"), Box.Width);
		Out->SetNumberField(TEXT("height"), Box.Height);
		return Out;
	}

	// 시안 대조(Tools/MockupParity)가 읽는 이름 붙은 사각형 — 어느 위젯에서 왔는지(또는 무엇에서 파생했는지)를 같이 남긴다
	static void SetNamedRect(const TSharedPtr<FJsonObject>& Rects, const TCHAR* Name, const FLayoutBox& Box, const FString& Source)
	{
		const TSharedPtr<FJsonObject> Entry = BoxJson(Box);
		Entry->SetStringField(TEXT("source"), Source);
		Rects->SetObjectField(Name, Entry);
	}

	static FLayoutBox BoxAround(double CenterX, double CenterY, double Width, double Height, bool bFound)
	{
		FLayoutBox Out;
		Out.bFound = bFound;
		Out.Left = CenterX - Width * 0.5;
		Out.Top = CenterY - Height * 0.5;
		Out.Width = Width;
		Out.Height = Height;
		return Out;
	}

	static const FColor* PixelAt(const FImage& Frame, int32 X, int32 Y)
	{
		if (Frame.Format != ERawImageFormat::BGRA8 || X < 0 || Y < 0 || X >= Frame.SizeX || Y >= Frame.SizeY)
		{
			return nullptr;
		}
		return reinterpret_cast<const FColor*>(Frame.RawData.GetData()) + static_cast<int64>(Y) * Frame.SizeX + X;
	}

	// 블록 평균 — 단일 픽셀은 AA 가장자리에 걸리면 판정이 흔들린다
	static FPixelSample SampleBlock(const FImage& Frame, int32 OriginX, int32 OriginY, int32 Size)
	{
		FPixelSample Out;
		int32 Count = 0;
		for (int32 Y = OriginY; Y < OriginY + Size; ++Y)
		{
			for (int32 X = OriginX; X < OriginX + Size; ++X)
			{
				if (const FColor* Pixel = PixelAt(Frame, X, Y))
				{
					Out.R += Pixel->R;
					Out.G += Pixel->G;
					Out.B += Pixel->B;
					Out.A += Pixel->A;
					++Count;
				}
			}
		}
		if (Count > 0)
		{
			const double Divisor = Count;
			Out.R /= Divisor; Out.G /= Divisor; Out.B /= Divisor; Out.A /= Divisor;
		}
		return Out;
	}

	static bool DrawToImage(const TSharedRef<SWidget>& Widget, FIntPoint Size, FImage& OutImage)
	{
		FWidgetRenderer Renderer(true);
		TStrongObjectPtr<UTextureRenderTarget2D> RenderTarget(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, false));
		if (!RenderTarget.IsValid())
		{
			return false;
		}
		const FVector2D DrawSize(Size.X, Size.Y);
		// 세 패스 — 앞 두 회는 레이아웃만 세운다(28칸 UniformGrid 는 지오메트리가 잡힌 다음 패스에서 자리를 얻는다)
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			Renderer.DrawWidget(RenderTarget.Get(), Widget, 1.f, DrawSize, 0.f);
			FlushRenderingCommands();
		}
		if (!FImageUtils::GetRenderTargetImage(RenderTarget.Get(), OutImage)
			|| OutImage.Format != ERawImageFormat::BGRA8)
		{
			return false;
		}
		// FWidgetRenderer(true) 는 linear 로 태그된 타겟에 이미 감마 인코딩된 바이트를 써 둔다
		OutImage.GammaSpace = EGammaSpace::sRGB;
		return true;
	}

	static bool SavePNG(const FImage& Frame, const FString& Path)
	{
		TArray64<uint8> Bytes;
		return FImageUtils::CompressImage(Bytes, TEXT("png"), Frame) && FFileHelper::SaveArrayToFile(Bytes, *Path);
	}

	// 머티리얼 브러시는 직전 씬 렌더의 씬 텍스처 추출물이 있어야 그려진다 — 백그라운드 에디터는 씬을 안 그린다.
	// 그린 레벨 뷰포트가 하나도 없으면(커맨드릿 -unattended) 면은 애초에 래스터될 수 없다
	static int32 ForceViewportDraw()
	{
		int32 Drawn = 0;
		if (GEditor == nullptr)
		{
			return Drawn;
		}
		for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
		{
			if (ViewportClient && ViewportClient->Viewport)
			{
				ViewportClient->Viewport->Draw(true);
				++Drawn;
			}
		}
		FlushRenderingCommands();
		return Drawn;
	}

	// 런타임 주입(FUIVFXTextureUtils::SetTextureKeepStyle)은 GameInstance 서브시스템을 타 에디터 월드에서 안 돈다 —
	// 같은 계약(ResourceObject 만 교체해 저작 틴트/ImageSize 를 살린다)을 DT 에서 직접 읽어 하네스가 대신 건다
	static bool InjectTexture(UUserWidget& Root, const FTextureBinding& Binding, const UDataTable* Table)
	{
		UImage* Layer = Cast<UImage>(Root.GetWidgetFromName(FName(Binding.Widget)));
		const FUIVFXTexRow* Row = Table
			? Table->FindRow<FUIVFXTexRow>(FName(Binding.Row), TEXT("AttendanceRenderHarness"), false)
			: nullptr;
		if (Layer == nullptr || Row == nullptr || Row->Texture.IsNull())
		{
			return false;
		}
		UTexture2D* Texture = Row->Texture.LoadSynchronous();
		if (Texture == nullptr)
		{
			return false;
		}
		Layer->SetBrushResourceObject(Texture);
		return true;
	}

	static int32 InjectAll(UUserWidget& Root, const UDataTable* Table, TArrayView<const FTextureBinding> Bindings)
	{
		int32 Applied = 0;
		for (const FTextureBinding& Binding : Bindings)
		{
			Applied += InjectTexture(Root, Binding, Table) ? 1 : 0;
		}
		return Applied;
	}

	// 머리띠 날짜 세 벌은 같은 자리에 겹쳐 있고 한 벌만 보인다. 접힌 벌은 0x0 으로 재진다 — 상태표가 켜 준 벌을 골라야 실치수를 잰다
	static const TCHAR* VisibleBadge(EAttendanceTileState State, bool bMilestone)
	{
		const FAttendanceTileVisibility Resolved = FAttendanceTileVisibility::Resolve(State, bMilestone);
		return Resolved.bBadgeMilestone ? TEXT("MilestoneBadge")
			: Resolved.bBadgeToday ? TEXT("DayBadgeToday") : TEXT("DayBadge");
	}

	// 상태마다 하나의 판 Image만 켜진다.
	static const TCHAR* VisiblePlateStack(EAttendanceTileState State, bool bMilestone)
	{
		const FAttendanceTileVisibility Resolved = FAttendanceTileVisibility::Resolve(State, bMilestone);
		return Resolved.bPlateToday ? TEXT("TilePlateToday")
			: Resolved.bPlateGold ? TEXT("TilePlateGold")
			: Resolved.bPlateGoldDone ? TEXT("TilePlateGoldDone")
			: Resolved.bPlateDone ? TEXT("TilePlateDone") : TEXT("TilePlate");
	}

	// 켜진 배지의 날짜 라벨. 상자가 상한에 걸리면 HorizontalBox 는 라벨에 desired 를 그대로 줘
	// **라벨만** 배지 밖으로 샌다 — 상자 측정으로는 절대 안 잡히는 자리다
	static const TCHAR* VisibleBadgeLabel(EAttendanceTileState State, bool bMilestone)
	{
		const FAttendanceTileVisibility Resolved = FAttendanceTileVisibility::Resolve(State, bMilestone);
		return Resolved.bBadgeMilestone ? TEXT("MilestoneDayText")
			: Resolved.bBadgeToday ? TEXT("DayTodayText") : TEXT("DayText");
	}

	// 머리띠 안 글자 줄 = 「12일」(+ 특별 칸이면 왼쪽 왕관). 둘을 합친 가로 범위가 칸 가운데에 서야 한다
	static FLayoutBox StripContent(UUserWidget& Tile, EAttendanceTileState State, bool bMilestone)
	{
		FLayoutBox Content = Measure(Tile, VisibleBadgeLabel(State, bMilestone), &Tile);
		if (Content.bFound && FAttendanceTileVisibility::Resolve(State, bMilestone).bBadgeMilestone)
		{
			const FLayoutBox Crown = Measure(Tile, TEXT("MilestoneCrown"), &Tile);
			if (Crown.bFound && Crown.Width > 0.0)
			{
				const double Left = FMath::Min(Content.Left, Crown.Left);
				const double Right = FMath::Max(Content.Right(), Crown.Right());
				Content.Left = Left;
				Content.Width = Right - Left;
			}
		}
		return Content;
	}

	// 카드 = 아이콘 상자. 보상이 둘이면 폭만 반이 되고 높이는 그대로다 (AttendanceDayTileWidget::ApplyIconLayout)
	static FVector2D ExpectedCardSize(bool bHasExtra)
	{
		return FVector2D(bHasExtra ? TileIconBoxWidth * 0.5 : TileIconBoxWidth, TileIconBoxHeight);
	}

	// DT_Attendance 의 bMilestone 만 옮겨 담은 최소 표 — 안내 줄·원판이 코드 상수가 아니라
	// 이 표에서 답을 뽑는지 보려면 일차 목록을 픽스처가 쥐고 있어야 한다
	static void MarkMilestoneDays(FAttendanceView& View, const TArray<int32>& MilestoneDays)
	{
		View.Days.SetNum(FAttendanceRules::CycleLength);
		for (int32 Index = 0; Index < FAttendanceRules::CycleLength; ++Index)
		{
			FAttendanceDayView& DayView = View.Days[Index];
			DayView.Day = Index + 1;
			DayView.bRowFound = true;
			DayView.bMilestone = MilestoneDays.Contains(DayView.Day);
		}
	}

	// 특별 칸 원판 상태를 일차 순서대로 한 글자씩(G=받음 N=다음 P=남음) — 표 하나로 네 원판을 한 번에 대조한다
	static FString NodeStates(const FAttendanceView& View)
	{
		FString Out;
		for (const FAttendanceDayView& Entry : View.Days)
		{
			if (!Entry.bRowFound || !Entry.bMilestone)
			{
				continue;
			}
			const EAttendanceNodeState State = View.MilestoneNodeState(Entry.Day);
			Out.AppendChar(State == EAttendanceNodeState::Got ? TEXT('G') : State == EAttendanceNodeState::Next ? TEXT('N') : TEXT('P'));
		}
		return Out;
	}

	static const TCHAR* NodePlateName(EAttendanceNodeState State)
	{
		return State == EAttendanceNodeState::Got ? TEXT("PlateGot")
			: State == EAttendanceNodeState::Next ? TEXT("PlateNext") : TEXT("PlatePending");
	}

	// 실물 표 세 장 — 에디터 월드에는 GameInstance 가 없어 TableManager 대신 DT 를 직접 읽는다
	struct FRewardTables
	{
		const UDataTable* Attendance = nullptr;
		const UDataTable* ShopItem = nullptr;
		const UDataTable* Resource = nullptr;
	};

	// TableManager::GetResourceInfo(같은 자원은 뒤 행) / GetItemIconResource(첫 매치, 머티리얼 우선)와 같은 조회
	static UObject* ResolveTableIcon(const FRewardTables& Tables, const CGRewardChip::FDisplayEntry& Entry)
	{
		UObject* Found = nullptr;
		if (Entry.ResourceType != EResourceType::None && Tables.Resource)
		{
			Tables.Resource->ForeachRow<FResourceInfo>(TEXT("AttendanceRealFixture"),
				[&Found, &Entry](const FName&, const FResourceInfo& Row)
				{
					if (Row.ResourceType == Entry.ResourceType && !Row.Icon.IsNull())
					{
						Found = Row.Icon.LoadSynchronous();
					}
				});
		}
		else if (Entry.ItemType != EItemType::None && Tables.ShopItem)
		{
			Tables.ShopItem->ForeachRow<FShopItemTable>(TEXT("AttendanceRealFixture"),
				[&Found, &Entry](const FName&, const FShopItemTable& Row)
				{
					if (Found == nullptr && Row.Item == Entry.ItemType)
					{
						Found = Row.ResolveIconResource();
					}
				});
		}
		return Found;
	}

	// 패널 BuildView 와 같은 길(UAttendancePanelWidget::FillDayFromRow)로 28칸을 채운다. 반환 = 행을 찾은 칸 수.
	// 칸은 아이콘을 약참조로 들고 있어 하네스가 강참조를 따로 쥔다(측정 중 GC 대비)
	static int32 FillDaysFromTables(FAttendanceView& View, const FRewardTables& Tables,
		TArray<TStrongObjectPtr<UObject>>& IconGuards)
	{
		TMap<int32, FAttendanceTable> Rows;
		if (Tables.Attendance)
		{
			// TableManager 와 같다 — 같은 일차가 두 번이면 뒤 행이 이긴다
			Tables.Attendance->ForeachRow<FAttendanceTable>(TEXT("AttendanceRealFixture"),
				[&Rows](const FName&, const FAttendanceTable& Row) { Rows.Add(Row.Day, Row); });
		}
		View.Days.SetNum(FAttendanceRules::CycleLength);
		int32 RowsFound = 0;
		for (int32 Index = 0; Index < FAttendanceRules::CycleLength; ++Index)
		{
			const int32 Day = Index + 1;
			FAttendanceDayView& DayView = View.Days[Index];
			DayView.Day = Day;
			DayView.State = View.TileState(Day);
			const FAttendanceTable* Row = Rows.Find(Day);
			if (Row == nullptr)
			{
				continue;
			}
			UAttendancePanelWidget::FillDayFromRow(DayView, *Row,
				[&Tables](const CGRewardChip::FDisplayEntry& Entry) { return ResolveTableIcon(Tables, Entry); });
			for (UObject* Icon : { DayView.PrimaryIcon.Get(), DayView.ExtraIcon.Get() })
			{
				if (Icon)
				{
					IconGuards.Emplace(Icon);
				}
			}
			++RowsFound;
		}
		return RowsFound;
	}

	// 하네스가 격자를 직접 채운다 — 패널의 BuildGridOnce 는 GameInstance 가 없으면 조용히 빠지고,
	// 그렇게 만든 타일은 패널의 private Tiles 에 못 들어가므로 SetDayView 도 여기서 직접 민다. 반환 = 칸 수
	static int32 FillGrid(UUserWidget& Panel, UWorld& EditorWorld, UClass& TileClass, const FAttendanceView& View)
	{
		UUniformGridPanel* Grid = Cast<UUniformGridPanel>(Panel.GetWidgetFromName(TEXT("CalendarGrid")));
		if (Grid == nullptr)
		{
			return 0;
		}
		Grid->ClearChildren();
		for (int32 Index = 0; Index < FAttendanceRules::CycleLength; ++Index)
		{
			UAttendanceDayTileWidget* Tile = CreateWidget<UAttendanceDayTileWidget>(&EditorWorld, &TileClass);
			if (Tile == nullptr)
			{
				continue;
			}
			if (UUniformGridSlot* GridSlot =
				Grid->AddChildToUniformGrid(Tile, Index / GridColumns, Index % GridColumns))
			{
				GridSlot->SetHorizontalAlignment(HAlign_Fill);
				GridSlot->SetVerticalAlignment(VAlign_Fill);
			}
			if (View.Days.IsValidIndex(Index))
			{
				Tile->SetDayView(View.Days[Index]);
			}
		}
		return Grid->GetChildrenCount();
	}

	// 상태표가 켜고 끈 층을 "양방향"으로 판정한다 — 미래 칸에 도장이 남아 있어도 초록이 되던 자리다
	struct FLayerExpectation
	{
		const TCHAR* Widget;
		bool bVisible;
	};

	static TArray<FLayerExpectation> ExpectedLayers(const FAttendanceTileVisibility& V)
	{
		return {
			{ TEXT("TilePlate"), V.bPlateNormal }, { TEXT("TilePlateToday"), V.bPlateToday },
			{ TEXT("TilePlateGold"), V.bPlateGold }, { TEXT("TilePlateDone"), V.bPlateDone },
			{ TEXT("TilePlateGoldDone"), V.bPlateGoldDone },
			{ TEXT("DayBadge"), V.bBadgeDefault }, { TEXT("DayBadgeToday"), V.bBadgeToday },
			{ TEXT("MilestoneBadge"), V.bBadgeMilestone }, { TEXT("MilestoneCrown"), V.bBadgeMilestone },
			{ TEXT("TodayRing"), V.bTodayRing },
			// 감속 설정이면 상태가 켜도 빛띠는 숨는다 (스펙 ⑥-b)
			{ TEXT("TodaySweep"), V.bTodaySweep && !FMotionGate::IsReduced() },
			// 밝은 테는 **입력**이 켜는 층이다 — 상태만으로 켜지면 오늘 칸이 상시 최대 밝기로 굳는다 (R33)
			{ TEXT("TodayHaloFlash"), false },
			{ TEXT("DoneMark"), V.bSeal },
		};
	}

	// 수량 라벨은 RewardCard(중첩 UserWidget) 의 제 트리 안에 있다 — 타일에서 바로 찾으면 못 잡는다
	static FLayoutBox MeasureQuantityLabel(UUserWidget& Tile)
	{
		UUserWidget* Card = Cast<UUserWidget>(Tile.GetWidgetFromName(TEXT("RewardCard")));
		return Card ? Measure(*Card, TEXT("QuantityText"), &Tile) : FLayoutBox();
	}

	// GetAbsolutePosition 은 누적 렌더 트랜스폼을 **이미** 반영한다(회전은 원점 모서리를 옮기고 중심은 그대로).
	// 원점 + R(half,half) 로 되돌리면 회전이 지워지고 "배치된 중심"만 남는다 — 이걸로 도장 중심을 잰다
	static FVector2D PlacedCenter(const FLayoutBox& Box, double AngleDegrees)
	{
		const double HalfW = Box.Width * 0.5, HalfH = Box.Height * 0.5;
		const double CosA = FMath::Cos(FMath::DegreesToRadians(AngleDegrees));
		const double SinA = FMath::Sin(FMath::DegreesToRadians(AngleDegrees));
		return FVector2D(Box.Left + HalfW * CosA - HalfH * SinA, Box.Top + HalfW * SinA + HalfH * CosA);
	}

	// 이동/각도/스케일 수치는 저작이 소유한다 ― 여기서는 배치가 저작값을 따르는지만 본다.
	// 각도를 리터럴로 두면 저작이 기울기를 고쳤을 때 회전 역산만 옛 값에 남아 중심이 조용히 틀어진다
	struct FAuthoredTransform
	{
		bool bFound = false;
		FVector2D Translation = FVector2D::ZeroVector;
		FVector2D Scale = FVector2D(1.0, 1.0);
		double Angle = 0.0;
	};

	static FAuthoredTransform AuthoredTransform(UUserWidget& Root, const TCHAR* Name)
	{
		FAuthoredTransform Out;
		const UWidget* Found = Root.GetWidgetFromName(FName(Name));
		if (Found == nullptr)
		{
			return Out;
		}
		const FWidgetTransform& Authored = Found->GetRenderTransform();
		Out.bFound = true;
		Out.Translation = Authored.Translation;
		Out.Scale = Authored.Scale;
		Out.Angle = Authored.Angle;
		return Out;
	}

	static const TCHAR* VisibilityName(ESlateVisibility Visibility)
	{
		switch (Visibility)
		{
		case ESlateVisibility::Visible:              return TEXT("Visible");
		case ESlateVisibility::Collapsed:            return TEXT("Collapsed");
		case ESlateVisibility::Hidden:               return TEXT("Hidden");
		case ESlateVisibility::HitTestInvisible:     return TEXT("HitTestInvisible");
		case ESlateVisibility::SelfHitTestInvisible: return TEXT("SelfHitTestInvisible");
		default:                                     return TEXT("Unknown");
		}
	}

	// 가시성만 바꾼 뒤 캐시 지오메트리를 다시 얻는 용도 — 픽셀이 필요 없으니 읽어오지 않는다(리드백이 제일 비싸다)
	static void Relayout(const TSharedRef<SWidget>& Widget, FIntPoint Size)
	{
		FWidgetRenderer Renderer(true);
		TStrongObjectPtr<UTextureRenderTarget2D> RenderTarget(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, false));
		if (!RenderTarget.IsValid())
		{
			return;
		}
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			Renderer.DrawWidget(RenderTarget.Get(), Widget, 1.f, FVector2D(Size.X, Size.Y), 0.f);
			FlushRenderingCommands();
		}
	}

	// 반복 장식·v3 가 대체한 줄·v5 아담한 판이 걷은 오른쪽 판(히어로 무대·이름·수량·도장·번쩍임·반짝이)은 트리에 없어야 한다
	static const TCHAR* const RetiredPanelLayers[] = {
		TEXT("AuroraA"), TEXT("AuroraB"), TEXT("AuroraC"), TEXT("HeroSpot"),
		TEXT("ArcRingA"), TEXT("ArcRingB"), TEXT("Halo"), TEXT("HeroBurst"), TEXT("HeroBacklight"),
		TEXT("MilestoneTrail"), TEXT("TrailGlint"),
		TEXT("HeroCardRimOuter"), TEXT("HeroCardRimGold"), TEXT("HeroCardRimInner"),
		TEXT("RewardSoftBacklight"), TEXT("AttendanceLowerGlow"), TEXT("AttendanceCalendarWatermark"),
		TEXT("HeroSpacer"), TEXT("TodayRewardQty"),
		TEXT("CycleLine"), TEXT("CycleLineBox"), TEXT("MilestoneLine"), TEXT("MilestoneLineBox"),
		TEXT("HeroSize"), TEXT("RewardSummarySurface"), TEXT("HeroVBox"), TEXT("EffectStageBox"), TEXT("TodayCardBox"),
		TEXT("HeroCard"), TEXT("HeroNameText"), TEXT("HeroQtyText"), TEXT("HeroSubText"), TEXT("DayHeadText"),
		TEXT("HeroCrown"), TEXT("StampBadge"), TEXT("FlashGlow"), TEXT("SparkleA"), TEXT("SparkleB"), TEXT("SparkleC"),
		TEXT("TodayRewardBox"), TEXT("TodayRewardIcon"), TEXT("StageSparkles"),
		TEXT("NextText"), TEXT("NextMilestoneIcon"), TEXT("HeaderDots"),
		TEXT("MilestoneNodeIcon0"), TEXT("MilestoneNodeIcon1"), TEXT("MilestoneNodeIcon2"), TEXT("MilestoneNodeIcon3"),
		TEXT("MilestoneNodeSocket0"), TEXT("MilestoneNodeSocket1"), TEXT("MilestoneNodeSocket2"), TEXT("MilestoneNodeSocket3")
	};
	// 장식을 정리한 뒤에도 실제 잉크가 남아야 하는 주요 안내 텍스트다(아래 한 줄의 진행·특별 보상 줄).
	static const TCHAR* const PanelLabelNames[] = {
		TEXT("TitleText"), TEXT("ProgressLabel"), TEXT("CycleText"), TEXT("NextMilestoneText") };
	// 글리프가 바탕을 거의 다 덮은 자리만 표본으로 쓴다 — 반투명 AA 가장자리는 뒤 문양이 비쳐도 정상이다.
	// (라벨을 지웠을 때의 색 변화량으로 "덮였다"를 판별한다 ― 잉크 색을 상수로 박지 않는다)
	static constexpr int32 GlyphCoreDelta = 170;
	// 표본이 비면 "변화 0" 이 단락으로 초록이 된다 ― 굵은 하한을 같이 건다
	static constexpr int32 MinGlyphCorePixels = 300;
	static int32 ChannelDelta(const FColor& A, const FColor& B)
	{
		return FMath::Max3(FMath::Abs(static_cast<int32>(A.R) - static_cast<int32>(B.R)),
			FMath::Abs(static_cast<int32>(A.G) - static_cast<int32>(B.G)),
			FMath::Abs(static_cast<int32>(A.B) - static_cast<int32>(B.B)));
	}

	// 수량 글자 잉크 = 라벨을 숨긴 대조 프레임과 크게 다른 픽셀 — 받은 칸은 잉크가 어두워 색 판별이 안 선다.
	// 하한은 받은 칸 글자 몸통보다 낮고 판 위 검은 외곽(30 안팎)보다 높다
	static constexpr int32 LabelInkDelta = 40;
	static FLayoutBox LabelInkBounds(const FImage& WithLabel, const FImage& WithoutLabel, const FLayoutBox& Label,
		int32& OutPixels)
	{
		FLayoutBox Out;
		OutPixels = 0;
		if (!Label.bFound)
		{
			return Out;
		}
		int32 MinX = TNumericLimits<int32>::Max(), MinY = TNumericLimits<int32>::Max();
		int32 MaxX = TNumericLimits<int32>::Lowest(), MaxY = TNumericLimits<int32>::Lowest();
		const int32 Left = FMath::RoundToInt32(Label.Left), Top = FMath::RoundToInt32(Label.Top);
		const int32 Right = FMath::RoundToInt32(Label.Right()), Bottom = FMath::RoundToInt32(Label.Bottom());
		for (int32 Y = Top; Y < Bottom; ++Y)
		{
			for (int32 X = Left; X < Right; ++X)
			{
				const FColor* LabelPixel = PixelAt(WithLabel, X, Y);
				const FColor* BarePixel = PixelAt(WithoutLabel, X, Y);
				if (LabelPixel == nullptr || BarePixel == nullptr || ChannelDelta(*LabelPixel, *BarePixel) < LabelInkDelta)
				{
					continue;
				}
				++OutPixels;
				MinX = FMath::Min(MinX, X); MaxX = FMath::Max(MaxX, X);
				MinY = FMath::Min(MinY, Y); MaxY = FMath::Max(MaxY, Y);
			}
		}
		if (OutPixels == 0)
		{
			return Out;
		}
		Out.bFound = true;
		Out.Left = MinX; Out.Top = MinY;
		Out.Width = MaxX - MinX + 1; Out.Height = MaxY - MinY + 1;
		return Out;
	}

	// 배열 크기를 템플릿으로 받는다 — TArrayView<const TCHAR* const> 은 원소가 이중 const 라 변환이 까다롭다
	template <int32 NumNames>
	static int32 HideWidgets(UUserWidget& Root, const TCHAR* const (&Names)[NumNames])
	{
		int32 Count = 0;
		for (const TCHAR* Name : Names)
		{
			if (UWidget* Found = FindWidgetDeep(&Root, FName(Name)))
			{
				// Collapsed 가 아니라 Hidden 이다 — 배치를 그대로 두어야 같은 좌표로 두 프레임을 비교할 수 있다
				Found->SetVisibility(ESlateVisibility::Hidden);
				++Count;
			}
		}
		return Count;
	}

	// 패널 NativeTick 은 protected 라 SObjectWidget::Tick 으로 민다 — 실제 프레임이 타는 경로와 같다
	static void AdvancePanel(const TSharedRef<SWidget>& PanelSlate, float Seconds)
	{
		const FGeometry Root = FGeometry::MakeRoot(FVector2D(static_cast<double>(MeasureWidth), static_cast<double>(FrameHeight)),
			FSlateLayoutTransform());
		PanelSlate->Tick(Root, 0.0, Seconds);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveOpsAttendanceTileStateTest, "CGR.LiveOps.AttendanceTileState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLiveOpsAttendanceTileStateTest::RunTest(const FString& Parameters)
{
	using namespace LiveOpsAttendance;
	using EState = EAttendanceTileState;

	// v5 상태표 (스펙 §1 ①⑥). 판(Normal/Today/Gold/Done/GoldDone) | 배지(기본/오늘/특별) | 오늘 링·빛띠 | 도장·그림 곱색
	const TArray<TTuple<EState, bool, const TCHAR*>> Cases = {
		{ EState::Future,       false, TEXT("10000|100|00|00") },
		{ EState::Today,        false, TEXT("01000|010|11|00") },
		// 오늘 받은 칸도 받은 칸과 같은 판 — 시안은 받은 칸끼리 구별하지 않는다(잔광 없음, 스펙 §4)
		{ EState::ClaimedToday, false, TEXT("00010|100|00|11") },
		{ EState::Done,         false, TEXT("00010|100|00|11") },
		// 특별 칸: 왕관 배지는 상태 무관 상시, 미수령이면 금 판, 받으면 어두운 금 판
		{ EState::Future,       true,  TEXT("00100|001|00|00") },
		// 오늘 + 특별 = 오늘 판이 이기고 왕관 배지는 남는다
		{ EState::Today,        true,  TEXT("01000|001|11|00") },
		{ EState::ClaimedToday, true,  TEXT("00001|001|00|11") },
		{ EState::Done,         true,  TEXT("00001|001|00|11") },
	};

	for (const TTuple<EState, bool, const TCHAR*>& Case : Cases)
	{
		const FAttendanceTileVisibility Resolved =
			FAttendanceTileVisibility::Resolve(Case.Get<0>(), Case.Get<1>());
		TestEqual(FString::Printf(TEXT("State=%d Milestone=%d"),
			static_cast<int32>(Case.Get<0>()), Case.Get<1>()),
			Signature(Resolved), FString(Case.Get<2>()));
		// 판은 머리띠 색까지 구운 한 장이다 — 두 장이 켜지면 머리띠가 겹쳐 탄다, 0 장이면 칸이 뚫린다
		const int32 Plates = static_cast<int32>(Resolved.bPlateNormal) + static_cast<int32>(Resolved.bPlateToday)
			+ static_cast<int32>(Resolved.bPlateGold) + static_cast<int32>(Resolved.bPlateDone)
			+ static_cast<int32>(Resolved.bPlateGoldDone);
		TestEqual(FString::Printf(TEXT("State=%d Milestone=%d: 판 한 장"),
			static_cast<int32>(Case.Get<0>()), Case.Get<1>()), Plates, 1);
		// 날짜 배지도 정확히 한 벌
		const int32 Badges = static_cast<int32>(Resolved.bBadgeDefault) + static_cast<int32>(Resolved.bBadgeToday)
			+ static_cast<int32>(Resolved.bBadgeMilestone);
		TestEqual(FString::Printf(TEXT("State=%d Milestone=%d: 날짜 한 벌"),
			static_cast<int32>(Case.Get<0>()), Case.Get<1>()), Badges, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveOpsAttendanceTileStampDropTest, "CGR.LiveOps.AttendanceTileStampDrop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLiveOpsAttendanceTileStampDropTest::RunTest(const FString& Parameters)
{
	// 칸 CDO 기본값과 같은 수치 — 곡선 자체의 단위값이라 저작값과 무관하게 고정한다
	constexpr float DropPx = 70.f, ExtraTilt = 22.f;
	auto PoseText = [](const FStampDropPose& Pose)
	{
		return FString::Printf(TEXT("이동 (%.2f, %.2f) 각도 %.2f 불투명도 %.2f"),
			Pose.Offset.X, Pose.Offset.Y, Pose.AngleDeg, Pose.Opacity);
	};
	auto PoseIs = [](const FStampDropPose& Pose, double X, double Y, float Angle, float Opacity)
	{
		return FMath::IsNearlyEqual(Pose.Offset.X, X, 0.01) && FMath::IsNearlyEqual(Pose.Offset.Y, Y, 0.01)
			&& FMath::IsNearlyEqual(Pose.AngleDeg, Angle, 0.01f) && FMath::IsNearlyEqual(Pose.Opacity, Opacity, 0.01f);
	};

	const FStampDropPose Start = FWidgetAnimationUtils::StampDrop(0.f, DropPx, ExtraTilt, false);
	TestTrue(FString::Printf(TEXT("T=0: 위 70 · 22° 더 기움 · 투명 %s"), *PoseText(Start)),
		PoseIs(Start, 0.0, -70.0, -22.f, 0.f));
	const FStampDropPose Landed = FWidgetAnimationUtils::StampDrop(1.f, DropPx, ExtraTilt, false);
	TestTrue(FString::Printf(TEXT("T=1: 제자리 · 0° · 불투명 %s"), *PoseText(Landed)),
		PoseIs(Landed, 0.0, 0.0, 0.f, 1.f));
	const FStampDropPose Reduced = FWidgetAnimationUtils::StampDrop(0.5f, DropPx, ExtraTilt, true);
	TestTrue(FString::Printf(TEXT("감속 T=0.5: 이동·각도 없이 불투명도만 %s"), *PoseText(Reduced)),
		PoseIs(Reduced, 0.0, 0.0, 0.f, 0.5f));
	// back-out — 제자리를 살짝 지나 내려앉는다(크기 대신 위치로 무게를 낸다, 스펙 §0-4)
	const FStampDropPose Overshoot = FWidgetAnimationUtils::StampDrop(0.6f, DropPx, ExtraTilt, false);
	TestTrue(FString::Printf(TEXT("T=0.6: 제자리를 지나친다 %s"), *PoseText(Overshoot)),
		Overshoot.Offset.Y > 0.0 && Overshoot.AngleDeg > 0.f);
	TestTrue(TEXT("T=0.7 부터 불투명"),
		FMath::IsNearlyEqual(FWidgetAnimationUtils::StampDrop(0.7f, DropPx, ExtraTilt, false).Opacity, 1.f, 0.01f));
	const FStampDropPose Past = FWidgetAnimationUtils::StampDrop(1.5f, DropPx, ExtraTilt, false);
	TestTrue(FString::Printf(TEXT("T>1 은 제자리로 묶인다 %s"), *PoseText(Past)), PoseIs(Past, 0.0, 0.0, 0.f, 1.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveOpsAttendanceViewTest, "CGR.LiveOps.AttendanceView",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLiveOpsAttendanceViewTest::RunTest(const FString& Parameters)
{
	using namespace LiveOpsAttendance;
	using EState = EAttendanceTileState;

	// ---- Day12Before — 1..11 Done, 12 Today, 13..28 Future ----
	FAttendanceView Today;
	Today.CycleDay = 11; Today.NextDay = 12; Today.bCanClaim = true; Today.bClaimedToday = false;
	// 특별 칸 일차는 DT 가 든다 — 표시 데이터에 실어야 안내 줄·원판이 답을 뽑는다(사본 픽스처도 이 표를 물려받는다)
	MarkMilestoneDays(Today, { 7, 14, 21, 28 });
	TestEqual(TEXT("오늘 전날"), Today.TileState(11), EState::Done);
	TestEqual(TEXT("오늘"), Today.TileState(12), EState::Today);
	TestEqual(TEXT("내일"), Today.TileState(13), EState::Future);
	TestEqual(TEXT("표시 기준일 = 받을 칸"), Today.ShownDay(), 12);
	TestEqual(TEXT("Day12Before: 원판 (7 받음 · 14 다음)"), NodeStates(Today), FString(TEXT("GNPP")));
	TestEqual(TEXT("Day12Before: 특별 보상까지 2일"), Today.MilestoneLine().ToString(), FString(TEXT("특별 보상까지 2일")));
	TestTrue(TEXT("Day12Before: 완료판 없음"), !Today.ShowsClaimedPlate());

	// ---- Day12Claimed — 12 는 ClaimedToday, 11 은 Done. NextDay 는 매니저가 CycleDay%28+1 = 13 으로 올려 둔다 ----
	FAttendanceView Claimed = Today;
	Claimed.CycleDay = 12; Claimed.NextDay = 13; Claimed.bCanClaim = false; Claimed.bClaimedToday = true;
	TestEqual(TEXT("방금 받은 칸"), Claimed.TileState(12), EState::ClaimedToday);
	TestEqual(TEXT("지난 칸"), Claimed.TileState(11), EState::Done);
	TestEqual(TEXT("표시 기준일 = 방금 받은 칸"), Claimed.ShownDay(), 12);
	// 받은 뒤에도 같은 거리다(스펙 §8.5-4) — 옛 판은 받은 날 이 줄을 접었다
	TestEqual(TEXT("Day12Claimed: 다음 특별 칸까지"), Claimed.DaysToNextMilestone(), 2);
	TestEqual(TEXT("Day12Claimed: 원판"), NodeStates(Claimed), FString(TEXT("GNPP")));
	TestEqual(TEXT("Day12Claimed: 특별 보상까지 2일"), Claimed.MilestoneLine().ToString(), FString(TEXT("특별 보상까지 2일")));
	TestTrue(TEXT("Day12Claimed: 완료판"), Claimed.ShowsClaimedPlate());

	// ---- Day14Claimed — 특별 칸을 받은 날: 14 는 받음, 다음은 21 ----
	FAttendanceView Day14 = Today;
	Day14.CycleDay = 14; Day14.NextDay = 15; Day14.bCanClaim = false; Day14.bClaimedToday = true;
	TestEqual(TEXT("Day14Claimed: 원판 (7·14 받음 · 21 다음)"), NodeStates(Day14), FString(TEXT("GGNP")));
	TestEqual(TEXT("Day14Claimed: 특별 보상까지 7일"), Day14.MilestoneLine().ToString(), FString(TEXT("특별 보상까지 7일")));

	// ---- Day28Claimed — 순환 마지막을 받은 날. 매니저는 NextDay 를 1 로 돌려 둔다 ----
	FAttendanceView Day28 = Today;
	Day28.CycleDay = 28; Day28.NextDay = 1; Day28.bCanClaim = false; Day28.bClaimedToday = true;
	TestFalse(TEXT("Day28Claimed: 받은 날은 새 순환이 아니다"), Day28.IsNewCycle());
	TestEqual(TEXT("Day28Claimed: 28칸 모두 받음"), Day28.ClaimedCount(), FAttendanceRules::CycleLength);
	TestEqual(TEXT("Day28Claimed: 원판 전부 금색"), NodeStates(Day28), FString(TEXT("GGGG")));
	TestNull(TEXT("Day28Claimed: 다음 특별 칸 없음"), Day28.NextMilestoneDay());
	TestEqual(TEXT("Day28Claimed: 특별 보상을 모두 받았습니다"), Day28.MilestoneLine().ToString(),
		FString(TEXT("특별 보상을 모두 받았습니다")));

	// ---- ServerTimeInvalidNoCountdown — 권위 시각이 없으면 완료판(과 그 카운트다운)을 띄우지 않는다 ----
	// 패널은 카운트다운을 ShowsClaimedPlate 하나로 켠다(RefreshAll · 교차 · 끝 프레임) — 실제 시퀀스는 AttendanceServerTimeInvalidNoCountdown 이 민다
	FAttendanceView InvalidAfterClaim = Claimed;
	InvalidAfterClaim.bServerTimeValid = false;
	TestFalse(TEXT("ServerTimeInvalidNoCountdown: 완료판·카운트다운 없음"), InvalidAfterClaim.ShowsClaimedPlate());
	TestEqual(TEXT("ServerTimeInvalidNoCountdown: 방금 받은 칸은 Done 으로 떨어진다"),
		InvalidAfterClaim.TileState(12), EState::Done);

	// 순환 롤오버 — 28 수령 후 다음 날. 2..28 은 Done 이 아니라 Future 로 그린다
	FAttendanceView Rollover;
	Rollover.CycleDay = 28; Rollover.NextDay = 1; Rollover.bCanClaim = true; Rollover.bClaimedToday = false;
	MarkMilestoneDays(Rollover, { 7, 14, 21, 28 });
	TestEqual(TEXT("롤오버 1일차"), Rollover.TileState(1), EState::Today);
	TestEqual(TEXT("롤오버 2일차"), Rollover.TileState(2), EState::Future);
	TestEqual(TEXT("롤오버 28일차"), Rollover.TileState(28), EState::Future);
	TestEqual(TEXT("롤오버: 새 순환 원판"), NodeStates(Rollover), FString(TEXT("NPPP")));
	TestEqual(TEXT("롤오버: 특별 보상까지 6일"), Rollover.MilestoneLine().ToString(), FString(TEXT("특별 보상까지 6일")));

	// 서버 시간 무효 — Today/ClaimedToday 판정이 없다(세이브 기준 Future/Done 만)
	FAttendanceView Invalid = Today;
	Invalid.bServerTimeValid = false; Invalid.bCanClaim = false;
	TestEqual(TEXT("무효: 오늘 칸도 Future"), Invalid.TileState(12), EState::Future);
	TestEqual(TEXT("무효: 지난 칸은 Done 유지"), Invalid.TileState(11), EState::Done);

	// 위 두 줄은 bCanClaim=false 라 게이트를 지워도 통과한다 — 게이트가 일하는지는 아래 셋이 가른다
	FAttendanceView InvalidCanClaim = Today;
	InvalidCanClaim.bServerTimeValid = false;
	TestEqual(TEXT("무효: 받을 수 있어도 오늘 칸이 아니다"), InvalidCanClaim.TileState(12), EState::Future);

	FAttendanceView InvalidClaimed = Claimed;
	InvalidClaimed.bServerTimeValid = false;
	TestEqual(TEXT("무효: 방금 받은 칸도 Done 으로 떨어진다"), InvalidClaimed.TileState(12), EState::Done);

	// 권위 시각이 없으면 순환이 넘어갔는지도 모른다 — 새 순환으로 보면 28칸이 통째로 비어 스펙 §4 가 깨진다
	FAttendanceView InvalidRollover = Rollover;
	InvalidRollover.bServerTimeValid = false;
	TestFalse(TEXT("무효: 새 순환 판정 없음"), InvalidRollover.IsNewCycle());
	TestEqual(TEXT("무효: 28칸은 Done 유지"), InvalidRollover.TileState(28), EState::Done);

	// 오늘 칸이 특별 칸이면 「0일」을 쓰지 않는다 — 오늘 칸(링)이 말하고, 원판은 그 칸을 「다음」으로 든다
	FAttendanceView OnMilestone = Today;
	OnMilestone.NextDay = 7; OnMilestone.CycleDay = 6;
	TestEqual(TEXT("오늘이 특별 칸: 남은 날 0"), OnMilestone.DaysToNextMilestone(), 0);
	TestTrue(TEXT("오늘이 특별 칸: 안내 줄 접힘"), OnMilestone.MilestoneLine().IsEmpty());
	TestEqual(TEXT("오늘이 특별 칸: 원판 7 = 다음"), NodeStates(OnMilestone), FString(TEXT("NPPP")));

	// 표가 7의 배수가 아닌 날을 특별 보상으로 표시하면 안내 줄·원판도 그 날을 센다 —
	// {7,14,21,28} 을 코드에 박아 두면 칸만 금색이 되고 줄은 딴 날을 세는 어긋남이 조용히 난다
	FAttendanceView DataDriven = Today;
	DataDriven.CycleDay = 7; DataDriven.NextDay = 8;
	MarkMilestoneDays(DataDriven, { 10, 20 });
	TestEqual(TEXT("특별 칸 일차는 DT 가 정한다(10일차까지 2일)"), DataDriven.DaysToNextMilestone(), 2);
	TestEqual(TEXT("특별 칸 일차는 DT 가 정한다(원판 둘)"), NodeStates(DataDriven), FString(TEXT("NP")));

	FAttendanceView PastLastMilestone = DataDriven;
	PastLastMilestone.CycleDay = 20; PastLastMilestone.NextDay = 21;
	TestEqual(TEXT("앞으로 특별 칸이 없으면 남은 날 0"), PastLastMilestone.DaysToNextMilestone(), 0);
	TestEqual(TEXT("앞으로 특별 칸이 없으면 모두 받았다"), PastLastMilestone.MilestoneLine().ToString(),
		FString(TEXT("특별 보상을 모두 받았습니다")));

	// 행이 없는 칸(리임포트 누락)은 금색으로 그려지지도 않는다 — 세는 쪽도 같이 건너뛴다
	FAttendanceView MissingRow = DataDriven;
	MissingRow.Days[9].bRowFound = false;
	TestEqual(TEXT("행 없는 칸은 세지 않는다(다음 특별 칸 20)"), MissingRow.DaysToNextMilestone(), 12);

	// 표가 통째로 비면 "모두 받았다"는 거짓이다 — 줄을 접는다
	FAttendanceView NoTable = Today;
	NoTable.Days.Reset();
	TestTrue(TEXT("표 없음: 안내 줄 접힘"), NoTable.MilestoneLine().IsEmpty());

	// ---- 막대 눈금 (스펙 ③) — 원판 중심 = 3 + (안쪽폭 − 34) × 일차/28, 채움 끝 = 같은 눈금 ----
	constexpr double TrackWidth = 904.0;
	const double Inner = TrackWidth - 2.0 * FAttendanceTrackScale::RimPx;   // 898
	TestTrue(FString::Printf(TEXT("원판 7 중심 %.2f (기대 219)"), FAttendanceTrackScale::NodeCenterX(7, TrackWidth)),
		FMath::IsNearlyEqual(FAttendanceTrackScale::NodeCenterX(7, TrackWidth), 219.0, 0.001));
	// 마지막 원판 오른쪽 끝 = 막대 안쪽 끝(901) — 판 밖으로 안 나간다
	const double LastCenter = FAttendanceTrackScale::NodeCenterX(FAttendanceRules::CycleLength, TrackWidth);
	TestTrue(FString::Printf(TEXT("원판 28 중심 %.2f (기대 867) · 오른쪽 끝 %.2f <= 안쪽 끝 %.2f"),
		LastCenter, LastCenter + FAttendanceTrackScale::NodeRadiusPx, FAttendanceTrackScale::RimPx + Inner),
		FMath::IsNearlyEqual(LastCenter, 867.0, 0.001)
		&& LastCenter + FAttendanceTrackScale::NodeRadiusPx <= FAttendanceTrackScale::RimPx + Inner + 0.001);
	TestTrue(TEXT("채움 0일 = 0"), FMath::IsNearlyZero(FAttendanceTrackScale::FillPercent(0, TrackWidth)));
	const float Fill11 = FAttendanceTrackScale::FillPercent(11, TrackWidth);
	TestTrue(FString::Printf(TEXT("채움 11일 %.4f (기대 %.4f)"), Fill11, (864.0 * 11.0 / 28.0) / 898.0),
		FMath::IsNearlyEqual(static_cast<double>(Fill11), (864.0 * 11.0 / 28.0) / 898.0, 0.0001));
	// 채움 끝(테 3 + 안쪽폭 × 비율) = 같은 일차 원판 중심
	const double FillEnd14 = FAttendanceTrackScale::RimPx
		+ Inner * static_cast<double>(FAttendanceTrackScale::FillPercent(14, TrackWidth));
	TestTrue(FString::Printf(TEXT("채움 14일 끝 %.2f = 원판 14 중심 %.2f"), FillEnd14,
		FAttendanceTrackScale::NodeCenterX(14, TrackWidth)),
		FMath::IsNearlyEqual(FillEnd14, FAttendanceTrackScale::NodeCenterX(14, TrackWidth), 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveOpsAttendanceRenderTest, "CGR.LiveOps.AttendanceRender",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLiveOpsAttendanceRenderTest::RunTest(const FString& Parameters)
{
	// 라이브옵스 창은 뒤로가기 처리 대상이라 생성 때 CommonInput 의 뒤로가기 액션을 찾는다 ― 게임 없이 만들면 입력 데이터가 아직 안 읽혀 ensure 가 난다
	ICommonInputModule::GetSettings().LoadData();
	using namespace LiveOpsAttendance;

	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UClass* PanelClass = LoadClass<UAttendancePanelWidget>(nullptr, PanelPath);
	UClass* TileClass = LoadClass<UAttendanceDayTileWidget>(nullptr, TilePath);
	if (EditorWorld == nullptr || PanelClass == nullptr || TileClass == nullptr)
	{
		AddError(TEXT("출석 렌더 하네스를 세울 수 없습니다 ― 에디터 월드 / UI_AttendancePanel / UIE_AttendanceDayTile 을 확인하세요"));
		return false;
	}

	AddInfo(TEXT("에디터 월드에는 GameInstance 가 없어 BuildGridOnce 와 런타임 텍스처 주입이 돌지 않습니다 ― "
		"하네스가 DT_Attendance·DT_ShopItem·DT_Resource 를 직접 읽어 패널과 같은 규칙(FillDayFromRow)으로 28칸을 채우고 "
		"버튼 빛줄기 텍스처를 주입합니다(칸·원판의 그림 층은 WBP 저작 텍스처 그대로). 서버 시간 경고 밴드는 게임 시계를 타므로 "
		"세 프레임 모두 밴드 없는 기하를 봅니다. 색 판정은 PIE 순회가 맡습니다"));

	UDataTable* VFXTable = LoadObject<UDataTable>(nullptr, VFXTablePath);
	TestNotNull(FString::Printf(TEXT("연출 텍스처 DT 로드: %s"), VFXTablePath), VFXTable);
	FRewardTables Tables;
	Tables.Attendance = LoadObject<UDataTable>(nullptr, AttendanceTablePath);
	Tables.ShopItem = LoadObject<UDataTable>(nullptr, ShopItemTablePath);
	Tables.Resource = LoadObject<UDataTable>(nullptr, ResourceTablePath);
	// 세 표를 모두 단언한 뒤에 멈춘다 — 단락 평가면 첫 실패 뒤의 누락이 보고에서 빠진다
	const bool bAttendanceLoaded = TestNotNull(FString::Printf(TEXT("출석 보상표 로드: %s"), AttendanceTablePath), Tables.Attendance);
	const bool bShopItemLoaded = TestNotNull(FString::Printf(TEXT("상점 아이템표 로드: %s"), ShopItemTablePath), Tables.ShopItem);
	const bool bResourceLoaded = TestNotNull(FString::Printf(TEXT("자원표 로드: %s"), ResourceTablePath), Tables.Resource);
	if (!(bAttendanceLoaded && bShopItemLoaded && bResourceLoaded))
	{
		// 실물 픽스처가 없으면 시안 대조(아이콘 겉크기 ②)를 볼 수 없다 — 회색 임시 그림으로 떨어지지 않는다
		return false;
	}
	UObject* DiamondIcon = LoadObject<UObject>(nullptr, DiamondIconPath);
	UObject* PremiumIcon = LoadObject<UObject>(nullptr, PremiumTicketPath);
	// 아이콘 없이 카드가 접히면 원인이 "카드가 안 그려진다"로 읽힌다 — 로드 실패는 제 이름으로 실패시킨다
	TestNotNull(FString::Printf(TEXT("보상 아이콘 로드: %s"), DiamondIconPath), DiamondIcon);
	TestNotNull(FString::Printf(TEXT("마일스톤 아이콘 로드: %s"), PremiumTicketPath), PremiumIcon);

	const FString Directory = RenderDirectory();
	IFileManager::Get().MakeDirectory(*Directory, true);
	// 그린 레벨 뷰포트가 없으면(커맨드릿 -unattended) 머티리얼 면은 원리상 래스터될 수 없다 —
	// 그 문맥에서 빨간불을 내면 원인이 "저작이 틀렸다"로 읽힌다. 시끄럽게 알리고 픽셀 판정만 건너뛴다
	const bool bHasLevelViewport = ForceViewportDraw() > 0;
	if (!bHasLevelViewport)
	{
		AddWarning(TEXT("그려진 레벨 뷰포트가 없습니다(헤드리스 커맨드릿 문맥) ― 씬 텍스처 추출물이 없어 "
			"머티리얼 면이 래스터되지 않으므로 픽셀 판정을 건너뜁니다. 기하·상태 레이어 판정은 그대로 돕니다"));
	}

	// 시안 v5 「아담한 판」의 세 장면 — 기대 원판·안내 줄은 현재 DT_Attendance(특별 칸 7·14·21·28) 기준이다
	struct FFixture
	{
		const TCHAR* Name;
		FAttendanceView View;
		const TCHAR* ExpectNodes;   // 특별 칸 순서대로 G=받음 N=다음 P=남음
		const TCHAR* ExpectLine;    // 빈 문자열 = 줄 접힘
	};
	FAttendanceView Before12; Before12.CycleDay = 11; Before12.NextDay = 12; Before12.bCanClaim = true;
	// 수령 후 매니저는 NextDay 를 CycleDay%28+1 로 올린다 — 프로덕션에 없는 상태를 단언하지 않는다
	FAttendanceView Claimed12; Claimed12.CycleDay = 12; Claimed12.NextDay = 13; Claimed12.bClaimedToday = true;
	FAttendanceView Today28; Today28.CycleDay = 27; Today28.NextDay = 28; Today28.bCanClaim = true;
	FFixture Fixtures[] = {
		{ TEXT("day12_before"), Before12, TEXT("GNPP"), TEXT("특별 보상까지 2일") },
		{ TEXT("day12_claimed"), Claimed12, TEXT("GNPP"), TEXT("특별 보상까지 2일") },
		// 오늘이 특별 칸 — 오늘 칸(링)이 말하므로 안내 줄은 접히고 원판 28 이 「다음」이다
		{ TEXT("day28_today"), Today28, TEXT("GGGN"), TEXT("") } };
	TArray<TStrongObjectPtr<UObject>> IconGuards;
	for (FFixture& Fixture : Fixtures)
	{
		const int32 RowsFound = FillDaysFromTables(Fixture.View, Tables, IconGuards);
		TestEqual(FString::Printf(TEXT("%s: DT_Attendance 28칸 해석"), Fixture.Name), RowsFound, FAttendanceRules::CycleLength);
		for (const FAttendanceDayView& DayView : Fixture.View.Days)
		{
			if (!DayView.bRowFound)
			{
				continue;
			}
			// 실물 픽스처의 목적 = 실제 그림(투명 여백이 제각각인 원본)으로 아이콘 겉크기를 본다 — 하나라도 비면 그 칸은 접힌다
			TestTrue(FString::Printf(TEXT("%s: %d일차 실물 아이콘 %s%s"), Fixture.Name, DayView.Day,
				DayView.PrimaryIcon.IsValid() ? *DayView.PrimaryIcon->GetPathName() : TEXT("없음"),
				DayView.bHasExtra ? (DayView.ExtraIcon.IsValid() ? TEXT(" + 둘째") : TEXT(" + 둘째 없음")) : TEXT("")),
				DayView.PrimaryIcon.IsValid() && (!DayView.bHasExtra || DayView.ExtraIcon.IsValid()));
		}
	}

	// 상태표가 켠 층만 켜져 있는지 **양방향**으로 본다 — 끄는 쪽을 안 보면 미래 칸에 남은 도장이 초록으로 통과한다
	auto AssertTileLayers = [&](UUserWidget& Tile, EAttendanceTileState State, bool bMilestone, const FString& Label)
	{
		const FAttendanceTileVisibility Expect = FAttendanceTileVisibility::Resolve(State, bMilestone);
		for (const FLayerExpectation& Layer : ExpectedLayers(Expect))
		{
			UWidget* Found = Tile.GetWidgetFromName(FName(Layer.Widget));
			if (!TestNotNull(FString::Printf(TEXT("%s: %s 바인딩"), *Label, Layer.Widget), Found))
			{
				continue;
			}
			const bool bVisible = Found->GetVisibility() != ESlateVisibility::Collapsed;
			TestTrue(FString::Printf(TEXT("%s: %s 가시성 %d (기대 %d)"),
				*Label, Layer.Widget, bVisible ? 1 : 0, Layer.bVisible ? 1 : 0),
				bVisible == Layer.bVisible);
		}
		// 받은 칸은 덮개가 아니라 그림 자체에 곱색을 준다(스펙 §8.5-1) — 곱색 값은 칸 CDO 저작값이다
		const FLinearColor* DimTint = ReadWidgetProperty<FLinearColor>(Tile, TEXT("IconDimTint"));
		const UItemCardSlotWidget* Reward = Cast<UItemCardSlotWidget>(Tile.GetWidgetFromName(TEXT("RewardCard")));
		if (TestNotNull(FString::Printf(TEXT("%s: IconDimTint 저작값"), *Label), DimTint)
			&& TestNotNull(FString::Printf(TEXT("%s: RewardCard 바인딩"), *Label), Reward))
		{
			TestFalse(FString::Printf(TEXT("%s: IconDimTint 가 저작됐다(흰색 = 곱해도 그대로)"), *Label),
				DimTint->Equals(FLinearColor::White));
			const FLinearColor IconTint = Reward->GetIconTint();
			TestTrue(FString::Printf(TEXT("%s: 그림 곱색 %s (어둡게 %d)"), *Label, *IconTint.ToString(), Expect.bIconDimmed ? 1 : 0),
				IconTint.Equals(*DimTint) == Expect.bIconDimmed);
		}
	};

	// 켜진 굽기 캔버스 층(판 5벌)마다 저작 텍스처와 칸 사방 12 바깥 상자를 본다 — 상태가 고른 판은 꺼져 있어도 잰다
	auto AssertPlateCanvas = [&](UUserWidget& Tile, EAttendanceTileState State, bool bMilestone, const FString& Label)
	{
		const FVector2D TileSize = FVector2D(Tile.GetCachedGeometry().GetLocalSize());
		const TCHAR* PlateName = VisiblePlateStack(State, bMilestone);
		for (const FAuthoredLayer& Layer : TileAuthoredLayers)
		{
			const UImage* Found = Cast<UImage>(Tile.GetWidgetFromName(FName(Layer.Widget)));
			const bool bChosenPlate = FCString::Strcmp(Layer.Widget, PlateName) == 0;
			if (!Layer.bPlateCanvas
				|| (!bChosenPlate && (Found == nullptr || Found->GetVisibility() == ESlateVisibility::Collapsed)))
			{
				continue;
			}
			TestTrue(FString::Printf(TEXT("%s %s 텍스처 %s (기대 %s)"), *Label, Layer.Widget,
				Found ? *BrushResourceName(Found->GetBrush()) : TEXT("없음"), Layer.Texture),
				Found != nullptr && BrushUsesTexture(Found->GetBrush(), Layer.Texture));
			const FLayoutBox Box = Measure(Tile, Layer.Widget, &Tile);
			TestTrue(FString::Printf(TEXT("%s %s 상자 (%.1f, %.1f) %.1fx%.1f (기대 칸 %.1fx%.1f 사방 %.0f 바깥)"),
				*Label, Layer.Widget, Box.Left, Box.Top, Box.Width, Box.Height, TileSize.X, TileSize.Y, PlateOutset),
				Box.bFound
				&& FMath::Abs(Box.Left + PlateOutset) <= Tolerance && FMath::Abs(Box.Top + PlateOutset) <= Tolerance
				&& FMath::Abs(Box.Width - (TileSize.X + 2.0 * PlateOutset)) <= Tolerance
				&& FMath::Abs(Box.Height - (TileSize.Y + 2.0 * PlateOutset)) <= Tolerance);
		}
	};

	// 프로세스 첫 캡처는 머티리얼 면이 안 잡히는 경우가 있다 — 판정 프레임 전에 한 장 버린다
	if (UAttendancePanelWidget* WarmUp = CreateWidget<UAttendancePanelWidget>(EditorWorld, PanelClass))
	{
		TStrongObjectPtr<UAttendancePanelWidget> WarmUpGuard(WarmUp);
		const TSharedRef<SWidget> WarmUpSlate = WarmUp->TakeWidget();
		InjectAll(*WarmUp, VFXTable, PanelTextures);
		FillGrid(*WarmUp, *EditorWorld, *TileClass, Fixtures[0].View);
		WarmUp->ApplyView(Fixtures[0].View);
		FImage Discard;
		DrawToImage(WarmUpSlate, FIntPoint(MeasureWidth, FrameHeight), Discard);
	}

	TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetNumberField(TEXT("frame_height"), FrameHeight);
	Report->SetNumberField(TEXT("expect_cell_pitch_x"), CellPitchX);
	Report->SetNumberField(TEXT("expect_cell_pitch_y"), CellPitchY);
	Report->SetNumberField(TEXT("expect_tile_w"), TileWidth);
	Report->SetNumberField(TEXT("expect_tile_h"), TileHeight);
	Report->SetNumberField(TEXT("expect_shell_w"), ShellWidth);
	Report->SetNumberField(TEXT("expect_shell_h"), ShellHeight);
	int32 FaceRasterizedFrames = 0;
	TArray<FString> BlankFaceFrames;

	// 면 래스터 가능 여부는 **판정 대상이 아닌 대조군**이 답한다. 판정 대상(플레이트)에게 물으면 플레이트가
	// 죽었을 때 스스로 "이 세션은 면을 못 그린다"고 선언하고 전 픽셀 판정을 건너뛴다 ― 회귀가 경고로 묻힌다.
	// 같은 패스에 대조군 셋을 나란히 그리고 이렇게 가른다:
	//   ① 단색이 비면 캡처 자체가 죽었다(에러) ② 단색만 살면 이 세션이 머티리얼 면을 못 그린다(경고 + 건너뜀)
	//   ③ 트레이는 살고 플레이트 마스터 대조군이 비면 그 마스터가 이 패스에서 안 그려진다 ― 실측상 같은
	//      에디터가 패스마다 뒤집히므로(247 vs 46) 빨간불이 아니라 이름을 박은 경고 + 건너뜀
	//   ④ 플레이트 마스터 대조군이 살았는데 위젯 플레이트만 비면 브러시 리소스 유실 · 가시성 · 알파 튜닝이다(에러)
	const FIntPoint TileFrameSize(FMath::RoundToInt32(TileWidth), FMath::RoundToInt32(TileHeight));
	TStrongObjectPtr<UMaterialInterface> ControlMaterial(
		LoadObject<UMaterialInterface>(nullptr, ControlMaterialPath));
	TStrongObjectPtr<UMaterialInterface> PlateMasterMaterial(
		LoadObject<UMaterialInterface>(nullptr, PlateMasterPath));
	TestNotNull(FString::Printf(TEXT("래스터 대조군 머티리얼 로드: %s"), ControlMaterialPath), ControlMaterial.Get());
	TestNotNull(FString::Printf(TEXT("플레이트 마스터 로드: %s"), PlateMasterPath), PlateMasterMaterial.Get());
	bool bProbeDrawn = false;
	double SolidControlAlpha = 0.0, MaterialControlAlpha = 0.0, PlateMasterAlpha = 0.0, PlateProbeAlpha = 0.0;
	if (UAttendanceDayTileWidget* ProbeTile = CreateWidget<UAttendanceDayTileWidget>(EditorWorld, TileClass))
	{
		TStrongObjectPtr<UAttendanceDayTileWidget> ProbeGuard(ProbeTile);
		FAttendanceDayView ProbeDay;
		ProbeDay.Day = PlainSampleDay;
		ProbeDay.bRowFound = true;
		ProbeDay.State = EAttendanceTileState::Future;
		ProbeDay.PrimaryIcon = DiamondIcon;
		ProbeDay.PrimaryLabel = FText::FromString(TEXT("x10"));
		ProbeTile->SetDayView(ProbeDay);

		FSlateBrush MaterialControlBrush;
		MaterialControlBrush.SetResourceObject(ControlMaterial.Get());
		MaterialControlBrush.SetImageSize(FVector2D(ControlColumnWidth, ControlColumnWidth));
		// 판정 대상의 저작 틴트(알파 0.80)를 따라가지 않는다 — 대조군은 "면이 그려지는가"만 답해야 한다
		FSlateBrush PlateMasterBrush;
		PlateMasterBrush.SetResourceObject(PlateMasterMaterial.Get());
		PlateMasterBrush.SetImageSize(FVector2D(ControlColumnWidth, ControlColumnWidth));
		const FSlateBrush* SolidControlBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
		const TSharedRef<SWidget> ProbeRoot =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(static_cast<float>(ControlColumnWidth))
				.HeightOverride(static_cast<float>(TileFrameSize.Y))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SImage).Image(SolidControlBrush)]
					+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SImage).Image(&MaterialControlBrush)]
					+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SImage).Image(&PlateMasterBrush)]
				]
			]
			// 타일은 칸 실치수로 못 박는다 — AutoWidth 가 desired 를 주면 프로브 좌표가 칸 좌표와 어긋난다
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(static_cast<float>(TileFrameSize.X))
				.HeightOverride(static_cast<float>(TileFrameSize.Y))
				[
					ProbeTile->TakeWidget()
				]
			];

		ForceViewportDraw();
		FImage ProbeImage;
		if (DrawToImage(ProbeRoot, FIntPoint(ControlColumnWidth + TileFrameSize.X, TileFrameSize.Y), ProbeImage))
		{
			bProbeDrawn = true;
			const int32 ControlX = ControlColumnWidth / 2 - ProbeBlock / 2;
			SolidControlAlpha = SampleBlock(ProbeImage, ControlX, TileFrameSize.Y / 6, ProbeBlock).A;
			MaterialControlAlpha = SampleBlock(ProbeImage, ControlX, TileFrameSize.Y / 2, ProbeBlock).A;
			PlateMasterAlpha = SampleBlock(ProbeImage, ControlX, TileFrameSize.Y * 5 / 6, ProbeBlock).A;
			PlateProbeAlpha = SampleBlock(ProbeImage,
				ControlColumnWidth + FMath::RoundToInt32(PlateProbeX),
				FMath::RoundToInt32(PlateProbeY), ProbeBlock).A;
			// 분류 근거를 눈으로도 볼 수 있게 남긴다 — 대조군 기둥 + 판정 타일이 한 장에 있다
			SavePNG(ProbeImage, Directory / TEXT("probe_raster_control.png"));
		}
	}
	const bool bSolidControlDrawn = SolidControlAlpha >= MinFaceAlpha;
	const bool bMaterialControlDrawn = MaterialControlAlpha >= MinFaceAlpha;
	const bool bPlateMasterDrawn = PlateMasterAlpha >= MinFaceAlpha;
	const bool bPlateFacesRasterize = PlateProbeAlpha >= MinFaceAlpha;
	// 픽셀 판정을 돌릴 자격 — 뷰포트(환경)와 대조군 둘(문맥)이 통과해야 한다. 판정 대상은 여기에 없다
	const bool bPixelJudgment = bHasLevelViewport && bProbeDrawn && bMaterialControlDrawn && bPlateMasterDrawn;
	Report->SetBoolField(TEXT("probe_drawn"), bProbeDrawn);
	Report->SetNumberField(TEXT("solid_control_alpha"), SolidControlAlpha);
	Report->SetNumberField(TEXT("material_control_alpha"), MaterialControlAlpha);
	Report->SetNumberField(TEXT("plate_master_control_alpha"), PlateMasterAlpha);
	Report->SetBoolField(TEXT("material_control_rasterizes"), bMaterialControlDrawn);
	Report->SetBoolField(TEXT("plate_master_control_rasterizes"), bPlateMasterDrawn);
	Report->SetBoolField(TEXT("plate_faces_rasterize"), bPlateFacesRasterize);
	Report->SetNumberField(TEXT("plate_probe_alpha"), PlateProbeAlpha);
	Report->SetBoolField(TEXT("pixel_judgment"), bPixelJudgment);
	if (!bProbeDrawn)
	{
		AddError(TEXT("래스터 대조군 프레임을 그리지 못했습니다 ― 픽셀 판정의 기준을 세울 수 없습니다"));
	}
	else if (!bSolidControlDrawn)
	{
		AddError(FString::Printf(
			TEXT("단색 대조군이 비었습니다(알파 %.1f < %.0f) ― 위젯 캡처 자체가 죽었습니다"),
			SolidControlAlpha, MinFaceAlpha));
	}
	else if (!bMaterialControlDrawn)
	{
		AddWarning(FString::Printf(
			TEXT("머티리얼 대조군이 비었습니다(알파 %.1f < %.0f, 단색 %.1f) ― 씬 텍스처 추출물이 없는 "
				 "문맥입니다. 픽셀 판정을 건너뛰고 기하·상태 레이어만 봅니다"),
			MaterialControlAlpha, MinFaceAlpha, SolidControlAlpha));
	}
	else if (!bPlateMasterDrawn)
	{
		AddWarning(FString::Printf(
			TEXT("%s 면이 이 패스에서 안 그려졌습니다(알파 %.1f < %.0f, 다른 머티리얼 대조군 %.1f) ― "
				 "이 마스터의 셰이더 문맥 문제이거나 컴파일 실패입니다. 픽셀 판정을 건너뜁니다"),
			PlateMasterPath, PlateMasterAlpha, MinFaceAlpha, MaterialControlAlpha));
	}
	else if (!bPlateFacesRasterize)
	{
		AddError(FString::Printf(
			TEXT("플레이트 면만 비었습니다(플레이트 알파 %.1f < %.0f, 같은 마스터 대조군 %.1f) ― 환경이 아니라 "
				 "회귀입니다. 브러시 리소스 유실 · 레이어 가시성 · 틴트 알파 튜닝을 보세요"),
			PlateProbeAlpha, MinFaceAlpha, PlateMasterAlpha));
	}

	// 라벨을 숨긴 대조 프레임은 기준 폭에서 한 번만 그린다.
	bool bLabelInkMeasured = false;
	for (const FFixture& Fixture : Fixtures)
	{
		for (int32 FrameWidth : FrameWidths)
		{
			const FString Tag = FString::Printf(TEXT("%s@%d"), Fixture.Name, FrameWidth);
			UAttendancePanelWidget* Panel = CreateWidget<UAttendancePanelWidget>(EditorWorld, PanelClass);
			if (Panel == nullptr)
			{
				AddError(TEXT("UI_AttendancePanel 인스턴스 생성 실패"));
				return false;
			}
			TStrongObjectPtr<UAttendancePanelWidget> PanelGuard(Panel);
			const TSharedRef<SWidget> PanelSlate = Panel->TakeWidget();

			const int32 Injected = InjectAll(*Panel, VFXTable, PanelTextures);
			const int32 FilledTiles = FillGrid(*Panel, *EditorWorld, *TileClass, Fixture.View);
			Panel->ApplyView(Fixture.View);

			// 머티리얼 면은 직전 씬 렌더의 추출물이 있어야 그려진다 — 캡처마다 새로 그린다
			ForceViewportDraw();
			FImage Frame;
			if (!DrawToImage(PanelSlate, FIntPoint(FrameWidth, FrameHeight), Frame))
			{
				AddError(FString::Printf(TEXT("렌더 실패: %s"), *Tag));
				continue;
			}
			const FString FileName = FString::Printf(TEXT("attendance_%s_%d.png"), Fixture.Name, FrameWidth);
			TestTrue(FString::Printf(TEXT("%s: PNG 저장 %s"), *Tag, *FileName), SavePNG(Frame, Directory / FileName));

			TestEqual(FString::Printf(TEXT("%s: 버튼 빛줄기 텍스처 주입"), *Tag),
				Injected, static_cast<int32>(UE_ARRAY_COUNT(PanelTextures)));
			TestEqual(FString::Printf(TEXT("%s: 격자 칸 수"), *Tag), FilledTiles, FAttendanceRules::CycleLength);

			// 판 축소(PanelScale)는 FrameStack 렌더 트랜스폼이다 — 패널 상자는 그 로컬(시안 치수)에서 재야 위치와 크기가 같은 공간이 된다
			const UWidget* FrameStack = Panel->GetWidgetFromName(TEXT("FrameStack"));
			if (TestNotNull(FString::Printf(TEXT("%s: FrameStack 바인딩"), *Tag), FrameStack))
			{
				const FWidgetTransform& StackTransform = FrameStack->GetRenderTransform();
				const FVector2D StackPivot = FrameStack->GetRenderTransformPivot();
				TestTrue(FString::Printf(TEXT("%s: 판 축소 %.3fx%.3f · 피벗 (%.2f, %.2f) (기대 %.2f · 0.50)"),
					*Tag, StackTransform.Scale.X, StackTransform.Scale.Y, StackPivot.X, StackPivot.Y, PanelScale),
					FMath::IsNearlyEqual(StackTransform.Scale.X, PanelScale, 0.001)
					&& FMath::IsNearlyEqual(StackTransform.Scale.Y, PanelScale, 0.001)
					&& FMath::IsNearlyEqual(StackPivot.X, 0.5, 0.001) && FMath::IsNearlyEqual(StackPivot.Y, 0.5, 0.001));
			}
			// 셸 = 틀 묶음 자신(출석 셸 굽기 + 공용 창 틀) — 등장 대상(AppearSlideTarget)은 배율 밖 부모라 기준이 아니다
			const FLayoutBox Shell = Measure(*Panel, TEXT("FrameStack"), FrameStack);
			const FLayoutBox Grid = Measure(*Panel, TEXT("GridSize"), FrameStack);
			const FLayoutBox BottomRow = Measure(*Panel, TEXT("BottomRow"), FrameStack);
			const FLayoutBox Claim = Measure(*Panel, TEXT("ClaimRowBox"), FrameStack);
			const FLayoutBox Bar = Measure(*Panel, TEXT("CycleBar"), FrameStack);
			const FLayoutBox Rings = Measure(*Panel, TEXT("AttRings"), FrameStack);

			// 바인딩이 사라지면 단락으로 초록이 되지 않게 "존재"부터 단언한다
			auto AssertFound = [&](const TCHAR* Name, const FLayoutBox& Box)
			{
				return TestTrue(FString::Printf(TEXT("%s: %s 지오메트리 측정"), *Tag, Name), Box.bFound);
			};
			const bool bShellFound = AssertFound(TEXT("FrameStack"), Shell);
			const bool bGridFound = AssertFound(TEXT("GridSize"), Grid);
			const bool bRowFound = AssertFound(TEXT("BottomRow"), BottomRow);
			const bool bClaimFound = AssertFound(TEXT("ClaimRowBox"), Claim);
			const bool bBarFound = AssertFound(TEXT("CycleBar"), Bar);
			const bool bRingsFound = AssertFound(TEXT("AttRings"), Rings);
			if (!(bShellFound && bGridFound && bRowFound && bClaimFound && bBarFound && bRingsFound))
			{
				continue;
			}

			auto AssertBox = [&](const TCHAR* Name, const FLayoutBox& Box, double Left, double Top, double Width, double Height)
			{
				TestTrue(FString::Printf(TEXT("%s: %s (%.1f, %.1f) %.1fx%.1f (기대 (%.1f, %.1f) %.1fx%.1f)"),
					*Tag, Name, Box.Left, Box.Top, Box.Width, Box.Height, Left, Top, Width, Height),
					FMath::Abs(Box.Left - Left) <= BoxTolerance && FMath::Abs(Box.Top - Top) <= BoxTolerance
					&& FMath::Abs(Box.Width - Width) <= BoxTolerance && FMath::Abs(Box.Height - Height) <= BoxTolerance);
			};
			AssertBox(TEXT("셸"), Shell, 0.0, 0.0, ShellWidth, ShellHeight);
			AssertBox(TEXT("격자"), Grid, GridLeft, GridTop, GridWidth, GridHeight);
			// 아래 한 줄은 격자 아래 24 — 격자가 자라면 줄이 판 아래로 밀린다
			AssertBox(TEXT("아래 한 줄"), BottomRow, GridLeft, Grid.Bottom() + BottomRowGap, BottomRowWidth, BottomRowHeight);
			AssertBox(TEXT("받기 줄"), Claim, BottomRow.Left + CtaLeft, BottomRow.Top + CtaTop, CtaWidth, CtaHeight);
			AssertBox(TEXT("진행 막대"), Bar, BottomRow.Left + BarLeft, BottomRow.Top + BarTop, BarWidth, BarHeight);
			// 원판 눈금은 CDO 막대 폭으로 잡는다 — 저작 막대와 다르면 원판·채움 끝이 막대와 어긋난다
			const float* TrackWidth = ReadWidgetProperty<float>(*Panel, TEXT("ProgressTrackWidth"));
			const double TrackWidthValue = TrackWidth ? static_cast<double>(*TrackWidth) : BarWidth;
			if (TestNotNull(FString::Printf(TEXT("%s: ProgressTrackWidth 저작값"), *Tag), TrackWidth))
			{
				TestTrue(FString::Printf(TEXT("%s: 막대 실측 폭 %.1f = CDO ProgressTrackWidth %.1f"), *Tag, Bar.Width, TrackWidthValue),
					FMath::Abs(Bar.Width - TrackWidthValue) <= BoxTolerance);
			}
			// 고리 8개 한 장 — 판 위로 51 튀어나온다(검은 테 포함). 판 폭과 같은 캔버스라 고리 x 는 굽기 그대로다
			AssertBox(TEXT("달력 고리"), Rings, 0.0, RingsCanvasTop, ShellWidth, RingsCanvasHeight);
			const FLayoutBox RingsPixels = MeasurePixels(*Panel, TEXT("AttRings"));
			TestTrue(FString::Printf(TEXT("%s: 고리 윗끝 화면 y %.1f >= 0"), *Tag, RingsPixels.Top),
				RingsPixels.bFound && RingsPixels.Top >= 0.0);

			// 오버플로 0 — 본문 요소가 셸 밖으로 나가면 여기서 잡힌다
			auto AssertInside = [&](const TCHAR* Name, const FLayoutBox& Box, const FLayoutBox& Container, const TCHAR* ContainerName)
			{
				TestTrue(FString::Printf(
					TEXT("%s: %s 가 %s 안 (좌 %.0f>=%.0f · 상 %.0f>=%.0f · 우 %.0f<=%.0f · 하 %.0f<=%.0f)"),
					*Tag, Name, ContainerName, Box.Left, Container.Left, Box.Top, Container.Top,
					Box.Right(), Container.Right(), Box.Bottom(), Container.Bottom()),
					Box.bFound && Box.Left >= Container.Left - BoxTolerance && Box.Top >= Container.Top - BoxTolerance
					&& Box.Right() <= Container.Right() + BoxTolerance && Box.Bottom() <= Container.Bottom() + BoxTolerance);
			};
			AssertInside(TEXT("GridSize"), Grid, Shell, TEXT("셸"));
			AssertInside(TEXT("BottomRow"), BottomRow, Shell, TEXT("셸"));
			AssertInside(TEXT("ClaimRowBox"), Claim, BottomRow, TEXT("아래 줄"));
			AssertInside(TEXT("CycleBar"), Bar, BottomRow, TEXT("아래 줄"));
			const FLayoutBox ProgressLabelBox = Measure(*Panel, TEXT("ProgressLabel"), FrameStack);
			const FLayoutBox CycleValueBox = Measure(*Panel, TEXT("CycleText"), FrameStack);
			const FLayoutBox GoldLineBox = Measure(*Panel, TEXT("NextMilestoneText"), FrameStack);
			AssertInside(TEXT("ProgressLabel"), ProgressLabelBox, BottomRow, TEXT("아래 줄"));
			AssertInside(TEXT("CycleText"), CycleValueBox, BottomRow, TEXT("아래 줄"));

			// 스펙 §4: 서버 시간이 무효면 CTA 를 비활성으로 알린다. 스타일 에셋은 골드 그대로이고
			// 회색 판은 그 스타일의 Disabled 브러시가 낸다(Pre-flight R13) ― "골드를 유지한다"가 아니다
			const bool bExpectPlate = Fixture.View.ShowsClaimedPlate();
			UCommonButtonBase* ClaimCta = Cast<UCommonButtonBase>(Panel->GetWidgetFromName(TEXT("ClaimButton")));
			if (TestNotNull(FString::Printf(TEXT("%s: ClaimButton 바인딩"), *Tag), ClaimCta))
			{
				TestTrue(FString::Printf(TEXT("%s: CTA 활성 %d (수령 가능 %d)"),
					*Tag, ClaimCta->GetIsEnabled() ? 1 : 0, Fixture.View.bCanClaim ? 1 : 0),
					ClaimCta->GetIsEnabled() == Fixture.View.bCanClaim);
				TestTrue(FString::Printf(TEXT("%s: CTA 가시성 %s (완료판 %d)"),
					*Tag, VisibilityName(ClaimCta->GetVisibility()), bExpectPlate ? 1 : 0),
					(ClaimCta->GetVisibility() == ESlateVisibility::Collapsed) == bExpectPlate);
				const UButtonWidget* ClaimLabelOwner = Cast<UButtonWidget>(ClaimCta);
				const float ClaimLabelSize = ClaimLabelOwner ? ClaimLabelOwner->GetTextSize() : -1.f;
				TestTrue(FString::Printf(TEXT("%s: CTA 글자 크기 %.1f (기대 %.1f)"), *Tag, ClaimLabelSize, ClaimTextSize),
					FMath::IsNearlyEqual(ClaimLabelSize, static_cast<float>(ClaimTextSize), 0.01f));
				const UCommonButtonStyle* CtaStyle = ClaimCta->GetStyle();
				if (TestNotNull(FString::Printf(TEXT("%s: CTA 스타일"), *Tag), CtaStyle))
				{
					FSlateBrush NormalBase, DisabledBrush;
					CtaStyle->GetNormalBaseBrush(NormalBase);
					CtaStyle->GetDisabledBrush(DisabledBrush);
					// 비활성 판이 평상 판과 같으면 "무효"가 화면에서 아무것도 말하지 않는다
					TestTrue(FString::Printf(TEXT("%s: 비활성 브러시가 평상 브러시와 다르다"), *Tag),
						NormalBase.GetResourceObject() != DisabledBrush.GetResourceObject()
						|| !NormalBase.TintColor.GetSpecifiedColor().Equals(
							DisabledBrush.TintColor.GetSpecifiedColor(), 0.01f));
				}
			}
			UWidget* ClaimedBoard = Panel->GetWidgetFromName(TEXT("ClaimedPlate"));
			if (TestNotNull(FString::Printf(TEXT("%s: ClaimedPlate 바인딩"), *Tag), ClaimedBoard))
			{
				const bool bClaimedVisible = ClaimedBoard->GetVisibility() != ESlateVisibility::Collapsed;
				TestTrue(FString::Printf(TEXT("%s: 완료판 가시성 %s (기대 %d)"),
					*Tag, VisibilityName(ClaimedBoard->GetVisibility()), bExpectPlate ? 1 : 0),
					bClaimedVisible == bExpectPlate);
			}
			// 완료판 두 줄 — 첫 줄은 C++ 이 「출석 완료」, 둘째 줄은 카운트다운(하네스는 타이머를 안 돌려 글자만 저작값)
			const UCommonTextBlock* ClaimedTitle = Cast<UCommonTextBlock>(Panel->GetWidgetFromName(TEXT("ClaimedTitle")));
			if (TestNotNull(FString::Printf(TEXT("%s: ClaimedTitle 바인딩"), *Tag), ClaimedTitle))
			{
				TestEqual(FString::Printf(TEXT("%s: 완료판 첫 줄"), *Tag), ClaimedTitle->GetText().ToString(), FString(TEXT("출석 완료")));
			}
			TestNotNull(FString::Printf(TEXT("%s: ClaimedCountdown 바인딩"), *Tag), Panel->GetWidgetFromName(TEXT("ClaimedCountdown")));

			// 아래 한 줄 문구 (설계 v5 §2·§11) — 진행 값 · 특별 보상 안내가 표시값을 그대로 싣는다
			const UCommonTextBlock* CycleLabel = Cast<UCommonTextBlock>(Panel->GetWidgetFromName(TEXT("CycleText")));
			if (TestNotNull(FString::Printf(TEXT("%s: CycleText 바인딩"), *Tag), CycleLabel))
			{
				TestEqual(FString::Printf(TEXT("%s: 출석 진행 값"), *Tag), CycleLabel->GetText().ToString(),
					FString::Printf(TEXT("%d / %d일"), Fixture.View.ClaimedCount(), FAttendanceRules::CycleLength));
			}
			const UProgressBar* CycleBar = Cast<UProgressBar>(Panel->GetWidgetFromName(TEXT("CycleBar")));
			if (TestNotNull(FString::Printf(TEXT("%s: CycleBar 바인딩"), *Tag), CycleBar))
			{
				const float ExpectFill = FAttendanceTrackScale::FillPercent(Fixture.View.ClaimedCount(), TrackWidthValue);
				TestTrue(FString::Printf(TEXT("%s: 막대 채움 %.4f (기대 %.4f = 원판 눈금)"), *Tag, CycleBar->GetPercent(), ExpectFill),
					FMath::IsNearlyEqual(CycleBar->GetPercent(), ExpectFill, 0.0001f));
			}
			const UCommonTextBlock* MilestoneLabel =
				Cast<UCommonTextBlock>(Panel->GetWidgetFromName(TEXT("NextMilestoneText")));
			if (TestNotNull(FString::Printf(TEXT("%s: NextMilestoneText 바인딩"), *Tag), MilestoneLabel))
			{
				const FString ExpectLine(Fixture.ExpectLine);
				const bool bLineVisible = MilestoneLabel->GetVisibility() != ESlateVisibility::Collapsed;
				TestTrue(FString::Printf(TEXT("%s: 특별 보상 안내 가시성 %d (기대 %d)"),
					*Tag, bLineVisible ? 1 : 0, ExpectLine.IsEmpty() ? 0 : 1),
					bLineVisible == !ExpectLine.IsEmpty());
				if (!ExpectLine.IsEmpty())
				{
					TestEqual(FString::Printf(TEXT("%s: 특별 보상 안내 문구"), *Tag), MilestoneLabel->GetText().ToString(), ExpectLine);
					AssertInside(TEXT("NextMilestoneText"), GoldLineBox, BottomRow, TEXT("아래 줄"));
					// 금색 줄은 진행 칸 오른쪽 끝(= 막대 오른쪽 끝 948)에 맞춘다 (스펙 §11-2)
					TestTrue(FString::Printf(TEXT("%s: 특별 보상 안내 오른쪽 끝 %.1f = 막대 오른쪽 끝 %.1f"),
						*Tag, GoldLineBox.Right(), Bar.Right()),
						FMath::Abs(GoldLineBox.Right() - Bar.Right()) <= BoxTolerance);
				}
			}

			TSharedPtr<FJsonObject> FrameJson = MakeShared<FJsonObject>();
			// 프레임 상자는 FrameStack 로컬(축소 전 시안 치수)이다(tile_* 는 칸 로컬) ― 두 공간을 섞어 읽는 사고를 막는다
			FrameJson->SetStringField(TEXT("space"), TEXT("frame_stack"));
			FrameJson->SetStringField(TEXT("png"), FileName);
			FrameJson->SetObjectField(TEXT("shell"), BoxJson(Shell));
			FrameJson->SetObjectField(TEXT("grid"), BoxJson(Grid));
			TSharedPtr<FJsonObject> Rects = MakeShared<FJsonObject>();
			SetNamedRect(Rects, TEXT("bottom_row"), BottomRow, TEXT("BottomRow"));
			SetNamedRect(Rects, TEXT("p_lab"), ProgressLabelBox, TEXT("ProgressLabel"));
			SetNamedRect(Rects, TEXT("p_val"), CycleValueBox, TEXT("CycleText"));
			SetNamedRect(Rects, TEXT("s_gold"), GoldLineBox, TEXT("NextMilestoneText"));
			SetNamedRect(Rects, TEXT("bar"), Bar, TEXT("CycleBar"));
			SetNamedRect(Rects, TEXT("cta"), Measure(*Panel, TEXT("ClaimButton"), FrameStack), TEXT("ClaimButton"));
			SetNamedRect(Rects, TEXT("claimed_plate"), Measure(*Panel, TEXT("ClaimedPlate"), FrameStack), TEXT("ClaimedPlate"));
			for (int32 RingIndex = 0; RingIndex < static_cast<int32>(UE_ARRAY_COUNT(RingX)); ++RingIndex)
			{
				FLayoutBox Ring;
				Ring.bFound = Rings.bFound;
				Ring.Left = Rings.Left + RingX[RingIndex];
				Ring.Top = Rings.Top + RingBodyTop;
				Ring.Width = RingWidth;
				Ring.Height = RingHeight;
				SetNamedRect(Rects, *FString::Printf(TEXT("ring%d"), RingIndex), Ring,
					TEXT("AttRings 상자 + 굽기 고리 x (T_UI_AttV5_Rings)"));
			}

			// ---- 특별 보상 원판 4개 (스펙 ③ · §5) — 상태 3벌 · 라벨 · 막대 눈금 위치 ----
			FString NodeSignature;
			int32 NodeIndex = 0;
			for (const FAttendanceDayView& Entry : Fixture.View.Days)
			{
				if (!Entry.bRowFound || !Entry.bMilestone)
				{
					continue;
				}
				const FString NodeName = FString::Printf(TEXT("MilestoneNode%d"), NodeIndex);
				++NodeIndex;
				UAttendanceMilestoneNodeWidget* Node =
					Cast<UAttendanceMilestoneNodeWidget>(Panel->GetWidgetFromName(FName(*NodeName)));
				if (!TestNotNull(FString::Printf(TEXT("%s: %s (UIE_AttendanceMilestoneNode)"), *Tag, *NodeName), Node))
				{
					continue;
				}
				const EAttendanceNodeState State = Fixture.View.MilestoneNodeState(Entry.Day);
				NodeSignature.AppendChar(State == EAttendanceNodeState::Got ? TEXT('G')
					: State == EAttendanceNodeState::Next ? TEXT('N') : TEXT('P'));
				const FString NodeTag = FString::Printf(TEXT("%s: 원판 %d일"), *Tag, Entry.Day);
				TestTrue(NodeTag + TEXT(" 표시"), Node->GetVisibility() != ESlateVisibility::Collapsed);

				const TPair<const TCHAR*, bool> NodeLayers[] = {
					{ TEXT("PlateGot"), State == EAttendanceNodeState::Got },
					{ TEXT("PlateNext"), State == EAttendanceNodeState::Next },
					{ TEXT("PlatePending"), State == EAttendanceNodeState::Pending },
					{ TEXT("GiftColor"), State != EAttendanceNodeState::Pending },
					{ TEXT("GiftGray"), State == EAttendanceNodeState::Pending } };
				for (const TPair<const TCHAR*, bool>& NodeLayer : NodeLayers)
				{
					const UWidget* LayerWidget = Node->GetWidgetFromName(FName(NodeLayer.Key));
					const bool bLayerVisible = LayerWidget && LayerWidget->GetVisibility() != ESlateVisibility::Collapsed;
					TestTrue(FString::Printf(TEXT("%s %s 가시성 %d (기대 %d)"), *NodeTag, NodeLayer.Key,
						bLayerVisible ? 1 : 0, NodeLayer.Value ? 1 : 0),
						LayerWidget != nullptr && bLayerVisible == NodeLayer.Value);
				}
				for (const TPair<const TCHAR*, const TCHAR*>& Authored : NodeAuthoredLayers)
				{
					const UImage* LayerImage = Cast<UImage>(Node->GetWidgetFromName(FName(Authored.Key)));
					TestTrue(FString::Printf(TEXT("%s %s 저작 텍스처 %s (기대 %s)"), *NodeTag, Authored.Key,
						LayerImage ? *BrushResourceName(LayerImage->GetBrush()) : TEXT("없음"), Authored.Value),
						LayerImage != nullptr && BrushUsesTexture(LayerImage->GetBrush(), Authored.Value));
				}
				const UCommonTextBlock* DayLabel = Cast<UCommonTextBlock>(Node->GetWidgetFromName(TEXT("DayLabel")));
				const FSlateColor* ExpectInk = ReadWidgetProperty<FSlateColor>(*Node,
					State == EAttendanceNodeState::Got ? TEXT("LabelInkGot") : TEXT("LabelInkOther"));
				if (TestNotNull(NodeTag + TEXT(" DayLabel"), DayLabel) && TestNotNull(NodeTag + TEXT(" 라벨 잉크 저작값"), ExpectInk))
				{
					TestEqual(NodeTag + TEXT(" 라벨"), DayLabel->GetText().ToString(), FString::Printf(TEXT("%d일"), Entry.Day));
					TestTrue(NodeTag + TEXT(" 라벨 잉크 = 상태 잉크"), DayLabel->GetColorAndOpacity() == *ExpectInk);
				}
				// 원판 중심 = 막대 왼쪽 + 3 + (안쪽폭 − 34) × 일차/28, 세로 = 막대 중심 (시안 .ms top −20 · 68)
				const FLayoutBox Plate = Measure(*Node, NodePlateName(State), FrameStack);
				const double ExpectX = Bar.Left + FAttendanceTrackScale::NodeCenterX(Entry.Day, TrackWidthValue);
				TestTrue(FString::Printf(TEXT("%s 중심 (%.1f, %.1f) (기대 %.1f, %.1f)"), *NodeTag,
					Plate.CenterX(), Plate.CenterY(), ExpectX, Bar.CenterY()),
					Plate.bFound && FMath::Abs(Plate.CenterX() - ExpectX) <= Tolerance
					&& FMath::Abs(Plate.CenterY() - Bar.CenterY()) <= Tolerance);
				AssertInside(*NodeName, Plate, Shell, TEXT("셸"));
				SetNamedRect(Rects, *FString::Printf(TEXT("node%d"), Entry.Day),
					BoxAround(Plate.CenterX(), Plate.CenterY(), NodePlateSize, NodePlateSize, Plate.bFound),
					FString::Printf(TEXT("%s.%s 중심 ± %.0f"), *NodeName, NodePlateName(State), NodePlateSize * 0.5));
			}
			TestEqual(FString::Printf(TEXT("%s: 원판 상태 (G 받음 · N 다음 · P 남음)"), *Tag), NodeSignature, FString(Fixture.ExpectNodes));

			UUniformGridPanel* GridPanel = Cast<UUniformGridPanel>(Panel->GetWidgetFromName(TEXT("CalendarGrid")));
			const bool bGridReady = GridPanel != nullptr && GridPanel->GetChildrenCount() > GridColumns;

			// 시안 대조용 칸 사각형 — 오늘 칸(받기 전·후)과 도장이 있는 칸, 칸 로컬 좌표
			if (bGridReady)
			{
				const int32 FocusIndex = FMath::Clamp(Fixture.View.ShownDay() - 1, 0, GridPanel->GetChildrenCount() - 1);
				const FAttendanceDayView& FocusDay = Fixture.View.Days[FocusIndex];
				UUserWidget* FocusTile = Cast<UUserWidget>(GridPanel->GetChildAt(FocusIndex));
				const bool bFocusSeal = FAttendanceTileVisibility::Resolve(FocusDay.State, FocusDay.bMilestone).bSeal;
				// 받기 전 프레임은 오늘 칸에 도장이 없다 — 첫 칸(세 프레임 모두 받은 칸)의 도장을 잰다
				const int32 SealIndex = bFocusSeal ? FocusIndex : 0;
				UUserWidget* SealTile = Cast<UUserWidget>(GridPanel->GetChildAt(SealIndex));
				if (FocusTile && SealTile)
				{
					FrameJson->SetNumberField(TEXT("tile_day"), FocusDay.Day);
					FrameJson->SetNumberField(TEXT("tile_seal_day"), SealIndex + 1);
					const TCHAR* BadgeName = VisibleBadge(FocusDay.State, FocusDay.bMilestone);
					SetNamedRect(Rects, TEXT("tile_strip"), Measure(*FocusTile, BadgeName, FocusTile), BadgeName);
					SetNamedRect(Rects, TEXT("tile_iconbox"), Measure(*FocusTile, TEXT("RewardCardBox"), FocusTile), TEXT("RewardCardBox"));
					SetNamedRect(Rects, TEXT("tile_qty"), MeasureQuantityLabel(*FocusTile), TEXT("RewardCard.QuantityText"));
					// 도장 사각형 = 굽기 캔버스(160) 안의 실제 도장(132) — 시안 .seal 과 같은 것을 잰다
					const FLayoutBox SealCanvasBox = Measure(*SealTile, TEXT("DoneMark"), SealTile);
					const FVector2D SealCenter = PlacedCenter(SealCanvasBox, AuthoredTransform(*SealTile, TEXT("DoneMark")).Angle);
					const double InkSize = SealCanvasBox.Width * SealInkDiameter / SealCanvas;
					SetNamedRect(Rects, TEXT("tile_seal"), BoxAround(SealCenter.X, SealCenter.Y, InkSize, InkSize, SealCanvasBox.bFound),
						TEXT("DoneMark 중심 · 캔버스 160 중 도장 132"));
				}
			}
			FrameJson->SetObjectField(TEXT("rects"), Rects);

			// ---- 픽셀 판정: 웰(근흑 면) 위에 플레이트 면이 실제로 얹혔는가 -------------
			// 세 폭 전부에서 잰다 — 한 폭만 래스터되면 나머지 두 장은 빈 PNG 인데 초록으로 남는다
			if (bGridReady && bPixelJudgment)
			{
				// 받은 칸은 한 단 어두운 판이라 밝기 차가 줄어든다 → 받지 않은 칸을 찾는다.
				// 오늘 칸은 마지막 수단이다: 링 번짐이 면 위에 얹혀 플레이트가 안 그려져도 밝아진다
				int32 ProbeIndex = INDEX_NONE, TodayFallbackIndex = INDEX_NONE;
				for (int32 Index = 0; Index < Fixture.View.Days.Num() && Index < GridPanel->GetChildrenCount(); ++Index)
				{
					const FAttendanceDayView& Probe = Fixture.View.Days[Index];
					if (FAttendanceTileVisibility::Resolve(Probe.State, Probe.bMilestone).bIconDimmed)
					{
						continue;
					}
					if (Probe.State == EAttendanceTileState::Today)
					{
						TodayFallbackIndex = TodayFallbackIndex == INDEX_NONE ? Index : TodayFallbackIndex;
						continue;
					}
					ProbeIndex = Index;
					break;
				}
				ProbeIndex = ProbeIndex != INDEX_NONE ? ProbeIndex : TodayFallbackIndex;
				const bool bLitProbe = ProbeIndex != INDEX_NONE;
				const int32 SampleIndex = bLitProbe ? ProbeIndex : 0;
				const double RequiredDelta = bLitProbe ? MinPlateWellLumaDelta : MinDimmedPlateWellLumaDelta;

				const UWidget* GridBox = FindWidgetDeep(Panel, FName(TEXT("GridSize")));
				const FIntPoint WellPixel = GridBox
					? PixelOf(*GridBox, ProbeMargin * 0.5, Grid.Height * 0.5) : FIntPoint::ZeroValue;
				const FPixelSample WellSample = SampleBlock(Frame, WellPixel.X, WellPixel.Y, 4);
				const FIntPoint PlatePixel = PixelOf(*GridPanel->GetChildAt(SampleIndex), PlateProbeX, PlateProbeY);
				const FPixelSample PlateSample = SampleBlock(Frame, PlatePixel.X, PlatePixel.Y, ProbeBlock);
				const bool bFaceRasterized = PlateSample.Luma() >= WellSample.Luma() + RequiredDelta;
				FaceRasterizedFrames += bFaceRasterized ? 1 : 0;

				FrameJson->SetNumberField(TEXT("probe_tile_index"), SampleIndex);
				FrameJson->SetBoolField(TEXT("probe_tile_lit"), bLitProbe);
				FrameJson->SetBoolField(TEXT("face_rasterized"), bFaceRasterized);
				FrameJson->SetNumberField(TEXT("plate_luma"), PlateSample.Luma());
				FrameJson->SetNumberField(TEXT("plate_alpha"), PlateSample.A);
				FrameJson->SetNumberField(TEXT("well_luma"), WellSample.Luma());

				// 판정은 프레임마다가 아니라 **일치**로 한다(아래 집계). 머티리얼 면은 씬 텍스처 추출물이
				// 있어야 그려지므로 "전부 안 그려짐"은 환경이고, "일부만 안 그려짐"은 그 프레임의 회귀다
				if (!bFaceRasterized)
				{
					BlankFaceFrames.Add(FString::Printf(TEXT("%s(%.1f/%.1f)"),
						*Tag, PlateSample.Luma(), WellSample.Luma()));
				}
			}

			// 셀 피치 = 이웃 칸 원점 차(FrameStack 로컬 — 절대 좌표 차는 PanelScale 배로 줄어 있다). 타일 실치수는 그 안에서 슬롯 패딩 12 를 뺀 값
			if (bGridReady && FrameWidth == MeasureWidth)
			{
				const FVector2D FirstOrigin = OriginIn(*GridPanel->GetChildAt(0), FrameStack);
				const double PitchX = OriginIn(*GridPanel->GetChildAt(1), FrameStack).X - FirstOrigin.X;
				const double PitchY = OriginIn(*GridPanel->GetChildAt(GridColumns), FrameStack).Y - FirstOrigin.Y;
				const FVector2D TileSize = FVector2D(GridPanel->GetChildAt(0)->GetCachedGeometry().GetLocalSize());
				TestTrue(FString::Printf(TEXT("%s: 셀 피치 %.2fx%.2f (기대 %.2fx%.2f)"),
					*Tag, PitchX, PitchY, CellPitchX, CellPitchY),
					FMath::Abs(PitchX - CellPitchX) <= Tolerance && FMath::Abs(PitchY - CellPitchY) <= Tolerance);
				TestTrue(FString::Printf(TEXT("%s: 타일 %.0fx%.0f (기대 %.0fx%.0f)"),
					*Tag, TileSize.X, TileSize.Y, TileWidth, TileHeight),
					FMath::Abs(TileSize.X - TileWidth) <= Tolerance
					&& FMath::Abs(TileSize.Y - TileHeight) <= Tolerance);
				FrameJson->SetNumberField(TEXT("cell_pitch_x"), PitchX);
				FrameJson->SetNumberField(TEXT("cell_pitch_y"), PitchY);
				FrameJson->SetNumberField(TEXT("tile_w"), TileSize.X);
				FrameJson->SetNumberField(TEXT("tile_h"), TileSize.Y);

				// 격자에서도 층을 판정한다 — 첫 칸(세 픽스처 모두 Done)과 "오늘/방금 받은" 칸을 각각 본다.
				// 첫 칸만 보면 링·빛띠가 켜지는 쪽을 한 번도 확인하지 않는다
				for (int32 Index = 0; Index < Fixture.View.Days.Num() && Index < GridPanel->GetChildrenCount(); ++Index)
				{
					const FAttendanceDayView& DayView = Fixture.View.Days[Index];
					const UUniformGridSlot* CellSlot = Cast<UUniformGridSlot>(GridPanel->GetChildAt(Index)->Slot);
					if (TestNotNull(TEXT("출석 격자 슬롯"), CellSlot))
					{
						TestEqual(TEXT("출석 행"), CellSlot->GetRow(), Index / GridColumns);
						TestEqual(TEXT("출석 열"), CellSlot->GetColumn(), Index % GridColumns);
					}
					const bool bFocusTile = DayView.State == EAttendanceTileState::Today
						|| DayView.State == EAttendanceTileState::ClaimedToday;
					if (Index != 0 && !bFocusTile)
					{
						continue;
					}
					if (UUserWidget* GridTile = Cast<UUserWidget>(GridPanel->GetChildAt(Index)))
					{
						AssertTileLayers(*GridTile, DayView.State, DayView.bMilestone,
							FString::Printf(TEXT("%s 격자 %d일차"), *Tag, DayView.Day));
						AssertPlateCanvas(*GridTile, DayView.State, DayView.bMilestone,
							FString::Printf(TEXT("%s 격자 %d일차"), *Tag, DayView.Day));
					}
				}

				UAttendanceDayTileWidget* Sample = Cast<UAttendanceDayTileWidget>(GridPanel->GetChildAt(0));
				if (TestNotNull(FString::Printf(TEXT("%s: 격자 첫 칸 타입"), *Tag), Sample))
				{
					const FAttendanceDayView& FirstDay = Fixture.View.Days[0];
					const FVector2D ExpectCard = ExpectedCardSize(FirstDay.bHasExtra);
					const FLayoutBox StripText = StripContent(*Sample, FirstDay.State, FirstDay.bMilestone);
					const FLayoutBox RewardCard = Measure(*Sample, TEXT("RewardCardBox"), Sample);
					const FLayoutBox Seal = Measure(*Sample, TEXT("DoneMark"), Sample);
					const bool bStripFound = AssertFound(VisibleBadgeLabel(FirstDay.State, FirstDay.bMilestone), StripText);
					const bool bRewardFound = AssertFound(TEXT("RewardCardBox"), RewardCard);
					const bool bSealFound = AssertFound(TEXT("DoneMark"), Seal);
					if (bStripFound)
					{
						TestTrue(FString::Printf(TEXT("%s: 첫 칸 머리띠 글자 중심 (%.1f, %.1f) (기대 %.1f, %.1f)"),
							*Tag, StripText.CenterX(), StripText.CenterY(), TileWidth * 0.5, StripHeight * 0.5),
							FMath::Abs(StripText.CenterX() - TileWidth * 0.5) <= Tolerance
							&& FMath::Abs(StripText.CenterY() - StripHeight * 0.5) <= Tolerance);
					}
					if (bRewardFound)
					{
						TestTrue(FString::Printf(TEXT("%s: 보상 카드 %.0fx%.0f (기대 %.0fx%.0f)"),
							*Tag, RewardCard.Width, RewardCard.Height, ExpectCard.X, ExpectCard.Y),
							FMath::Abs(RewardCard.Width - ExpectCard.X) <= Tolerance
							&& FMath::Abs(RewardCard.Height - ExpectCard.Y) <= Tolerance);
					}
					// 첫 칸은 세 픽스처 모두 Done 이라 도장이 떠 있다 — 눌린 상자 사고의 런타임 가드
					if (bSealFound)
					{
						TestTrue(FString::Printf(TEXT("%s: 도장 정사각 %.0fx%.0f · 크기 (기대 %.0f)"), *Tag, Seal.Width, Seal.Height, SealCanvas),
							FMath::Abs(Seal.Width - Seal.Height) <= 1.0 && FMath::Abs(Seal.Width - SealCanvas) <= Tolerance);
					}
					FrameJson->SetStringField(TEXT("first_tile_space"), TEXT("tile"));
					FrameJson->SetObjectField(TEXT("first_tile_strip_text"), BoxJson(StripText));
					FrameJson->SetObjectField(TEXT("first_tile_reward_card"), BoxJson(RewardCard));
					FrameJson->SetObjectField(TEXT("first_tile_done_mark"), BoxJson(Seal));
					FrameJson->SetObjectField(TEXT("first_tile_qty_label"), BoxJson(MeasureQuantityLabel(*Sample)));
				}
			}

			for (const TCHAR* Name : RetiredPanelLayers)
			{
				const UWidget* Retired = Panel->GetWidgetFromName(Name);
				TestTrue(FString::Printf(TEXT("%s: 걷은 층 %s 제거"), *Tag, Name),
					!Retired || !Retired->GetParent());
			}
			for (const TPair<FString, int32>& Group : TArray<TPair<FString, int32>>{
				TPair<FString, int32>(TEXT("Ribbon"), 7), {TEXT("StageDust"), 14},
				{TEXT("StageSparkle"), 8}, {TEXT("TrailNode"), 4}, {TEXT("TrailMote"), 2}})
			{
				for (int32 Index = 1; Index <= Group.Value; ++Index)
				{
					const FString Name = FString::Printf(TEXT("%s%02d"), *Group.Key, Index);
					const UWidget* Retired = Panel->GetWidgetFromName(FName(*Name));
					TestTrue(Tag + TEXT(" 반복 장식 제거 ") + Name, !Retired || !Retired->GetParent());
				}
			}
			// 장식 제거 후에도 실제 라벨 잉크가 그려져야 한다. 빈 프레임과 비교해 공허한 통과를 막는다.
			// 첫 픽스처(12일 받기 전)는 특별 보상 줄이 보이는 프레임이라 네 라벨이 모두 잉크를 낸다
			if (!bLabelInkMeasured && FrameWidth == MeasureWidth && bPixelJudgment)
			{
				bLabelInkMeasured = true;
				TArray<FLayoutBox> LabelBoxes;
				for (const TCHAR* Name : PanelLabelNames)
				{
					const FLayoutBox Box = MeasurePixels(*Panel, Name);
					TestTrue(Tag + FString::Printf(TEXT(" 라벨 %s 바인딩"), Name), Box.bFound);
					if (Box.bFound) LabelBoxes.Add(Box);
				}
				HideWidgets(*Panel, PanelLabelNames);
				Relayout(PanelSlate, FIntPoint(FrameWidth, FrameHeight));
				ForceViewportDraw();
				FImage Blank;
				if (TestTrue(Tag + TEXT(" 라벨 대조 프레임"), DrawToImage(PanelSlate, FIntPoint(FrameWidth, FrameHeight), Blank)))
				{
					int32 CorePixels = 0;
					for (const FLayoutBox& Box : LabelBoxes)
					{
						for (int32 Y = FMath::RoundToInt32(Box.Top); Y < FMath::RoundToInt32(Box.Bottom()); ++Y)
						{
							for (int32 X = FMath::RoundToInt32(Box.Left); X < FMath::RoundToInt32(Box.Right()); ++X)
							{
								const FColor* WithLabel = PixelAt(Frame, X, Y);
								const FColor* WithoutLabel = PixelAt(Blank, X, Y);
								if (WithLabel && WithoutLabel && ChannelDelta(*WithLabel, *WithoutLabel) >= GlyphCoreDelta) ++CorePixels;
							}
						}
					}
					TestTrue(Tag + TEXT(" 라벨 글리프 코어 존재"), CorePixels >= MinGlyphCorePixels);
					FrameJson->SetNumberField(TEXT("label_glyph_core"), CorePixels);
				}
			}
			Report->SetObjectField(Tag, FrameJson);
		}
	}

	// ---- 타일 10장 (상태 4 × 마일스톤 2) + 보상 하나인 마일스톤 2 --------------------
	struct FTileFixture
	{
		const TCHAR* Name;
		EAttendanceTileState State;
		bool bMilestone;
		bool bHasExtra;
	};
	const FTileFixture TileFixtures[] = {
		{ TEXT("future"), EAttendanceTileState::Future, false, false },
		{ TEXT("today"), EAttendanceTileState::Today, false, false },
		{ TEXT("claimed"), EAttendanceTileState::ClaimedToday, false, false },
		{ TEXT("done"), EAttendanceTileState::Done, false, false },
		{ TEXT("future_milestone"), EAttendanceTileState::Future, true, true },
		{ TEXT("today_milestone"), EAttendanceTileState::Today, true, true },
		{ TEXT("claimed_milestone"), EAttendanceTileState::ClaimedToday, true, true },
		{ TEXT("done_milestone"), EAttendanceTileState::Done, true, true },
		// 7/14/21 일차 모양 — 마일스톤인데 둘째 보상이 없다. 보상 수에 따라 카드 폭이 달라진다
		{ TEXT("today_milestone_nochip"), EAttendanceTileState::Today, true, false },
		// 같은 7/14/21 모양의 수령 후 — 보상 하나인 마일스톤에서도 도장/수량 판정을 밟는다
		{ TEXT("done_milestone_nochip"), EAttendanceTileState::Done, true, false },
	};
	TSharedPtr<FJsonObject> TileReport = MakeShared<FJsonObject>();
	// 보상 수가 수량 줄 자리를 흔들면 안 된다(C6) — 픽스처 사이에서 잉크 띠를 대조한다
	TMap<FString, FVector2D> QtyInkRows;

	for (const FTileFixture& Fixture : TileFixtures)
	{
		UAttendanceDayTileWidget* Tile = CreateWidget<UAttendanceDayTileWidget>(EditorWorld, TileClass);
		if (Tile == nullptr)
		{
			AddError(TEXT("UIE_AttendanceDayTile 인스턴스 생성 실패"));
			break;
		}
		TStrongObjectPtr<UAttendanceDayTileWidget> TileGuard(Tile);
		const TSharedRef<SWidget> TileSlate = Tile->TakeWidget();
		for (const FAuthoredLayer& Layer : TileAuthoredLayers)
		{
			const UImage* Authored = Cast<UImage>(Tile->GetWidgetFromName(FName(Layer.Widget)));
			TestTrue(FString::Printf(TEXT("타일 %s: %s 저작 텍스처 %s (기대 %s)"), Fixture.Name, Layer.Widget,
				Authored ? *BrushResourceName(Authored->GetBrush()) : TEXT("없음"), Layer.Texture),
				Authored != nullptr && BrushUsesTexture(Authored->GetBrush(), Layer.Texture));
		}
		for (const TCHAR* Name : RetiredTileLayers)
		{
			const UWidget* Retired = Tile->GetWidgetFromName(FName(Name));
			TestTrue(FString::Printf(TEXT("타일 %s: 철거된 층 %s 없음"), Fixture.Name, Name),
				Retired == nullptr || Retired->GetParent() == nullptr);
		}

		FAttendanceDayView DayView;
		// 보상 둘인 마일스톤은 28일차(둘째 보상이 있는 유일한 칸), 하나인 쪽은 7일차 모양으로 세운다
		DayView.Day = Fixture.bMilestone
			? (Fixture.bHasExtra ? FAttendanceRules::CycleLength : MilestoneStride) : PlainSampleDay;
		DayView.bRowFound = true;
		DayView.bMilestone = Fixture.bMilestone;
		DayView.State = Fixture.State;
		DayView.PrimaryIcon = Fixture.bMilestone ? PremiumIcon : DiamondIcon;
		DayView.PrimaryLabel = FText::FromString(Fixture.bMilestone ? TEXT("x1") : TEXT("x10"));
		DayView.bHasExtra = Fixture.bHasExtra;
		DayView.ExtraIcon = DiamondIcon;
		DayView.ExtraLabel = FText::FromString(TEXT("x100"));
		Tile->SetDayView(DayView);
		UItemCardSlotWidget* Reward = Cast<UItemCardSlotWidget>(Tile->GetWidgetFromName(TEXT("RewardCard")));
		if (TestNotNull(TEXT("출석 보상 슬롯"), Reward))
		{
			TestEqual(TEXT("출석 전용 보상 슬롯 클래스"), Reward->GetClass()->GetPathName(),
				FString(TEXT("/Game/CompanyGrowth/UI/Elements/LiveOps/UIE_AttendanceReward.UIE_AttendanceReward_C")));
			const UCommonTextBlock* Quantity = Cast<UCommonTextBlock>(Reward->GetWidgetFromName(TEXT("QuantityText")));
			if (TestNotNull(TEXT("보상 수량 라벨"), Quantity))
			{
				TestEqual(TEXT("실제 보상 수량 전달"), Quantity->GetText().ToString(), DayView.PrimaryLabel.ToString());
			}
			const UItemCardWidget* RewardBody = Reward->GetCardWidget();
			if (TestNotNull(TEXT("출석 보상 본체"), RewardBody))
			{
				TestEqual(TEXT("출석 전용 보상 본체 클래스"), RewardBody->GetClass()->GetPathName(),
					FString(TEXT("/Game/CompanyGrowth/UI/Elements/LiveOps/UIE_AttendanceRewardBody.UIE_AttendanceRewardBody_C")));
				const UImage* RewardIcon = Cast<UImage>(RewardBody->GetWidgetFromName(TEXT("EntityImage")));
				if (TestNotNull(TEXT("실제 보상 아이콘"), RewardIcon))
				{
					TestTrue(TEXT("아이콘 데이터 전달"), RewardIcon->GetBrush().GetResourceObject() == DayView.PrimaryIcon.Get());
				}
				const UWidget* Shadow = RewardBody->GetWidgetFromName(TEXT("Shadow_Img"));
				TestTrue(TEXT("반복 보상 카드 그림자 제거"), Shadow && Shadow->GetVisibility() == ESlateVisibility::Collapsed);
				// 공용 CardClear 의 최소 100×100 은 28일차 카드(73 폭)를 붙든다 — 출석 전용 사본은 최소가 0 이어야 한다
				const UCommonButtonStyle* CardStyle = RewardBody->GetStyle();
				TestTrue(TEXT("보상 카드 투명 스타일(출석 전용·최소 0)"), CardStyle && CardStyle->GetClass()->GetPathName()
					== TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/CUI_Style2_Btn_CardClear_Attendance.CUI_Style2_Btn_CardClear_Attendance_C")
					&& CardStyle->MinWidth == 0 && CardStyle->MinHeight == 0);
			}
		}
		// 둘째 수량도 첫 수량과 같은 글자다 — 두 수량이 한 줄로 나란히 읽힌다
		const UCommonTextBlock* QuantityLabels[] = {
			Reward ? Cast<UCommonTextBlock>(Reward->GetWidgetFromName(TEXT("QuantityText"))) : nullptr,
			Cast<UCommonTextBlock>(Tile->GetWidgetFromName(TEXT("ExtraRewardQty"))) };
		for (const UCommonTextBlock* QuantityLabel : QuantityLabels)
		{
			if (QuantityLabel == nullptr)
			{
				continue;
			}
			const FSlateFontInfo& QuantityFont = QuantityLabel->GetFont();
			TestTrue(FString::Printf(TEXT("%s 수량 타이포 %s %.1f · 외곽 %d (기대 Bold %.1f · %d)"),
				*QuantityLabel->GetName(), *QuantityFont.TypefaceFontName.ToString(), QuantityFont.Size,
				QuantityFont.OutlineSettings.OutlineSize, CardLabelFontSize, CardLabelOutline),
				QuantityFont.TypefaceFontName == FName(TEXT("Bold"))
				&& FMath::IsNearlyEqual(QuantityFont.Size, static_cast<float>(CardLabelFontSize))
				&& QuantityFont.OutlineSettings.OutlineSize == CardLabelOutline);
		}
		// 날짜는 머리띠 한 줄 「12일」(한 글꼴 한 크기, 스펙 ⑥), 오늘 칸은 「오늘」
		const FString DayLabel = FString::Printf(TEXT("%d일"), DayView.Day);
		const TPair<const TCHAR*, FString> DateLabels[] = {
			{ TEXT("DayText"), DayLabel }, { TEXT("MilestoneDayText"), DayLabel }, { TEXT("DayTodayText"), FString(TEXT("오늘")) } };
		for (const TPair<const TCHAR*, FString>& DateLabel : DateLabels)
		{
			const UCommonTextBlock* Date = Cast<UCommonTextBlock>(Tile->GetWidgetFromName(DateLabel.Key));
			if (TestNotNull(FString(DateLabel.Key) + TEXT(" 날짜 라벨"), Date))
			{
				TestEqual(TEXT("날짜 타이포"), Date->GetFont().Size, DateFontSize);
				TestEqual(FString::Printf(TEXT("%s 날짜 데이터 전달"), DateLabel.Key),
					Date->GetText().ToString(), DateLabel.Value);
			}
		}
		// 상태 잉크 = 칸 CDO 저작값을 상태로 고른 것. 받은 칸은 머리띠가 어두워 글자·수량도 함께 내려간다
		const bool bClaimedFixture = FAttendanceTileVisibility::Resolve(Fixture.State, Fixture.bMilestone).bIconDimmed;
		const TPair<const TCHAR*, const TCHAR*> DateInks[] = {
			{ TEXT("DayText"), bClaimedFixture ? TEXT("DayInkDone") : TEXT("DayInkNormal") },
			{ TEXT("MilestoneDayText"), bClaimedFixture ? TEXT("DayInkMilestoneDone") : TEXT("DayInkMilestone") },
			{ TEXT("DayTodayText"), TEXT("DayInkToday") } };
		for (const TPair<const TCHAR*, const TCHAR*>& DateInk : DateInks)
		{
			const UCommonTextBlock* Date = Cast<UCommonTextBlock>(Tile->GetWidgetFromName(DateInk.Key));
			const FSlateColor* Ink = ReadWidgetProperty<FSlateColor>(*Tile, DateInk.Value);
			if (Date && TestNotNull(FString::Printf(TEXT("타일 %s: %s 저작값"), Fixture.Name, DateInk.Value), Ink))
			{
				TestTrue(FString::Printf(TEXT("타일 %s: %s 잉크 = %s"), Fixture.Name, DateInk.Key, DateInk.Value),
					Date->GetColorAndOpacity() == *Ink);
			}
		}
		const FSlateColor* DoneQtyInk = ReadWidgetProperty<FSlateColor>(*Tile, TEXT("QtyInkDone"));
		if (Reward && TestNotNull(FString::Printf(TEXT("타일 %s: QtyInkDone 저작값"), Fixture.Name), DoneQtyInk))
		{
			TestTrue(FString::Printf(TEXT("타일 %s: 수량 잉크가 받은 칸 색 %d"), Fixture.Name, bClaimedFixture ? 1 : 0),
				(Reward->GetLabelColor() == *DoneQtyInk) == bClaimedFixture);
		}

		AssertTileLayers(*Tile, Fixture.State, Fixture.bMilestone,
			FString::Printf(TEXT("타일 %s"), Fixture.Name));
		// 둘째 보상 줄은 상태가 아니라 데이터가 정한다 — 상태표 밖이라 따로 본다
		if (UWidget* ExtraReward = Tile->GetWidgetFromName(TEXT("ExtraRewardBox")))
		{
			const bool bExtraVisible = ExtraReward->GetVisibility() != ESlateVisibility::Collapsed;
			TestTrue(FString::Printf(TEXT("타일 %s: 둘째 보상 줄 가시성 %d (둘째 보상 %d)"),
				Fixture.Name, bExtraVisible ? 1 : 0, Fixture.bHasExtra ? 1 : 0),
				bExtraVisible == Fixture.bHasExtra);
		}

		ForceViewportDraw();
		FImage TileImage;
		if (!DrawToImage(TileSlate, TileFrameSize, TileImage))
		{
			AddError(FString::Printf(TEXT("타일 렌더 실패: %s"), Fixture.Name));
			continue;
		}
		const FString TileFile = FString::Printf(TEXT("tile_%s.png"), Fixture.Name);
		TestTrue(FString::Printf(TEXT("타일 PNG 저장 %s"), *TileFile), SavePNG(TileImage, Directory / TileFile));

		AssertPlateCanvas(*Tile, Fixture.State, Fixture.bMilestone,
			FString::Printf(TEXT("타일 %s"), Fixture.Name));
		// 같은 마스터 대조군이 그려지는 패스에서만 실제 판의 불투명 면을 판정한다.
		double PlateLuma = -1.0;
		if (bPixelJudgment)
		{
			const FPixelSample SurfaceSample = SampleBlock(TileImage,
				FMath::RoundToInt32(PlateProbeX), FMath::RoundToInt32(PlateProbeY), ProbeBlock);
			PlateLuma = SurfaceSample.Luma();
			TestTrue(FString::Printf(TEXT("타일 %s: 판 면 알파 %.1f"), Fixture.Name, SurfaceSample.A),
				SurfaceSample.A >= MinFaceAlpha);
		}

		// 단독 타일은 칸 실치수(반올림)로 그린다 — 가로 중앙 판정은 실제로 배치된 폭을 기준으로 한다
		const double LocalTileWidth = static_cast<double>(TileFrameSize.X);
		const double LocalTileHeight = static_cast<double>(TileFrameSize.Y);
		const FVector2D ExpectCard = ExpectedCardSize(Fixture.bHasExtra);
		const TCHAR* BadgeName = VisibleBadge(Fixture.State, Fixture.bMilestone);
		const FLayoutBox Badge = Measure(*Tile, BadgeName);
		const FLayoutBox StripText = StripContent(*Tile, Fixture.State, Fixture.bMilestone);
		const FLayoutBox RewardCard = Measure(*Tile, TEXT("RewardCardBox"));
		const FLayoutBox QtyLabel = MeasureQuantityLabel(*Tile);
		const FLayoutBox Seal = Measure(*Tile, TEXT("DoneMark"));
		const FLayoutBox ExtraIconBox = Measure(*Tile, TEXT("ExtraRewardIconBox"));
		const FLayoutBox ExtraQty = Measure(*Tile, TEXT("ExtraRewardQty"));
		// 각도·스케일도 저작에서 읽는다 — 회전 역산이 저작과 어긋나면 "배치된 중심"이 조용히 틀어진다
		const FAuthoredTransform SealTransform = AuthoredTransform(*Tile, TEXT("DoneMark"));
		const FVector2D SealCenter = PlacedCenter(Seal, SealTransform.Angle);
		const double CardCenterY = RewardCard.CenterY();
		// 수량 잉크 띠 — 라인박스 파생값은 기록/대조용이고 판독선은 아래 실측 잉크가 낸다
		const double QtyInkTop = QtyLabel.Top + (QtyLabel.Height - CardLabelFontSize) * 0.5;
		const double QtyInkBottom = QtyInkTop + CardLabelFontSize;
		TestTrue(FString::Printf(TEXT("타일 %s: %s 측정"), Fixture.Name, BadgeName), Badge.bFound);
		TestTrue(FString::Printf(TEXT("타일 %s: RewardCardBox 측정"), Fixture.Name), RewardCard.bFound);
		// 머리띠 글자 = 칸 가운데 한 줄(+왕관). 상자가 아니라 글자 자체를 잰다 — 상자가 상한에 걸리면 글자만 밖으로 샌다 (R31)
		if (TestTrue(FString::Printf(TEXT("타일 %s: 머리띠 글자 %s 측정"), Fixture.Name,
			VisibleBadgeLabel(Fixture.State, Fixture.bMilestone)), StripText.bFound && StripText.Width > 0.0))
		{
			TestTrue(FString::Printf(TEXT("타일 %s: 머리띠 글자 중심 (%.1f, %.1f) (기대 %.1f, %.1f)"),
				Fixture.Name, StripText.CenterX(), StripText.CenterY(), LocalTileWidth * 0.5, StripHeight * 0.5),
				FMath::Abs(StripText.CenterX() - LocalTileWidth * 0.5) <= Tolerance
				&& FMath::Abs(StripText.CenterY() - StripHeight * 0.5) <= Tolerance);
			TestTrue(FString::Printf(TEXT("타일 %s: 머리띠 글자 %.1f..%.1f 가 칸 안(여백 %.0f) · 머리띠 %.0f 안 (%.1f..%.1f)"),
				Fixture.Name, StripText.Left, StripText.Right(), StripTextMinMargin, StripHeight, StripText.Top, StripText.Bottom()),
				StripText.Left >= StripTextMinMargin && StripText.Right() <= LocalTileWidth - StripTextMinMargin
				&& StripText.Top >= -Tolerance && StripText.Bottom() <= StripHeight + Tolerance);
		}
		// 탭 플래시는 구운 오늘 링 띠(칸 밖)에 두르는 흰 테다 — 접혀 있어 상자는 못 재고 브러시 두께·슬롯 패딩을 본다
		const UImage* HaloFlash = Cast<UImage>(Tile->GetWidgetFromName(TEXT("TodayHaloFlash")));
		if (TestNotNull(FString::Printf(TEXT("타일 %s: 탭 플래시"), Fixture.Name), HaloFlash))
		{
			const UOverlaySlot* FlashSlot = Cast<UOverlaySlot>(HaloFlash->Slot);
			const FMargin FlashPad = FlashSlot ? FlashSlot->GetPadding() : FMargin();
			TestTrue(FString::Printf(TEXT("타일 %s: 탭 플래시 테 두께 %.1f · 슬롯 패딩 %.1f (기대 %.0f · -%.0f)"),
				Fixture.Name, HaloFlash->GetBrush().OutlineSettings.Width, FlashPad.Left, HaloFlashThickness, RingInkOutset),
				FMath::IsNearlyEqual(HaloFlash->GetBrush().OutlineSettings.Width, static_cast<float>(HaloFlashThickness), 0.01f)
				&& FlashSlot != nullptr && FlashPad == FMargin(-static_cast<float>(RingInkOutset)));
		}

		if (RewardCard.bFound)
		{
			// 보상 수와 무관하게 카드 중심이 아이콘 상자 중심에 앉는다 — 수량 줄 기준선이 칸마다 같아지는 근거다
			TestTrue(FString::Printf(TEXT("타일 %s: 아이콘 상자 중심 y %.1f (기대 %.1f)"),
				Fixture.Name, CardCenterY, IconCenterY),
				FMath::Abs(CardCenterY - IconCenterY) <= Tolerance);
			TestTrue(FString::Printf(TEXT("타일 %s: 보상 카드 %.1fx%.1f (기대 %.1fx%.0f)"),
				Fixture.Name, RewardCard.Width, RewardCard.Height, ExpectCard.X, ExpectCard.Y),
				FMath::Abs(RewardCard.Width - ExpectCard.X) <= Tolerance
				&& FMath::Abs(RewardCard.Height - ExpectCard.Y) <= Tolerance);
			TestTrue(FString::Printf(TEXT("타일 %s: 아이콘 상자 상단 %.1f >= 머리띠 %.0f"), Fixture.Name, RewardCard.Top, StripHeight),
				RewardCard.Top >= StripHeight - Tolerance);
			if (!Fixture.bHasExtra)
			{
				// 보상 하나면 카드가 칸 가운데 — 비어 있는 둘째 칸 슬롯 여백이 카드를 한쪽으로 밀면 아이콘 자리와 어긋난다
				TestTrue(FString::Printf(TEXT("타일 %s: 카드 가로 중심 %.1f (기대 %.1f)"),
					Fixture.Name, RewardCard.CenterX(), LocalTileWidth * 0.5),
					FMath::Abs(RewardCard.CenterX() - LocalTileWidth * 0.5) <= Tolerance);
			}
		}
		if (Fixture.bHasExtra && TestTrue(FString::Printf(TEXT("타일 %s: 보상 둘 줄 측정"), Fixture.Name),
			RewardCard.bFound && QtyLabel.bFound && ExtraIconBox.bFound && ExtraQty.bFound))
		{
			// (a) 두 수량 줄은 같은 기준선 — 둘째 아이콘 상자가 카드와 같은 높이라 수량 상단이 맞아야 한다
			TestTrue(FString::Printf(TEXT("타일 %s: 둘째 아이콘 상자 높이 %.1f (기대 %.0f)"),
				Fixture.Name, ExtraIconBox.Height, ExpectCard.Y),
				FMath::Abs(ExtraIconBox.Height - ExpectCard.Y) <= Tolerance);
			TestTrue(FString::Printf(TEXT("타일 %s: 둘째 수량 상단 %.1f (첫 수량 %.1f)"),
				Fixture.Name, ExtraQty.Top, QtyLabel.Top),
				FMath::Abs(ExtraQty.Top - QtyLabel.Top) <= Tolerance);
			// (b) 아이콘 상자 둘·수량 둘이 칸 좌우 가장자리에서 여백을 남긴다 — 「x100」은 다이아 칸보다 넓어 음수 패딩으로 걸쳐 있다
			const double PairLeft = FMath::Min(FMath::Min(RewardCard.Left, ExtraIconBox.Left),
				FMath::Min(QtyLabel.Left, ExtraQty.Left));
			const double PairRight = FMath::Max(FMath::Max(RewardCard.Right(), ExtraIconBox.Right()),
				FMath::Max(QtyLabel.Right(), ExtraQty.Right()));
			TestTrue(FString::Printf(TEXT("타일 %s: 보상 둘 줄 %.1f..%.1f (칸 폭 %.0f, 좌우 여백 하한 %.0f)"),
				Fixture.Name, PairLeft, PairRight, LocalTileWidth, PairRowMinMargin),
				PairLeft >= PairRowMinMargin && PairRight <= LocalTileWidth - PairRowMinMargin);
			// (c) 둘째 그림은 반 상자(폭 반 · 높이 0.72) 안에 그림 비율대로 꽉 찬다 — 한 변은 상자에 닿는다
			if (const UImage* ExtraImage = Cast<UImage>(Tile->GetWidgetFromName(TEXT("ExtraRewardIcon"))))
			{
				const FVector2D PairBox(TileIconBoxWidth * 0.5, TileIconBoxHeight * PairBoxHeightRatio);
				const FVector2D Drawn = FVector2D(ExtraImage->GetBrush().GetImageSize());
				TestTrue(FString::Printf(TEXT("타일 %s: 둘째 그림 %.1fx%.1f (반 상자 %.1fx%.1f 안에 맞춤)"),
					Fixture.Name, Drawn.X, Drawn.Y, PairBox.X, PairBox.Y),
					Drawn.X <= PairBox.X + Tolerance && Drawn.Y <= PairBox.Y + Tolerance
					&& (FMath::Abs(Drawn.X - PairBox.X) <= Tolerance || FMath::Abs(Drawn.Y - PairBox.Y) <= Tolerance));
			}
		}
		// 도장은 상태표가 켜 준 칸에서만 자리를 얻는다 — 접힌 칸에서 0x0 을 재고 초록이 되지 않게 표에서 가져온다
		const bool bExpectSeal = FAttendanceTileVisibility::Resolve(Fixture.State, Fixture.bMilestone).bSeal;
		TestTrue(FString::Printf(TEXT("타일 %s: DoneMark 바인딩"), Fixture.Name), Seal.bFound);
		// 수량 잉크 대조 프레임 — 라벨만 Hidden 으로 지워(배치 유지) 같은 칸을 한 장 더 그린다
		FImage TileImageNoLabel;
		bool bNoLabelDrawn = false;
		if (UWidget* QuantityWidget = Reward ? Reward->GetWidgetFromName(TEXT("QuantityText")) : nullptr)
		{
			const ESlateVisibility ShownVisibility = QuantityWidget->GetVisibility();
			QuantityWidget->SetVisibility(ESlateVisibility::Hidden);
			bNoLabelDrawn = DrawToImage(TileSlate, TileFrameSize, TileImageNoLabel);
			QuantityWidget->SetVisibility(ShownVisibility);
		}
		TestTrue(FString::Printf(TEXT("타일 %s: 수량 잉크 대조 프레임"), Fixture.Name), bNoLabelDrawn);
		int32 QtyInkPixels = 0;
		const FLayoutBox QtyInk = bNoLabelDrawn
			? LabelInkBounds(TileImage, TileImageNoLabel, QtyLabel, QtyInkPixels) : FLayoutBox();
		// 수량은 아이콘 상자 아래쪽 칸 바닥 쪽에 선다 — 위로는 아이콘 중심을 넘지 않고 아래로는 칸 밖으로 안 나간다
		if (RewardCard.bFound && QtyLabel.bFound)
		{
			TestTrue(FString::Printf(
				TEXT("타일 %s: 수량 라벨 %.1f..%.1f (상단 >= 아이콘 중심 %.1f · 하단 <= 칸 %.0f)"),
				Fixture.Name, QtyLabel.Top, QtyLabel.Bottom(), CardCenterY, LocalTileHeight),
				QtyLabel.Top >= CardCenterY - Tolerance && QtyLabel.Bottom() <= LocalTileHeight + Tolerance);
		}
		// 보상 하나인 칸의 수량 잉크 중심 = 시안 수량 줄(칸 아래 16 · 줄 38) — 받은 칸은 잉크가 어두워져도 자리는 같다
		if (!Fixture.bHasExtra)
		{
			TestTrue(FString::Printf(TEXT("타일 %s: 수량 잉크 중심 y %.1f (기대 %.1f ± %.0f, %d px)"),
				Fixture.Name, QtyInk.CenterY(), QtyInkCenterY, QtyInkCenterTolerance, QtyInkPixels),
				QtyInk.bFound && FMath::Abs(QtyInk.CenterY() - QtyInkCenterY) <= QtyInkCenterTolerance);
		}
		if (SealTransform.bFound)
		{
			// SetDayView 가 캡처 전에 떨어지던 자세를 제자리로 되돌린다 — 크기 애니메이션은 없다(스펙 §0-4)
			TestTrue(FString::Printf(TEXT("타일 %s: DoneMark 저작 스케일 (%.2f, %.2f) (기대 1.00)"),
				Fixture.Name, SealTransform.Scale.X, SealTransform.Scale.Y),
				FMath::IsNearlyEqual(SealTransform.Scale.X, 1.0, 0.01)
				&& FMath::IsNearlyEqual(SealTransform.Scale.Y, 1.0, 0.01));
		}
		double SealInkBottom = 0.0;
		if (bExpectSeal && Seal.bFound)
		{
			TestTrue(FString::Printf(TEXT("타일 %s: 도장 정사각 %.0fx%.0f · 캔버스 %.0f"),
				Fixture.Name, Seal.Width, Seal.Height, SealCanvas),
				FMath::Abs(Seal.Width - Seal.Height) <= 1.0 && FMath::Abs(Seal.Width - SealCanvas) <= Tolerance);
			// 도장은 아이콘 상자 가운데에 찍힌다 (시안 .seal top 112 = .box 중심)
			TestTrue(FString::Printf(TEXT("타일 %s: 도장 중심 (%.1f, %.1f) (기대 %.1f, %.1f)"),
				Fixture.Name, SealCenter.X, SealCenter.Y, LocalTileWidth * 0.5, IconCenterY),
				FMath::Abs(SealCenter.X - LocalTileWidth * 0.5) <= Tolerance
				&& FMath::Abs(SealCenter.Y - IconCenterY) <= Tolerance);
			// 연한 도장(0.62)이라 수량 윗부분에 살짝 걸치는 건 시안 그대로다 — 숫자 가운데 줄까지 덮으면 읽히지 않는다
			SealInkBottom = SealCenter.Y + SealInkDiameter * 0.5 * (Seal.Width / SealCanvas);
			if (TestTrue(FString::Printf(TEXT("타일 %s: 수량 잉크 검출 %d px"), Fixture.Name, QtyInkPixels), QtyInk.bFound))
			{
				TestTrue(FString::Printf(TEXT("타일 %s: 도장 아래 끝 %.1f <= 수량 잉크 가운데 %.1f"),
					Fixture.Name, SealInkBottom, QtyInk.CenterY()),
					SealInkBottom <= QtyInk.CenterY());
			}
		}

		TSharedPtr<FJsonObject> TileJson = MakeShared<FJsonObject>();
		// 타일 픽스처의 상자는 타일 로컬 좌표다(프레임 상자는 FrameStack 로컬)
		TileJson->SetStringField(TEXT("space"), TEXT("tile"));
		TileJson->SetStringField(TEXT("png"), TileFile);
		TileJson->SetBoolField(TEXT("expect_seal"), bExpectSeal);
		TileJson->SetBoolField(TEXT("expect_extra"), Fixture.bHasExtra);
		// 음수면 이 패스가 픽셀 판정 자격을 얻지 못한 것이다(대조군 게이트).
		TileJson->SetNumberField(TEXT("plate_surface_luma"), PlateLuma);
		TileJson->SetStringField(TEXT("badge"), BadgeName);
		TileJson->SetObjectField(TEXT("day_badge"), BoxJson(Badge));
		TileJson->SetObjectField(TEXT("strip_text"), BoxJson(StripText));
		TileJson->SetObjectField(TEXT("reward_card"), BoxJson(RewardCard));
		TileJson->SetNumberField(TEXT("expect_card_w"), ExpectCard.X);
		TileJson->SetNumberField(TEXT("expect_card_h"), ExpectCard.Y);
		TileJson->SetObjectField(TEXT("extra_icon_box"), BoxJson(ExtraIconBox));
		TileJson->SetObjectField(TEXT("extra_qty"), BoxJson(ExtraQty));
		TileJson->SetObjectField(TEXT("done_mark"), BoxJson(Seal));
		TileJson->SetNumberField(TEXT("seal_center_x"), SealCenter.X);
		TileJson->SetNumberField(TEXT("seal_center_y"), SealCenter.Y);
		TileJson->SetNumberField(TEXT("seal_ink_bottom"), SealInkBottom);
		TileJson->SetNumberField(TEXT("authored_seal_angle"), SealTransform.Angle);
		TileJson->SetNumberField(TEXT("authored_seal_scale_x"), SealTransform.Scale.X);
		TileJson->SetNumberField(TEXT("authored_seal_scale_y"), SealTransform.Scale.Y);
		TileJson->SetNumberField(TEXT("card_center_y"), CardCenterY);
		TileJson->SetNumberField(TEXT("qty_ink_top"), QtyInkTop);
		TileJson->SetNumberField(TEXT("qty_ink_bottom"), QtyInkBottom);
		TileJson->SetObjectField(TEXT("qty_real_ink"), BoxJson(QtyInk));
		TileJson->SetNumberField(TEXT("qty_real_ink_px"), QtyInkPixels);
		if (QtyLabel.bFound)
		{
			QtyInkRows.Add(FString(Fixture.Name), FVector2D(QtyInkTop, QtyInkBottom));
		}
		TileJson->SetObjectField(TEXT("qty_label"), BoxJson(QtyLabel));
		TileReport->SetObjectField(Fixture.Name, TileJson);
	}
	// C6 의 핵심 약속 — 보상이 둘이어도 마일스톤이어도 수량 줄이 안 움직인다. 열 장 전부 같은 행이어야 하고,
	// 라벨을 못 잰 픽스처는 빠져 있어야(= 여기서 빨간불) 0 끼리 비교해 통과하는 일이 없다
	TestEqual(TEXT("수량 잉크 띠 수집 (타일 전부)"), QtyInkRows.Num(),
		static_cast<int32>(UE_ARRAY_COUNT(TileFixtures)));
	if (const FVector2D* BaselineInk = QtyInkRows.Find(TEXT("future")))
	{
		for (const FTileFixture& Fixture : TileFixtures)
		{
			const FVector2D* Ink = QtyInkRows.Find(FString(Fixture.Name));
			if (!TestNotNull(FString::Printf(TEXT("타일 %s: 수량 잉크 띠 기록"), Fixture.Name), Ink))
			{
				continue;
			}
			TestTrue(FString::Printf(TEXT("타일 %s: 수량 잉크 띠 %.1f..%.1f (기준 future %.1f..%.1f)"),
				Fixture.Name, Ink->X, Ink->Y, BaselineInk->X, BaselineInk->Y),
				FMath::Abs(Ink->X - BaselineInk->X) <= Tolerance
				&& FMath::Abs(Ink->Y - BaselineInk->Y) <= Tolerance);
		}
	}
	else
	{
		AddError(TEXT("수량 잉크 띠 기준(future 타일)이 없습니다 ― 보상 수 대조를 세울 수 없습니다"));
	}

	Report->SetObjectField(TEXT("tiles"), TileReport);
	Report->SetStringField(TEXT("box_space_note"),
		TEXT("프레임 상자·rects(tile_* 제외) = FrameStack 로컬(축소 전) · tile_* 와 타일 상자 = 타일 로컬 (각 항목의 space 필드가 권위)"));
	Report->SetNumberField(TEXT("face_rasterized_frames"), FaceRasterizedFrames);
	Report->SetBoolField(TEXT("level_viewport_drawn"), bHasLevelViewport);

	FString Serialized;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);
	FFileHelper::SaveStringToFile(Serialized, *(Directory / TEXT("attendance-render.json")));

	const int32 ExpectedFrames = static_cast<int32>(UE_ARRAY_COUNT(Fixtures) * UE_ARRAY_COUNT(FrameWidths));
	if (!bPixelJudgment)
	{
		AddWarning(FString::Printf(
			TEXT("픽셀 면 판정을 건너뛰었습니다(레벨 뷰포트 %d · 단색 %.1f · 머티리얼 대조군 %.1f · "
				 "플레이트 마스터 대조군 %.1f) ― PNG 는 기하·상태 레이어 대조에만 쓰고 면 색은 PIE 순회에서 보세요"),
			bHasLevelViewport ? 1 : 0, SolidControlAlpha, MaterialControlAlpha, PlateMasterAlpha));
	}
	else if (FaceRasterizedFrames < ExpectedFrames)
	{
		// 같은 패스에서 같은 마스터가 그려졌으니 전멸이든 일부든 환경이 아니라 회귀다. 빈 프레임 이름을 남긴다
		AddError(FString::Printf(
			TEXT("머티리얼 면이 %d/%d 프레임만 래스터됐습니다(같은 마스터 대조군 %.1f 는 정상) ― 빈 프레임: %s"),
			FaceRasterizedFrames, ExpectedFrames, PlateMasterAlpha,
			*FString::Join(BlankFaceFrames, TEXT(", "))));
	}
	AddInfo(FString::Printf(TEXT("렌더 증거: %s (프레임 %d · 타일 %d)"),
		*Directory, ExpectedFrames, static_cast<int32>(UE_ARRAY_COUNT(TileFixtures))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveOpsAttendanceTileTapTest, "CGR.LiveOps.AttendanceTileTap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLiveOpsAttendanceTileTapTest::RunTest(const FString& Parameters)
{
	using namespace LiveOpsAttendance;

	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UClass* TileClass = LoadClass<UAttendanceDayTileWidget>(nullptr, TilePath);
	if (EditorWorld == nullptr || TileClass == nullptr)
	{
		AddError(TEXT("탭 하네스를 세울 수 없습니다 ― 에디터 월드 / UIE_AttendanceDayTile 을 확인하세요"));
		return false;
	}

	UAttendanceDayTileWidget* Tile = CreateWidget<UAttendanceDayTileWidget>(EditorWorld, TileClass);
	if (!TestNotNull(TEXT("UIE_AttendanceDayTile 인스턴스 생성"), Tile))
	{
		return false;
	}
	TStrongObjectPtr<UAttendanceDayTileWidget> TileGuard(Tile);
	// NativeOnMouseButtonDown 은 protected 라 테스트가 직접 못 부른다 — SObjectWidget 을 거쳐 실제 라우팅으로 민다
	const TSharedRef<SWidget> TileSlate = Tile->TakeWidget();

	FAttendanceDayView DayView;
	DayView.Day = TapSampleDay;
	DayView.bRowFound = true;
	DayView.State = EAttendanceTileState::Today;
	// 아이콘이 없으면 SetDayView 가 카드를 접으며 경고를 찍는다 — 탭 판정과 무관한 잡음이라 실물을 물린다
	DayView.PrimaryIcon = LoadObject<UObject>(nullptr, DiamondIconPath);
	DayView.PrimaryLabel = FText::FromString(TEXT("x10"));
	Tile->SetDayView(DayView);

	int32 Taps = 0;
	int32 TappedDay = 0;
	const FDelegateHandle Handle = Tile->OnTapped.AddLambda([&Taps, &TappedDay](int32 Day)
	{
		++Taps;
		TappedDay = Day;
	});

	const FGeometry TileGeometry =
		FGeometry::MakeRoot(FVector2D(TileWidth, TileHeight), FSlateLayoutTransform());
	// PressedButtons 는 포인터로 들려 다니므로 이벤트보다 오래 살아야 한다
	const TSet<FKey> PressedButtons = { EKeys::LeftMouseButton };
	const FVector2D TileCentre(TileWidth * 0.5, TileHeight * 0.5);
	const FPointerEvent PressEvent(0, TileCentre, TileCentre, PressedButtons,
		EKeys::LeftMouseButton, 0.f, FModifierKeysState());

	// 기본값은 탭 불가 — 패널이 열어 주기 전에는 어느 칸도 수령을 트리거하면 안 된다
	TestFalse(TEXT("탭 잠김: 프레스가 Handled 가 아니다"),
		TileSlate->OnMouseButtonDown(TileGeometry, PressEvent).IsEventHandled());
	TestEqual(TEXT("탭 잠김: 브로드캐스트 0회"), Taps, 0);
	TestTrue(FString::Printf(TEXT("탭 잠김: 가시성 %s (기대 SelfHitTestInvisible)"),
		VisibilityName(Tile->GetVisibility())),
		Tile->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);

	const UWidget* HaloFlash = Tile->GetWidgetFromName(TEXT("TodayHaloFlash"));
	if (!TestNotNull(TEXT("TodayHaloFlash 바인딩"), HaloFlash))
	{
		Tile->OnTapped.Remove(Handle);
		return false;
	}
	TestTrue(TEXT("탭 잠김: 프레스 플래시가 꺼져 있다"),
		HaloFlash->GetVisibility() == ESlateVisibility::Collapsed);

	Tile->SetTappable(true);
	TestTrue(FString::Printf(TEXT("탭 가능: 가시성 %s (기대 Visible)"), VisibilityName(Tile->GetVisibility())),
		Tile->GetVisibility() == ESlateVisibility::Visible);
	TestTrue(TEXT("탭 가능: 프레스가 Handled"),
		TileSlate->OnMouseButtonDown(TileGeometry, PressEvent).IsEventHandled());
	TestEqual(TEXT("탭 가능: 브로드캐스트 1회"), Taps, 1);
	// 패널은 이 날짜로 오늘 칸인지 가린다 — 0 이나 인덱스가 실려 오면 엉뚱한 날을 수령한다
	TestEqual(TEXT("탭 가능: 브로드캐스트가 실은 일차"), TappedDay, TapSampleDay);
	// R33 프레스 피드백. 아웃라인 전용 RoundedBox 는 RenderOpacity 가 안 실려 **가시성**이 유일한 신호다
	TestTrue(FString::Printf(TEXT("탭 가능: 프레스 플래시가 켜졌다 (가시성 %s)"),
		VisibilityName(HaloFlash->GetVisibility())),
		HaloFlash->GetVisibility() != ESlateVisibility::Collapsed);

	// 프레스는 같은 스택에서 패널 잠금까지 내려온다 — 여기서 플래시가 꺼지면 피드백이 한 프레임도 안 그려진다(R30)
	Tile->SetTappable(false);
	TestTrue(FString::Printf(TEXT("잠금 직후: 프레스 플래시가 살아 있다 (가시성 %s)"),
		VisibilityName(HaloFlash->GetVisibility())),
		HaloFlash->GetVisibility() != ESlateVisibility::Collapsed);
	// 아래 '수령 후 자가 잠금' 단언이 이미 잠긴 칸에서 출발하지 않게 되돌린다
	Tile->SetTappable(true);

	// 수령 연출 경로는 RefreshAll 없이 SetDayView 만 부른다 — 상태가 넘어가면 타일이 스스로 잠가야 한다
	FAttendanceDayView ClaimedView = DayView;
	ClaimedView.State = EAttendanceTileState::ClaimedToday;
	Tile->SetDayView(ClaimedView);
	TestTrue(FString::Printf(TEXT("수령 후: 가시성 %s (기대 SelfHitTestInvisible)"),
		VisibilityName(Tile->GetVisibility())),
		Tile->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);
	TestTrue(TEXT("수령 후: 프레스 플래시가 꺼졌다"),
		HaloFlash->GetVisibility() == ESlateVisibility::Collapsed);
	TestFalse(TEXT("수령 후: 재탭이 Handled 가 아니다"),
		TileSlate->OnMouseButtonDown(TileGeometry, PressEvent).IsEventHandled());
	TestEqual(TEXT("수령 후: 브로드캐스트 수 그대로"), Taps, 1);

	// 연출 중 재탭 차단은 패널이 SetTappable(false) 로도 건다 — 같은 프레스가 다시 통과하면 안 된다
	Tile->SetDayView(DayView);
	Tile->SetTappable(true);
	Tile->SetTappable(false);
	TestFalse(TEXT("재잠금: 프레스가 Handled 가 아니다"),
		TileSlate->OnMouseButtonDown(TileGeometry, PressEvent).IsEventHandled());
	TestEqual(TEXT("재잠금: 브로드캐스트 수 그대로"), Taps, 1);

	Tile->OnTapped.Remove(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveOpsAttendanceTileStampResetTest, "CGR.LiveOps.AttendanceTileStampResetsOnSetDayView",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLiveOpsAttendanceTileStampResetTest::RunTest(const FString& Parameters)
{
	using namespace LiveOpsAttendance;

	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UClass* TileClass = LoadClass<UAttendanceDayTileWidget>(nullptr, TilePath);
	if (EditorWorld == nullptr || TileClass == nullptr)
	{
		AddError(TEXT("도장 하네스를 세울 수 없습니다 ― 에디터 월드 / UIE_AttendanceDayTile 을 확인하세요"));
		return false;
	}
	UAttendanceDayTileWidget* Tile = CreateWidget<UAttendanceDayTileWidget>(EditorWorld, TileClass);
	if (!TestNotNull(TEXT("UIE_AttendanceDayTile 인스턴스 생성"), Tile))
	{
		return false;
	}
	TStrongObjectPtr<UAttendanceDayTileWidget> TileGuard(Tile);
	// NativeTick 은 protected 라 SObjectWidget::Tick 으로 민다 — 실제 프레임이 타는 경로와 같다
	const TSharedRef<SWidget> TileSlate = Tile->TakeWidget();
	const FGeometry TileGeometry = FGeometry::MakeRoot(FVector2D(TileWidth, TileHeight), FSlateLayoutTransform());

	FAttendanceDayView DayView;
	DayView.Day = TapSampleDay;
	DayView.bRowFound = true;
	DayView.State = EAttendanceTileState::ClaimedToday;
	DayView.PrimaryIcon = LoadObject<UObject>(nullptr, DiamondIconPath);
	DayView.PrimaryLabel = FText::FromString(TEXT("x10"));
	Tile->SetDayView(DayView);
	Tile->PlayStampEcho(false);

	const UWidget* Seal = Tile->GetWidgetFromName(TEXT("DoneMark"));
	const float* Duration = ReadWidgetProperty<float>(*Tile, TEXT("SealDropDuration"));
	if (!TestNotNull(TEXT("DoneMark 바인딩"), Seal) || !TestNotNull(TEXT("SealDropDuration 저작값"), Duration))
	{
		return false;
	}

	// 메아리 T=0.3 — 도장이 아직 위에서 내려오는 중이어야 아래 되감기 단언이 공허하지 않다
	TileSlate->Tick(TileGeometry, 0.0, 0.3f * *Duration);
	const FWidgetTransform MidPose = Seal->GetRenderTransform();
	TestTrue(FString::Printf(TEXT("메아리 중간: 내려오는 중 (이동 y %.2f · 각도 %.2f · 불투명도 %.2f)"),
		MidPose.Translation.Y, MidPose.Angle, Seal->GetRenderOpacity()),
		!MidPose.Translation.IsNearlyZero(0.001) && Seal->GetRenderOpacity() < 1.f);
	// 크기 애니메이션 금지 (스펙 §0-4) — 떨어짐은 이동·각도·불투명도로만 낸다
	TestTrue(FString::Printf(TEXT("메아리 중간: 크기 (%.2f, %.2f) (기대 1.00)"), MidPose.Scale.X, MidPose.Scale.Y),
		MidPose.Scale.Equals(FVector2D(1.0, 1.0), 0.001));

	auto AssertAtRest = [this, Seal](const TCHAR* When)
	{
		const FWidgetTransform Pose = Seal->GetRenderTransform();
		TestTrue(FString::Printf(TEXT("%s: 도장 이동 (%.2f, %.2f) (기대 0, 0)"), When, Pose.Translation.X, Pose.Translation.Y),
			Pose.Translation.IsNearlyZero(0.001));
		// 놓인 −14° 는 도장 텍스처에 구워져 있다 — 위젯 각도는 0 이 제자리다
		TestTrue(FString::Printf(TEXT("%s: 도장 각도 %.2f (기대 0)"), When, Pose.Angle),
			FMath::IsNearlyZero(Pose.Angle, 0.001f));
		TestTrue(FString::Printf(TEXT("%s: 도장 불투명도 %.2f (기대 1)"), When, Seal->GetRenderOpacity()),
			FMath::IsNearlyEqual(Seal->GetRenderOpacity(), 1.f, 0.001f));
	};

	// 메아리 도중 창을 닫았다 다시 연 것과 같다 — 패널은 SetDayView 만 다시 부른다
	FAttendanceDayView DoneView = DayView;
	DoneView.State = EAttendanceTileState::Done;
	Tile->SetDayView(DoneView);
	AssertAtRest(TEXT("SetDayView(Done) 직후"));
	TestTrue(TEXT("받은 칸: 도장이 보인다"), Seal->GetVisibility() != ESlateVisibility::Collapsed);
	// 되감은 뒤 남은 메아리가 다음 프레임에 도장을 다시 끌어올리면 안 된다
	TileSlate->Tick(TileGeometry, 0.0, 0.1f * *Duration);
	AssertAtRest(TEXT("다음 프레임"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveOpsAttendanceClaimSequenceTest, "CGR.LiveOps.AttendanceClaimSequenceEndsAt045",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLiveOpsAttendanceClaimSequenceTest::RunTest(const FString& Parameters)
{
	ICommonInputModule::GetSettings().LoadData();
	using namespace LiveOpsAttendance;

	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UClass* PanelClass = LoadClass<UAttendancePanelWidget>(nullptr, PanelPath);
	if (EditorWorld == nullptr || PanelClass == nullptr)
	{
		AddError(TEXT("수령 연출 하네스를 세울 수 없습니다 ― 에디터 월드 / UI_AttendancePanel 을 확인하세요"));
		return false;
	}
	UAttendancePanelWidget* Panel = CreateWidget<UAttendancePanelWidget>(EditorWorld, PanelClass);
	if (!TestNotNull(TEXT("UI_AttendancePanel 인스턴스 생성"), Panel))
	{
		return false;
	}
	TStrongObjectPtr<UAttendancePanelWidget> PanelGuard(Panel);
	const TSharedRef<SWidget> PanelSlate = Panel->TakeWidget();
	// 하네스 패널은 칸을 갖지 않는다(BuildGridOnce 는 GameInstance 필요) — 0.24 칸 메아리는 AttendanceTileStampResetsOnSetDayView 가 본다
	AddInfo(TEXT("버튼 → 완료판 교차(0.30~0.45)와 시퀀스 끝 시각만 민다 ― 칸 도장 메아리는 칸 하네스 시험이 맡습니다"));

	FAttendanceView Before;
	Before.CycleDay = 11; Before.NextDay = 12; Before.bCanClaim = true;
	MarkMilestoneDays(Before, { 7, 14, 21, 28 });
	// 수령 후 매니저는 NextDay 를 CycleDay%28+1 로 올린다
	FAttendanceView After = Before;
	After.CycleDay = 12; After.NextDay = 13; After.bCanClaim = false; After.bClaimedToday = true;

	UWidget* Button = Panel->GetWidgetFromName(TEXT("ClaimButton"));
	UWidget* Plate = Panel->GetWidgetFromName(TEXT("ClaimedPlate"));
	const UCommonTextBlock* PlateCountdown = Cast<UCommonTextBlock>(Panel->GetWidgetFromName(TEXT("ClaimedCountdown")));
	if (!TestNotNull(TEXT("ClaimButton"), Button) || !TestNotNull(TEXT("ClaimedPlate"), Plate)
		|| !TestNotNull(TEXT("ClaimedCountdown"), PlateCountdown))
	{
		return false;
	}
	Panel->ApplyView(Before);
	TestTrue(TEXT("받기 전: 버튼 보임 · 완료판 접힘"),
		Button->GetVisibility() != ESlateVisibility::Collapsed && Plate->GetVisibility() == ESlateVisibility::Collapsed);

	Panel->PlayClaimSequence(After, After.CycleDay);
	AdvancePanel(PanelSlate, 0.29f);
	TestTrue(FString::Printf(TEXT("0.29s: 교차 전 ― 완료판 %s · 버튼 불투명도 %.2f"),
		VisibilityName(Plate->GetVisibility()), Button->GetRenderOpacity()),
		Plate->GetVisibility() == ESlateVisibility::Collapsed
		&& Button->GetVisibility() != ESlateVisibility::Collapsed && FMath::IsNearlyEqual(Button->GetRenderOpacity(), 1.f));

	AdvancePanel(PanelSlate, 0.155f);
	// 0.445s — 끝(0.45) 바로 앞: 아직 교차 중이다
	TestTrue(FString::Printf(TEXT("0.445s: 교차 중 ― 버튼 %.2f · 완료판 %.2f"),
		Button->GetRenderOpacity(), Plate->GetRenderOpacity()),
		Button->GetVisibility() != ESlateVisibility::Collapsed
		&& Button->GetRenderOpacity() > 0.f && Button->GetRenderOpacity() < 1.f
		&& Plate->GetVisibility() != ESlateVisibility::Collapsed
		&& Plate->GetRenderOpacity() > 0.f && Plate->GetRenderOpacity() < 1.f);

	AdvancePanel(PanelSlate, 0.01f);
	// 0.455s — 끝(0.45)을 지났다: 끝 프레임이 들어가 버튼은 접히고 완료판이 온전히 선다. 히어로 번쩍임이 있던 옛 끝(0.46)이면 아직 교차 중이다
	auto AssertSettled = [this, Button, Plate](const TCHAR* When)
	{
		TestTrue(FString::Printf(TEXT("%s: 버튼 접힘 (%s)"), When, VisibilityName(Button->GetVisibility())),
			Button->GetVisibility() == ESlateVisibility::Collapsed);
		TestTrue(FString::Printf(TEXT("%s: 완료판 보임 · 불투명도 %.2f"), When, Plate->GetRenderOpacity()),
			Plate->GetVisibility() != ESlateVisibility::Collapsed && FMath::IsNearlyEqual(Plate->GetRenderOpacity(), 1.f));
	};
	AssertSettled(TEXT("0.455s"));
	// 카운트다운은 완료판 둘째 줄 하나에서만 돈다 — 위 시간 줄(NextText)은 없어졌다
	TestTrue(FString::Printf(TEXT("카운트다운이 완료판에서 돈다 「%s」"), *PlateCountdown->GetText().ToString()),
		PlateCountdown->GetText().ToString().StartsWith(TEXT("다음 출석까지")));
	if (const UCommonTextBlock* PlateTitle = Cast<UCommonTextBlock>(Panel->GetWidgetFromName(TEXT("ClaimedTitle"))))
	{
		TestEqual(TEXT("완료판 첫 줄"), PlateTitle->GetText().ToString(), FString(TEXT("출석 완료")));
	}
	else
	{
		AddError(TEXT("ClaimedTitle 이 없습니다 ― 완료판 두 줄(스펙 ⑤)을 저작하세요"));
	}
	// 끝난 뒤 남은 교차가 다음 프레임에 버튼을 되살리면 안 된다
	AdvancePanel(PanelSlate, 0.1f);
	AssertSettled(TEXT("다음 프레임"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveOpsAttendanceServerTimeInvalidTest, "CGR.LiveOps.AttendanceServerTimeInvalidNoCountdown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLiveOpsAttendanceServerTimeInvalidTest::RunTest(const FString& Parameters)
{
	ICommonInputModule::GetSettings().LoadData();
	using namespace LiveOpsAttendance;

	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UClass* PanelClass = LoadClass<UAttendancePanelWidget>(nullptr, PanelPath);
	if (EditorWorld == nullptr || PanelClass == nullptr)
	{
		AddError(TEXT("서버 시간 하네스를 세울 수 없습니다 ― 에디터 월드 / UI_AttendancePanel 을 확인하세요"));
		return false;
	}
	UAttendancePanelWidget* Panel = CreateWidget<UAttendancePanelWidget>(EditorWorld, PanelClass);
	if (!TestNotNull(TEXT("UI_AttendancePanel 인스턴스 생성"), Panel))
	{
		return false;
	}
	TStrongObjectPtr<UAttendancePanelWidget> PanelGuard(Panel);
	const TSharedRef<SWidget> PanelSlate = Panel->TakeWidget();

	UWidget* Button = Panel->GetWidgetFromName(TEXT("ClaimButton"));
	UWidget* Plate = Panel->GetWidgetFromName(TEXT("ClaimedPlate"));
	UCommonTextBlock* PlateCountdown = Cast<UCommonTextBlock>(Panel->GetWidgetFromName(TEXT("ClaimedCountdown")));
	if (!TestNotNull(TEXT("ClaimButton"), Button) || !TestNotNull(TEXT("ClaimedPlate"), Plate)
		|| !TestNotNull(TEXT("ClaimedCountdown"), PlateCountdown))
	{
		return false;
	}

	// 받은 직후 권위 시각을 잃은 프레임 — 완료판·카운트다운이 거짓 숫자를 세면 안 된다 (Review Focus 4)
	FAttendanceView Before;
	Before.CycleDay = 11; Before.NextDay = 12; Before.bCanClaim = true;
	MarkMilestoneDays(Before, { 7, 14, 21, 28 });
	FAttendanceView InvalidAfter = Before;
	InvalidAfter.CycleDay = 12; InvalidAfter.NextDay = 13; InvalidAfter.bCanClaim = false; InvalidAfter.bClaimedToday = true;
	InvalidAfter.bServerTimeValid = false;

	Panel->ApplyView(Before);
	// 카운트다운이 한 번이라도 시작되면 Start 가 바로 글자를 다시 쓴다 — 표지 글자가 남아 있으면 시작되지 않은 것이다
	const FString Sentinel(TEXT("countdown-not-started"));
	PlateCountdown->SetText(FText::FromString(Sentinel));
	Panel->PlayClaimSequence(InvalidAfter, InvalidAfter.CycleDay);
	AdvancePanel(PanelSlate, 0.3f);
	AdvancePanel(PanelSlate, 0.2f);

	TestTrue(FString::Printf(TEXT("완료판 접힘 (%s)"), VisibilityName(Plate->GetVisibility())),
		Plate->GetVisibility() == ESlateVisibility::Collapsed);
	TestTrue(FString::Printf(TEXT("버튼은 남고 비활성 (가시성 %s · 활성 %d · 불투명도 %.2f)"),
		VisibilityName(Button->GetVisibility()), Button->GetIsEnabled() ? 1 : 0, Button->GetRenderOpacity()),
		Button->GetVisibility() == ESlateVisibility::Visible && !Button->GetIsEnabled()
		&& FMath::IsNearlyEqual(Button->GetRenderOpacity(), 1.f));
	TestEqual(TEXT("카운트다운 시작 안 함"), PlateCountdown->GetText().ToString(), Sentinel);
	return true;
}

#endif
