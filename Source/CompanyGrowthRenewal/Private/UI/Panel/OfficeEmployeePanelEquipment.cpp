// 사무실 직원 패널 — 장비 탭 (월드 칸 + 지시선 + 상세 카드 + 장착 순간 + 장비 강화). 설계 §5
#include "UI/Panel/OfficeEmployeePanelWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Entity/Officeworker/FunnyOfficeworker.h"
#include "Enum/ItemType.h"
#include "Enum/WidgetType.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Manager/EmployeeManager.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/SettingsManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/EmployeeEquipmentTable.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentDetailBuilder.h"
#include "UI/Element/Employee/EmployeeEquipmentDialogWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentIconVisual.h"
#include "UI/Element/Employee/EmployeeEquipmentListWidget.h"
#include "UI/Element/Employee/EmployeeRarityDisplay.h"
#include "UI/Element/Employee/EmployeeRotateRingWidget.h"
#include "UI/Element/Employee/EquipmentCalloutLinesWidget.h"
#include "UI/Element/Employee/EquipmentDetailWindowWidget.h"
#include "UI/Element/Effects/SelectionChevronWidget.h"
#include "UI/Panel/EquipmentGachaPanelWidget.h"
#include "Utils/EmployeeEquipmentEffectEvaluator.h"

#define LOCTEXT_NAMESPACE "OfficeEmployeePanelEquipment"

namespace OfficeEquipTab
{
	// TabSwitcher 자식 순서 (0 성장 / 1 잠재능력 / 2 장비)
	constexpr int32 TabIndex = 2;
	constexpr int32 PartCount = static_cast<int32>(EEmployeeEquipmentSlot::Count);

	// 머리 위 셰브론 — 겉모양 맨 위에서 화면 간격(UI 단위, 줌·화면비와 무관), 배율(0.8 = 획 5.2px, 흰 머리 위에서도 읽히는 굵기), 그림자 낙차, 흔들림 진폭·반주기, 등장 페이드
	constexpr float HeadChevronGapUnits = 56.f;
	constexpr float HeadChevronScale = 0.8f;
	constexpr float HeadChevronShadowDropPx = 3.f;
	constexpr float HeadChevronBobPx = 8.f;
	constexpr float HeadChevronBobHalfPeriod = 1.0f;
	constexpr float HeadChevronFadeInTime = 0.15f;
	// 겉모양 맨 위 — 머리 둘레에만 있는 부품(모자·안경·머리칼)은 자기 상자를 원뿔대(바닥 = 상자 폭, 윗면 = 그 절반)로,
	// 머리 아래까지 내려오는 부품(몸·옷)은 머리 위 높이만 작은 원으로 본다. 기준 자세 팔·소매 폭은 돌린 직원에서 깊이가 되어 꼭대기를 띄운다.
	// 상자 윗면 전체(R 실측: 머리칼 +51·우산 모자 +77 단위)는 둥근 머리칼·챙 모자에 과하고, 원뿔(윗면 0)은 금관 같은 원통에 모자라다
	constexpr float HeadPartBelowHeadCm = 8.f;
	constexpr float BodyTopRadiusCm = 8.f;
	constexpr float HeadPartTopFootprintScale = 0.5f;
	constexpr int32 TopFootprintSamples = 8;

	// 발밑 돌리기 고리 — 바닥 반지름(cm)을 화면 가로로 투영해 폭, 높이 = 폭 / 2.6(눕힌 타원). 글자는 고리 아래 6
	constexpr float RotateRingRadiusCm = 50.f;
	constexpr float RotateRingFlatten = 2.6f;
	constexpr float RotateCaptionGap = 6.f;
	// 고리 양 끝 화살표가 타원 밖으로 나가는 폭
	constexpr float RotateArrowReach = 30.f;
	// 누르면 돌리기가 시작되는 영역 = 몸(머리 위 ~ 발) + 고리·글자
	constexpr float RotateBodyHalfWidthRatio = 0.2f;
	constexpr float RotateBodyMinHalfWidth = 60.f;
	constexpr float RotateHeadPad = 24.f;
	// 아래 행 칸(신발·손)과 고리 사이 최소 여백
	constexpr float RotateSlotClearance = 12.f;
	// 빈칸 폭을 끝까지 끌면 반 바퀴 (직원창 전신 회전과 같은 비율)
	constexpr float RotateDegreesPerGapWidth = 180.f;

	// 승인 목업 안 B .wslot — 칸 150. 곡선은 칸 안쪽 가장자리에서 11 떨어져 시작해 앵커 22 앞에서 끝나고,
	// 조절점 = 가로 가운데 · 세로는 칸 높이에서 앵커 쪽으로 15%
	constexpr float SlotFaceSize = 150.f;
	// WBP WorldSlotSize — 칸 버튼은 128 칸 + 사방 8 여백을 ScaleBox 로 맞추므로 보이는 칸 150 = 상자 150×144/128
	constexpr float SlotBoxSize = 168.75f;
	constexpr float LineGapFromSlot = 11.f;
	constexpr float CurveStopShort = 22.f;
	constexpr float CurveControlRise = 0.15f;
	// 열 = 직원 중심 ±(330 × 화면 배율), 위·아래 행은 0.86 만큼 안쪽(호). 배율 = 레이어 폭 / 기준 2560 (16:10 이하는 1)
	constexpr float ColumnOffset = 330.f;
	constexpr float RowArc[3] = { 0.86f, 1.f, 0.86f };
	constexpr float DesignWidth = 2560.f;
	// 행 = 머리 위 · 몸통 · 발 아래. 머리~발 높이의 17% 만큼 바깥.
	// 최소 행 간격 = 상자 반 84.4 + 부위명(6 + 24pt 줄 37) + 장비 이름(19pt 줄 29, 위 −2) + 다음 상자 반 84.4 + 여유
	constexpr float RowOutsideRatio = 0.17f;
	constexpr float RowMinGap = 240.f;
	// 위 행 상자 위끝·아래 행 글자 아래끝이 화면 가장자리에서 24 이상 떨어지게
	constexpr float RowTopMargin = 109.f;
	constexpr float RowBottomMargin = 170.f;
	// 장착 칸 면 = 등급색을 흰색 쪽으로 — 목업 S #B887E0 → 위 #FBF4FF(7%) · 아래 #EBDDF7(28%)
	constexpr float SlotFaceTopGradeShare = 0.07f;
	constexpr float SlotFaceBottomGradeShare = 0.28f;
	constexpr int32 ChipCount = PartCount + 1;

	// 앵커 보정 (cm) — 머리끝 뼈가 없으면 Head 위로, 안경은 얼굴 앞, 의상은 가슴 앞
	constexpr float HeadTopFallbackCm = 22.f;
	constexpr float EyeHeightAlpha = 0.42f;
	constexpr float EyeForwardCm = 12.f;
	constexpr float ChestForwardCm = 10.f;
	constexpr float ChestSideAlpha = 0.3f;
	constexpr float BagShoulderAlpha = 0.85f;
	constexpr float BagFallbackSideCm = 16.f;
	constexpr float HandDropCm = 6.f;
	constexpr float ToastAboveHeadPx = 22.f;

	// 장착 순간 (목업 .wflash 0.5s · .wtoast 0.2s+0.08s) — 1.6초 뒤 칸 복귀.
	// 빛 고리는 작았다가 원 크기로 짧게 꽂히고 0.55초 안에 사라진다(스케일 1.0 초과 금지)
	constexpr float MomentDuration = 1.6f;
	constexpr float FlashInTime = 0.16f;
	constexpr float FlashStartScale = 0.55f;
	constexpr float FlashEndTime = 0.55f;
	constexpr float ToastDelay = 0.08f;
	constexpr float ToastInTime = 0.2f;
	constexpr float MomentFadeOutTime = 0.3f;
	constexpr float ToastRisePx = 12.f;

	// 상세 카드 높이 = 카드 내용 desired. 못 읽으면(첫 프레임) 이 값
	constexpr float DetailFallbackHeight = 980.f;
	// 카드는 세로 가운데 — 위아래 88씩 비워 위끝이 지갑 줄(위 24 + 높이 64) 아래에 온다(16:10 에선 다이아 칩과 가로로 겹친다)
	constexpr float DetailVerticalMargin = 176.f;

	// 칸 등급 액자 — 직원창 홈 칸과 같은 값 (EmployeeWindowEquipment.cpp)
	constexpr float SlotFrameThickness = 2.f;
	constexpr float SlotFrameRadius = 14.f;
	constexpr float SlotSilhouetteAlpha = 0.70f;

	struct FPartLayout
	{
		bool bLeftColumn;
		int32 Row;
	};

	// EEmployeeEquipmentSlot 순서(머리·안경·의상·손·신발·가방) — 왼쪽 머리·의상·신발 / 오른쪽 안경·가방·손
	const FPartLayout PartLayouts[PartCount] = {
		{ true, 0 }, { false, 0 }, { true, 1 }, { false, 2 }, { true, 2 }, { false, 1 }
	};

	FLinearColor SlotFrameColor(const FEmployeeEquipmentGradeRow* Grade, EEmployeeEquipmentGrade Tier)
	{
		return (!Grade || Tier == EEmployeeEquipmentGrade::Common) ? EmployeeRarityDisplay::CommonColor() : Grade->Color;
	}

	// 화면에 보이는 등급색(sRGB)과 흰색을 섞는다 — 선형 공간에서 섞으면 더 하얗게 날아간다
	FLinearColor TowardWhite(const FLinearColor& Grade, float GradeShare)
	{
		const FColor Shown = Grade.ToFColorSRGB();
		auto Mix = [GradeShare](uint8 Channel)
		{
			return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(255.f + (static_cast<float>(Channel) - 255.f) * GradeShare), 0, 255));
		};
		return FLinearColor::FromSRGBColor(FColor(Mix(Shown.R), Mix(Shown.G), Mix(Shown.B)));
	}

	// 부위 실루엣. C++ 문자열 경로 — 폴더 /UI/Textures/UIIcon 은 DefaultGame.ini 쿠킹 목록에 있다
	UTexture2D* PartSilhouette(int32 PartIndex)
	{
		static const TCHAR* const Names[] = { TEXT("Head"), TEXT("Glasses"), TEXT("Outfit"), TEXT("Hand"), TEXT("Shoes"), TEXT("Bag") };
		if (PartIndex < 0 || PartIndex >= static_cast<int32>(UE_ARRAY_COUNT(Names))) return nullptr;
		const FString Path = FString::Printf(
			TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/Slot/T_UIIcon_Slot_%s.T_UIIcon_Slot_%s"), Names[PartIndex], Names[PartIndex]);
		return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Path)).LoadSynchronous();
	}

	float EaseOutCubic(float Alpha)
	{
		const float Clamped = FMath::Clamp(Alpha, 0.f, 1.f);
		return 1.f - FMath::Pow(1.f - Clamped, 3.f);
	}

	FText PrimaryEffectLine(const UTableManagerSubsystem& Tables, const FEmployeeEquipmentDefinitionRow& Definition, int32 Level)
	{
		const FEmployeeEquipmentEconomyRow* Economy = Tables.GetEmployeeEquipmentEconomy();
		const FEmployeeEquipmentEffectRow* Effect = Tables.FindEmployeeEquipmentEffect(Definition.PrimaryEffectId);
		if (!Economy || !Effect)
		{
			return FText::GetEmpty();
		}
		const FEquipmentEffectLoadout Effects = FEmployeeEquipmentEffectEvaluator::BuildItemEffects(
			Definition, Level, Tables.GetEmployeeEquipmentEffects(), *Economy);
		return FText::Format(LOCTEXT("EffectLine", "{0} {1}"), Effect->DisplayName,
			FText::FromString(CGEquipmentDetail::FormatEffectValue(*Effect, Effects.BasicValues.FindRef(Definition.PrimaryEffectId))));
	}

	// 칸 하나 — 직원창 홈 칸과 같은 표기(아이콘·빈 칸 실루엣·등급 액자·Lv)
	void ApplySlotVisual(UObject* Consumer, UIconWithButtonWidget* Button, const UTableManagerSubsystem* Tables,
		EEmployeeEquipmentSlot Part, const FEmployeeEquipmentInstance* Item, bool bWearsBase, TObjectPtr<UMaterialInstanceDynamic>& IconMaterial)
	{
		if (!Button)
		{
			return;
		}
		const int32 PartIndex = static_cast<int32>(Part);
		CGEquipmentIcon::ApplySlotButton(Consumer, Button, Tables, Part, Item, IconMaterial, bWearsBase);
		if (UImage* Silhouette = Cast<UImage>(Button->GetWidgetFromName(TEXT("SlotEmptyIcon"))))
		{
			if (!Item)
			{
				if (UTexture2D* Glyph = PartSilhouette(PartIndex))
				{
					if (UMaterialInstanceDynamic* GlyphMaterial = Silhouette->GetDynamicMaterial())
					{
						GlyphMaterial->SetTextureParameterValue(TEXT("IconTexture"), Glyph);
					}
					else
					{
						Silhouette->SetBrushFromTexture(Glyph);
					}
				}
				Silhouette->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, bWearsBase ? 1.f : SlotSilhouetteAlpha));
			}
			Silhouette->SetVisibility(Item ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
		const FEmployeeEquipmentDefinitionRow* Definition = Tables && Item ? Tables->FindEmployeeEquipmentDefinition(Item->DefinitionId) : nullptr;
		const FEmployeeEquipmentGradeRow* Grade = Definition ? Tables->FindEmployeeEquipmentGrade(Definition->Grade) : nullptr;
		if (UImage* Frame = Cast<UImage>(Button->GetWidgetFromName(TEXT("GradeFrame"))))
		{
			Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
			Frame->SetColorAndOpacity(FLinearColor::White);
			if (UMaterialInstanceDynamic* FrameMaterial = Frame->GetDynamicMaterial())
			{
				FrameMaterial->SetVectorParameterValue(TEXT("LineCol"),
					SlotFrameColor(Grade, Definition ? Definition->Grade : EEmployeeEquipmentGrade::Common));
				FrameMaterial->SetScalarParameterValue(TEXT("ThickPx"), SlotFrameThickness);
				FrameMaterial->SetScalarParameterValue(TEXT("RadiusPx"), SlotFrameRadius);
				FrameMaterial->SetScalarParameterValue(TEXT("BaseA"), 1.f);
				// 액자 SDF 크기 = 칸 비주얼 상자 오버라이드(권위값) — 캐시 지오메트리는 첫 프레임·오프스크린에서 0 이다
				if (const USizeBox* VisualBox = Cast<USizeBox>(Button->GetWidgetFromName(TEXT("HomeSlotVisualSize"))))
				{
					FrameMaterial->SetScalarParameterValue(TEXT("Wpx"), VisualBox->GetWidthOverride());
					FrameMaterial->SetScalarParameterValue(TEXT("Hpx"), VisualBox->GetHeightOverride());
				}
			}
		}
		if (UTextBlock* LevelLabel = Cast<UTextBlock>(Button->GetWidgetFromName(TEXT("SlotLevelText"))))
		{
			const bool bShowLevel = Item && Item->EnhanceLevel > 0;
			LevelLabel->SetVisibility(bShowLevel ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (bShowLevel)
			{
				LevelLabel->SetText(FText::Format(LOCTEXT("SlotLevel", "Lv.{0}"), FText::AsNumber(Item->EnhanceLevel)));
			}
		}
	}

	FString VecJson(const FVector2D& Value)
	{
		return FString::Printf(TEXT("[%.1f,%.1f]"), Value.X, Value.Y);
	}
}

// ===== 바인딩 =====

void UOfficeEmployeePanelWidget::BindEquipmentControls()
{
	WorldSlotButtons.Reset();
	WorldSlotBoxes.Reset();
	WorldSlotLabels.Reset();
	WorldSlotStates.Reset();
	WorldSlotRings.Reset();
	BenchSlotButtons.Reset();
	BenchSlotLabels.Reset();
	BenchSlotRings.Reset();
	WorldSlotIconMaterials.SetNum(OfficeEquipTab::PartCount);
	BenchSlotIconMaterials.SetNum(OfficeEquipTab::PartCount);

	WorldSlotEquipped.Reset();
	WorldSlotFaces.Reset();
	WorldSlotArts.Reset();
	EquipChipButtons.Reset();
	EquipChipOn.Reset();
	EquipChipTexts.Reset();
	EquipChipWells.Reset();

	TArray<FString> Missing;
	auto FindNamed = [this, &Missing](const TCHAR* Prefix, int32 Index) -> UWidget*
	{
		const FName Name(*FString::Printf(TEXT("%s%d"), Prefix, Index));
		UWidget* Found = WidgetTree ? WidgetTree->FindWidget(Name) : nullptr;
		if (!Found)
		{
			Missing.Add(Name.ToString());
		}
		return Found;
	};
	for (int32 Index = 0; Index < OfficeEquipTab::PartCount; ++Index)
	{
		UIconWithButtonWidget* WorldButton = Cast<UIconWithButtonWidget>(FindNamed(TEXT("WorldSlot"), Index));
		UIconWithButtonWidget* BenchButton = Cast<UIconWithButtonWidget>(FindNamed(TEXT("BenchSlot"), Index));
		WorldSlotButtons.Add(WorldButton);
		WorldSlotBoxes.Add(FindNamed(TEXT("WorldSlotBox"), Index));
		WorldSlotLabels.Add(Cast<UTextBlock>(FindNamed(TEXT("WorldSlotLabel"), Index)));
		WorldSlotStates.Add(Cast<UTextBlock>(FindNamed(TEXT("WorldSlotState"), Index)));
		WorldSlotRings.Add(FindNamed(TEXT("WorldSlotRing"), Index));
		BenchSlotButtons.Add(BenchButton);
		BenchSlotLabels.Add(Cast<UTextBlock>(FindNamed(TEXT("BenchSlotLabel"), Index)));
		BenchSlotRings.Add(FindNamed(TEXT("BenchSlotRing"), Index));
		WorldSlotEquipped.Add(FindNamed(TEXT("WorldSlotEquipped"), Index));
		WorldSlotFaces.Add(Cast<UImage>(FindNamed(TEXT("WorldSlotFace"), Index)));
		WorldSlotArts.Add(Cast<UImage>(FindNamed(TEXT("WorldSlotArt"), Index)));
		if (WorldButton) WorldButton->OnClicked().AddUObject(this, &ThisClass::HandleEquipSlotClicked, Index);
		if (BenchButton) BenchButton->OnClicked().AddUObject(this, &ThisClass::HandleEquipSlotClicked, Index);
	}
	for (int32 Chip = 0; Chip < OfficeEquipTab::ChipCount; ++Chip)
	{
		UButton* ChipButton = Cast<UButton>(FindNamed(TEXT("EquipPartChipButton"), Chip));
		EquipChipButtons.Add(ChipButton);
		EquipChipOn.Add(FindNamed(TEXT("EquipPartChipOn"), Chip));
		EquipChipTexts.Add(Cast<UTextBlock>(FindNamed(TEXT("EquipPartChipText"), Chip)));
		EquipChipWells.Add(FindNamed(TEXT("EquipPartChipWell"), Chip));
		// 부위 실루엣은 빈 칸 버튼과 같은 외곽선 재질 — 부위별 글리프만 넣는다(「전체」는 네모 넷이라 저작 그대로)
		if (Chip > 0)
		{
			UImage* Glyph = Cast<UImage>(FindNamed(TEXT("EquipPartChipGlyph"), Chip));
			UTexture2D* Silhouette = OfficeEquipTab::PartSilhouette(Chip - 1);
			if (Glyph && Silhouette)
			{
				if (UMaterialInstanceDynamic* GlyphMaterial = Glyph->GetDynamicMaterial())
				{
					GlyphMaterial->SetTextureParameterValue(TEXT("IconTexture"), Silhouette);
				}
				else
				{
					Glyph->SetBrushResourceObject(Silhouette);
				}
			}
		}
		if (ChipButton)
		{
			switch (Chip)
			{
			case 0: ChipButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleEquipChip0); break;
			case 1: ChipButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleEquipChip1); break;
			case 2: ChipButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleEquipChip2); break;
			case 3: ChipButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleEquipChip3); break;
			case 4: ChipButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleEquipChip4); break;
			case 5: ChipButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleEquipChip5); break;
			default: ChipButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleEquipChip6); break;
			}
		}
	}
	// 저작 잉크는 처음 한 번만 읽는다 — 다시 붙을 때(재구성)는 켜진 칩이 이미 흰색이다
	if (!bEquipChipInkCaptured && EquipChipTexts.Num() > 0 && EquipChipTexts[0])
	{
		EquipChipInk = EquipChipTexts[0]->GetColorAndOpacity();
		bEquipChipInkCaptured = true;
	}
	if (Missing.Num() > 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeEmployeePanel] 장비 칸 위젯 누락 — WBP 이름 규약 확인: %s"), *FString::Join(Missing, TEXT(", ")));
	}

	if (EquipGachaButton) EquipGachaButton->OnClicked().AddUObject(this, &ThisClass::HandleEquipGachaClicked);
	// 틀은 CardHost 안에 저작돼 있다 — 카드가 뜰 때만 붙인다(★ 카드와 한 자리)
	if (EquipDetailFrame && !IsEquipDetailShown()) EquipDetailFrame->RemoveFromParent();
	if (EquipDetailCloseButton) EquipDetailCloseButton->OnCloseClicked.AddUniqueDynamic(this, &ThisClass::HandleEquipDetailCloseClicked);
	if (EquipmentList)
	{
		EquipmentList->OnItemPicked.BindUObject(this, &ThisClass::HandleEquipItemPicked);
		EquipmentList->OnListRefreshed.BindUObject(this, &ThisClass::RefreshEquipOwnedCount);
		if (bEquipAllParts)
		{
			EquipmentList->ShowAllParts();
		}
		else
		{
			EquipmentList->SelectPart(EquipSelectedPart);
		}
	}
	RefreshEquipChips();
	if (EquipToast) EquipToast->SetVisibility(ESlateVisibility::Collapsed);
	SetWorldSlotsShown(false);
	if (BenchSlotRow) BenchSlotRow->SetVisibility(ESlateVisibility::Collapsed);
}

void UOfficeEmployeePanelWidget::UnbindEquipmentControls()
{
	for (UIconWithButtonWidget* Button : WorldSlotButtons)
	{
		if (Button) Button->OnClicked().RemoveAll(this);
	}
	for (UIconWithButtonWidget* Button : BenchSlotButtons)
	{
		if (Button) Button->OnClicked().RemoveAll(this);
	}
	for (UButton* ChipButton : EquipChipButtons)
	{
		if (ChipButton) ChipButton->OnClicked.RemoveAll(this);
	}
	if (EquipGachaButton) EquipGachaButton->OnClicked().RemoveAll(this);
	if (EquipDetailCloseButton) EquipDetailCloseButton->OnCloseClicked.RemoveAll(this);
	if (EquipmentList)
	{
		EquipmentList->OnItemPicked.Unbind();
		EquipmentList->OnListRefreshed.Unbind();
	}
}

bool UOfficeEmployeePanelWidget::IsEquipmentTabShown() const
{
	return TabSwitcher && TabSwitcher->GetActiveWidgetIndex() == OfficeEquipTab::TabIndex;
}

// ===== 탭·직원 동기화 =====

void UOfficeEmployeePanelWidget::SyncEquipmentTab(bool bEmployeeChanged)
{
	const bool bShown = IsEquipmentTabShown() && IsActivated() && SelectedEmployeeId != INDEX_NONE;
	// 장비 탭에 들어올 때마다 「전체」 — 칸 링·곡선 없이 보유 장비 전부를 보여 준다
	const bool bEntering = bShown && !bEquipTabEntered;
	bEquipTabEntered = bShown;
	if (bEntering)
	{
		bEquipAllParts = true;
	}
	// 상세 카드·미리보기·다이얼로그는 직원 하나에 묶여 있다 — 직원이 바뀌거나 탭을 떠나면 걷는다
	if (bEmployeeChanged || !bShown)
	{
		CloseEquipDialog();
		CloseEquipDetail();
		EndEquipMoment();
	}
	if (!bShown)
	{
		SetWorldSlotsShown(false);
		// 돌려 둔 각도는 장비 탭에서만 산다 — 다른 탭으로 가면 정면으로
		ResetWorkerRotation();
		if (BenchSlotRow) BenchSlotRow->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	if (EquipmentList)
	{
		if (bEntering && bEquipAllParts)
		{
			EquipmentList->ShowAllParts();
		}
		EquipmentList->Configure(SelectedEmployeeId);
	}
	RefreshEquipHeader();
	RefreshEquipSlots();
	RefreshEquipChips();
}

void UOfficeEmployeePanelWidget::RefreshEquipmentTab()
{
	if (!IsEquipmentTabShown() || SelectedEmployeeId == INDEX_NONE)
	{
		return;
	}
	if (EquipmentList)
	{
		EquipmentList->Refresh();
	}
	RefreshEquipHeader();
	RefreshEquipSlots();
}

void UOfficeEmployeePanelWidget::HandleEquipmentChanged()
{
	RefreshEquipmentTab();
}

void UOfficeEmployeePanelWidget::RefreshEquipHeader()
{
	UGameInstance* GI = GetGameInstance();
	const UEmployeeEquipmentManagerSubsystem* Equipment = GI ? GI->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
	const UTableManagerSubsystem* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Equipment || !Tables)
	{
		return;
	}
	const int32 PartIndex = static_cast<int32>(EquipSelectedPart);
	const FEmployeeEquipmentSlotRow* PartRow = Tables->FindEmployeeEquipmentSlot(EquipSelectedPart);
	const FText PartName = PartRow ? PartRow->DisplayName : FText::GetEmpty();

	if (EquipPartIcon)
	{
		if (UTexture2D* Glyph = OfficeEquipTab::PartSilhouette(PartIndex))
		{
			EquipPartIcon->SetBrushFromTexture(Glyph);
		}
		EquipPartIcon->SetVisibility(bEquipAllParts && EquipAllPartsGlyph ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (EquipAllPartsGlyph)
	{
		EquipAllPartsGlyph->SetVisibility(bEquipAllParts ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (EquipPartNameText)
	{
		EquipPartNameText->SetText(bEquipAllParts ? LOCTEXT("AllPartsTitle", "전체 장비") : PartName);
	}
	if (EquipPartStateText)
	{
		if (bEquipAllParts)
		{
			int32 EquippedParts = 0;
			for (int32 Index = 0; Index < OfficeEquipTab::PartCount; ++Index)
			{
				if (Equipment->GetEquippedInstance(SelectedEmployeeId, static_cast<EEmployeeEquipmentSlot>(Index)))
				{
					++EquippedParts;
				}
			}
			EquipPartStateText->SetText(FText::Format(LOCTEXT("AllPartsState", "장착 {0}/{1}"),
				FText::AsNumber(EquippedParts), FText::AsNumber(OfficeEquipTab::PartCount)));
		}
		else
		{
			const FEmployeeEquipmentInstance* Item = Equipment->GetEquippedInstance(SelectedEmployeeId, EquipSelectedPart);
			const FEmployeeEquipmentDefinitionRow* Definition = Item ? Tables->FindEmployeeEquipmentDefinition(Item->DefinitionId) : nullptr;
			const FEmployeeEquipmentGradeRow* Grade = Definition ? Tables->FindEmployeeEquipmentGrade(Definition->Grade) : nullptr;
			EquipPartStateText->SetText(Definition
				? FText::Format(LOCTEXT("EquippedState", "장착 중 · {0} {1}"), Definition->DisplayName, Grade ? Grade->DisplayName : FText::GetEmpty())
				: LOCTEXT("EmptyState", "장착 중인 장비 없음"));
		}
	}

	// 빈 안내는 부위(전체면 모두) 보유 수로만 — 등급 거르기로 비는 건 목록이 따로 말한다
	int32 OwnedInScope = 0;
	for (const TPair<FGuid, FEmployeeEquipmentInstance>& Pair : Equipment->GetState().Instances)
	{
		const FEmployeeEquipmentDefinitionRow* Definition = Tables->FindEmployeeEquipmentDefinition(Pair.Value.DefinitionId);
		if (Definition && (bEquipAllParts || Definition->EquipmentSlot == EquipSelectedPart))
		{
			++OwnedInScope;
		}
	}
	if (EquipEmptyText)
	{
		EquipEmptyText->SetText(bEquipAllParts ? LOCTEXT("EmptyAll", "보유한 장비가 없습니다")
			: FText::Format(LOCTEXT("EmptyPart", "보유한 {0} 장비가 없습니다"), PartName));
	}
	if (EquipEmptyNotice)
	{
		EquipEmptyNotice->SetVisibility(OwnedInScope == 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	RefreshEquipOwnedCount();
}

void UOfficeEmployeePanelWidget::RefreshEquipOwnedCount()
{
	if (EquipOwnedCountText && EquipmentList)
	{
		EquipOwnedCountText->SetText(FText::Format(LOCTEXT("OwnedCount", "보유 장비 {0}개"), FText::AsNumber(EquipmentList->GetVisibleItemCount())));
	}
}

void UOfficeEmployeePanelWidget::RefreshEquipSlots()
{
	UGameInstance* GI = GetGameInstance();
	const UEmployeeEquipmentManagerSubsystem* Equipment = GI ? GI->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
	const UTableManagerSubsystem* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const bool bHasEmployee = EmployeeManager && EmployeeManager->FindEmployee(SelectedEmployeeId);
	for (int32 Index = 0; Index < OfficeEquipTab::PartCount; ++Index)
	{
		const EEmployeeEquipmentSlot Part = static_cast<EEmployeeEquipmentSlot>(Index);
		const FEmployeeEquipmentInstance* Item = Equipment && bHasEmployee ? Equipment->GetEquippedInstance(SelectedEmployeeId, Part) : nullptr;
		// 의상·신발은 전원이, 안경은 직급이 되면 기본 파츠를 이미 입고 있다 — 진짜 빈 칸과 구분한다(직원창 홈 칸과 같음)
		const bool bWearsBase = !Item && bHasEmployee && EmployeeManager->HasBaseAppearance(SelectedEmployeeId, Part);
		const FEmployeeEquipmentSlotRow* PartRow = Tables ? Tables->FindEmployeeEquipmentSlot(Part) : nullptr;
		const FText PartName = PartRow ? PartRow->DisplayName : FText::GetEmpty();
		const FEmployeeEquipmentDefinitionRow* Definition = Tables && Item ? Tables->FindEmployeeEquipmentDefinition(Item->DefinitionId) : nullptr;
		const bool bPicked = !bEquipAllParts && Part == EquipSelectedPart;

		if (WorldSlotButtons.IsValidIndex(Index))
		{
			OfficeEquipTab::ApplySlotVisual(this, WorldSlotButtons[Index], Tables, Part, Item, bWearsBase, WorldSlotIconMaterials[Index]);
		}
		// 장착 칸 — 등급 옅은 면(위 7% · 아래 28%) + 등급 테 3, 장비 그림은 칸 가득. 칸 버튼의 작은 아이콘·액자는 이 면이 덮는다
		const FEmployeeEquipmentGradeRow* GradeRow = Definition ? Tables->FindEmployeeEquipmentGrade(Definition->Grade) : nullptr;
		if (WorldSlotEquipped.IsValidIndex(Index) && WorldSlotEquipped[Index])
		{
			WorldSlotEquipped[Index]->SetVisibility(Definition ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (Definition && WorldSlotFaces.IsValidIndex(Index) && WorldSlotFaces[Index])
		{
			const FLinearColor GradeColor = OfficeEquipTab::SlotFrameColor(GradeRow, Definition->Grade);
			if (UMaterialInstanceDynamic* FaceMaterial = WorldSlotFaces[Index]->GetDynamicMaterial())
			{
				FaceMaterial->SetVectorParameterValue(TEXT("FillTop"), OfficeEquipTab::TowardWhite(GradeColor, OfficeEquipTab::SlotFaceTopGradeShare));
				FaceMaterial->SetVectorParameterValue(TEXT("FillBottom"), OfficeEquipTab::TowardWhite(GradeColor, OfficeEquipTab::SlotFaceBottomGradeShare));
				FaceMaterial->SetVectorParameterValue(TEXT("BorderColor"), GradeColor);
			}
		}
		if (WorldSlotArts.IsValidIndex(Index) && WorldSlotArts[Index])
		{
			UObject* Art = WorldSlotIconMaterials.IsValidIndex(Index) && WorldSlotIconMaterials[Index]
				? static_cast<UObject*>(WorldSlotIconMaterials[Index].Get())
				: (Definition && !Definition->Icon.IsNull() ? static_cast<UObject*>(Definition->Icon.LoadSynchronous()) : nullptr);
			CGEquipmentIcon::Apply(WorldSlotArts[Index], Definition ? Art : nullptr);
		}
		if (BenchSlotButtons.IsValidIndex(Index))
		{
			OfficeEquipTab::ApplySlotVisual(this, BenchSlotButtons[Index], Tables, Part, Item, bWearsBase, BenchSlotIconMaterials[Index]);
		}
		if (WorldSlotLabels.IsValidIndex(Index) && WorldSlotLabels[Index]) WorldSlotLabels[Index]->SetText(PartName);
		if (BenchSlotLabels.IsValidIndex(Index) && BenchSlotLabels[Index]) BenchSlotLabels[Index]->SetText(PartName);
		if (WorldSlotStates.IsValidIndex(Index) && WorldSlotStates[Index])
		{
			WorldSlotStates[Index]->SetText(Definition ? Definition->DisplayName : FText::GetEmpty());
			WorldSlotStates[Index]->SetVisibility(Definition ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		for (const TArray<TObjectPtr<UWidget>>* Rings : { &WorldSlotRings, &BenchSlotRings })
		{
			if (Rings->IsValidIndex(Index) && (*Rings)[Index])
			{
				(*Rings)[Index]->SetVisibility(bPicked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
			}
		}
	}
}

void UOfficeEmployeePanelWidget::SelectEquipPart(EEmployeeEquipmentSlot Part)
{
	if (static_cast<int32>(Part) < 0 || Part >= EEmployeeEquipmentSlot::Count)
	{
		return;
	}
	bEquipAllParts = false;
	EquipSelectedPart = Part;
	if (EquipmentList)
	{
		EquipmentList->SelectPart(Part);
	}
	RefreshEquipHeader();
	RefreshEquipSlots();
	RefreshEquipChips();
}

void UOfficeEmployeePanelWidget::SelectAllEquipParts()
{
	bEquipAllParts = true;
	if (EquipmentList)
	{
		EquipmentList->ShowAllParts();
	}
	RefreshEquipHeader();
	RefreshEquipSlots();
	RefreshEquipChips();
}

void UOfficeEmployeePanelWidget::SelectEquipChip(int32 ChipIndex)
{
	if (ChipIndex == 0)
	{
		SelectAllEquipParts();
	}
	else if (ChipIndex > 0 && ChipIndex < OfficeEquipTab::ChipCount)
	{
		SelectEquipPart(static_cast<EEmployeeEquipmentSlot>(ChipIndex - 1));
	}
}

void UOfficeEmployeePanelWidget::RefreshEquipChips()
{
	const int32 OnChip = bEquipAllParts ? 0 : static_cast<int32>(EquipSelectedPart) + 1;
	for (int32 Chip = 0; Chip < EquipChipOn.Num(); ++Chip)
	{
		const bool bOn = Chip == OnChip;
		const ESlateVisibility OnVisibility = bOn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		if (EquipChipOn[Chip])
		{
			EquipChipOn[Chip]->SetVisibility(OnVisibility);
		}
		if (EquipChipWells.IsValidIndex(Chip) && EquipChipWells[Chip])
		{
			EquipChipWells[Chip]->SetVisibility(OnVisibility);
		}
		if (EquipChipTexts.IsValidIndex(Chip) && EquipChipTexts[Chip])
		{
			EquipChipTexts[Chip]->SetColorAndOpacity(bOn ? FSlateColor(FLinearColor::White) : EquipChipInk);
		}
	}
}

void UOfficeEmployeePanelWidget::HandleEquipSlotClicked(int32 SlotIndex)
{
	SelectEquipPart(static_cast<EEmployeeEquipmentSlot>(SlotIndex));
}

// ===== 월드 배치 (매 틱 — 카메라가 아직 미끄러지는 중일 수 있다) =====

void UOfficeEmployeePanelWidget::SetWorldSlotsShown(bool bShown)
{
	bWorldSlotsShown = bShown;
	for (UWidget* Box : WorldSlotBoxes)
	{
		if (!Box) continue;
		// 칸 상자(아이콘·이름 줄)를 눌러도 뒤의 빈칸 닫기 버튼으로 새지 않게 상자가 히트를 받는다
		const ESlateVisibility Wanted = bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
		if (Box->GetVisibility() != Wanted) Box->SetVisibility(Wanted);
	}
	if (!bShown && EquipCalloutLines)
	{
		EquipCalloutLines->ClearCallout();
		LastCurvePoints.Reset();
	}
	if (!bShown)
	{
		SetRotateHintShown(false);
	}
}

void UOfficeEmployeePanelWidget::SetRotateHintShown(bool bShown)
{
	bRotateHintPlaced = bShown;
	for (UWidget* Hint : { static_cast<UWidget*>(RotateRing.Get()), static_cast<UWidget*>(RotateCaption.Get()) })
	{
		if (!Hint) continue;
		// 누름은 패널이 영역으로 판정한다(터널) — 고리·글자는 히트를 받지 않는다
		const ESlateVisibility Wanted = bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		if (Hint->GetVisibility() != Wanted) Hint->SetVisibility(Wanted);
	}
	if (!bShown)
	{
		CancelRotateDrag();
	}
}

// ===== 고른 직원 머리 위 셰브론 (모든 탭 — 장착 순간·대기 직원·닫힘에선 숨김) =====

void UOfficeEmployeePanelWidget::TickHeadChevron(float DeltaTime)
{
	AOfficeworker* Worker = IsActivated() && !bEquipMomentActive ? FocusWorker.Get() : nullptr;
	USkeletalMeshComponent* Body = Worker ? Worker->GetMesh() : nullptr;
	APlayerController* PC = GetOwningPlayer();
	FVector2D ViewportPos;
	if (!Body || !PC || !WorldLayer || Body->GetBoneIndex(FName(TEXT("Head"))) == INDEX_NONE || !GetWorkerVisualTop(PC, Worker, ViewportPos))
	{
		HideHeadChevron();
		return;
	}
	if (!HeadChevron)
	{
		// 그림자를 먼저 붙여 캔버스에서 아래에 그린다
		for (TObjectPtr<USelectionChevronWidget>* Target : { &HeadChevronShadow, &HeadChevron })
		{
			USelectionChevronWidget* Chevron = CreateWidget<USelectionChevronWidget>(this, USelectionChevronWidget::StaticClass());
			if (!Chevron)
			{
				return;
			}
			if (UCanvasPanelSlot* NewSlot = WorldLayer->AddChildToCanvas(Chevron))
			{
				// 하단 가운데 정렬 = V 꼭짓점이 앵커 (건물 선택 셰브론과 같은 배치)
				NewSlot->SetAutoSize(true);
				NewSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
				NewSlot->SetAlignment(FVector2D(0.5f, 1.f));
			}
			Chevron->SetRenderTransformPivot(FVector2D(0.5f, 1.f));
			Chevron->SetVisibility(ESlateVisibility::Collapsed);
			*Target = Chevron;
		}
		HeadChevronShadow->SetChevronColors(FLinearColor(0.007f, 0.016f, 0.042f, 0.55f), FLinearColor(0.007f, 0.016f, 0.042f, 0.3f));
	}
	UCanvasPanelSlot* ChevronSlot = Cast<UCanvasPanelSlot>(HeadChevron->Slot);
	UCanvasPanelSlot* ShadowSlot = HeadChevronShadow ? Cast<UCanvasPanelSlot>(HeadChevronShadow->Slot) : nullptr;
	if (!ChevronSlot || !ShadowSlot)
	{
		return;
	}
	if (HeadChevronWorker.Get() != Worker || HeadChevron->GetVisibility() == ESlateVisibility::Collapsed)
	{
		HeadChevronWorker = Worker;
		HeadChevronElapsed = 0.f;
	}
	HeadChevronElapsed += DeltaTime;

	const FVector2D Absolute = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this).LocalToAbsolute(ViewportPos);
	HeadChevronTopLocal = WorldLayer->GetCachedGeometry().AbsoluteToLocal(Absolute);
	const FVector2D AnchorLocal = HeadChevronTopLocal - FVector2D(0.f, OfficeEquipTab::HeadChevronGapUnits);
	ChevronSlot->SetPosition(AnchorLocal);
	ShadowSlot->SetPosition(AnchorLocal + FVector2D(0.f, OfficeEquipTab::HeadChevronShadowDropPx));

	const USettingsManagerSubsystem* Settings = GetGameInstance() ? GetGameInstance()->GetSubsystem<USettingsManagerSubsystem>() : nullptr;
	// 목업 .selchev = 0 → 위로 오갔다 돌아오는 ease-in-out — 코사인 반주기가 같은 곡선. 배율은 1 이하로만
	const float Bob = (Settings && Settings->IsReduceMotionEnabled()) ? 0.f
		: -OfficeEquipTab::HeadChevronBobPx * 0.5f * (1.f - FMath::Cos(PI * HeadChevronElapsed / OfficeEquipTab::HeadChevronBobHalfPeriod));
	FWidgetTransform Xform;
	Xform.Translation = FVector2D(0.f, Bob);
	Xform.Scale = FVector2D(OfficeEquipTab::HeadChevronScale, OfficeEquipTab::HeadChevronScale);
	const float Opacity = FMath::Clamp(HeadChevronElapsed / OfficeEquipTab::HeadChevronFadeInTime, 0.f, 1.f);
	for (USelectionChevronWidget* Chevron : { HeadChevronShadow.Get(), HeadChevron.Get() })
	{
		Chevron->SetRenderTransform(Xform);
		Chevron->SetRenderOpacity(Opacity);
		if (Chevron->GetVisibility() != ESlateVisibility::HitTestInvisible)
		{
			Chevron->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}

bool UOfficeEmployeePanelWidget::GetWorkerVisualTop(APlayerController* PC, const AOfficeworker* Worker, FVector2D& OutViewportTop,
	TArray<FString>* OutParts)
{
	static const FName HeadBone(TEXT("Head"));
	const USkeletalMeshComponent* Body = Worker ? Worker->GetMesh() : nullptr;
	FVector2D Head;
	if (!Body || !PC || !UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Body->GetSocketLocation(HeadBone), Head, true))
	{
		return false;
	}
	const FVector HeadWorld = Body->GetSocketLocation(HeadBone);
	// 내려다보는 카메라라 상자 윗면의 먼 모서리가 화면 실루엣 꼭대기다 — 8 꼭짓점 투영의 최소 Y
	float TopY = Head.Y;
	float PartTop = TNumericLimits<float>::Max();
	auto Consider = [PC, &PartTop](const FVector& WorldPoint)
	{
		FVector2D Projected;
		if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, WorldPoint, Projected, true))
		{
			PartTop = FMath::Min(PartTop, static_cast<float>(Projected.Y));
		}
	};
	auto CornerOf = [](const FBox& Box, int32 Corner)
	{
		return FVector((Corner & 1) ? Box.Max.X : Box.Min.X, (Corner & 2) ? Box.Max.Y : Box.Min.Y, (Corner & 4) ? Box.Max.Z : Box.Min.Z);
	};
	// 보이는 메시 부품만(머리 위 버블은 위젯, 바닥 선택 링은 낮다)
	TInlineComponentArray<UMeshComponent*> Meshes(Worker);
	for (const UMeshComponent* Mesh : Meshes)
	{
		if (!Mesh || !Mesh->IsRegistered() || !Mesh->IsVisible())
		{
			continue;
		}
		PartTop = TNumericLimits<float>::Max();
		FString PartDetail = TEXT("\"static\":true");
		if (const USkinnedMeshComponent* Skinned = Cast<USkinnedMeshComponent>(Mesh))
		{
			// 리더 포즈 부품은 몸 경계를 빌려 쓰고 몸 경계는 컬링용이라 느슨하다(p4 실측: 머리 위 170 단위) —
			// 에셋의 가져온 기준 자세 상자를 그 에셋의 Head 뼈 기준으로 지금 머리 뼈에 붙인다
			const USkeletalMesh* Asset = Cast<USkeletalMesh>(Skinned->GetSkinnedAsset());
			const int32 RefHead = Asset ? Asset->GetRefSkeleton().FindBoneIndex(HeadBone) : INDEX_NONE;
			if (RefHead == INDEX_NONE)
			{
				continue;
			}
			const FVector RefHeadPos = Asset->GetComposedRefPoseMatrix(RefHead).GetOrigin();
			const FBox Local = Asset->GetImportedBounds().GetBox();
			const FTransform& Xform = Skinned->GetComponentTransform();
			const bool bHeadPart = Local.Min.Z >= RefHeadPos.Z - OfficeEquipTab::HeadPartBelowHeadCm;
			const FVector Extent = Local.GetExtent();
			const float PartScale = static_cast<float>(Xform.GetMaximumAxisScale());
			const float FootprintRadius = static_cast<float>(FMath::Max(Extent.X, Extent.Y)) * PartScale;
			// 원 발자국이라 직원을 돌려도 꼭대기가 출렁이지 않는다
			auto ConsiderRing = [&Consider](const FVector& RingCenter, float RingRadius)
			{
				Consider(RingCenter);
				for (int32 Sample = 0; Sample < OfficeEquipTab::TopFootprintSamples && RingRadius > 0.f; ++Sample)
				{
					const float Angle = 2.f * PI * static_cast<float>(Sample) / OfficeEquipTab::TopFootprintSamples;
					Consider(RingCenter + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * RingRadius);
				}
			};
			FVector TopCenter = bHeadPart ? Local.GetCenter() : RefHeadPos;
			TopCenter.Z = Local.Max.Z;
			ConsiderRing(HeadWorld + Xform.TransformVector(TopCenter - RefHeadPos),
				bHeadPart ? FootprintRadius * OfficeEquipTab::HeadPartTopFootprintScale : OfficeEquipTab::BodyTopRadiusCm * PartScale);
			if (bHeadPart)
			{
				FVector BottomCenter = Local.GetCenter();
				BottomCenter.Z = Local.Min.Z;
				ConsiderRing(HeadWorld + Xform.TransformVector(BottomCenter - RefHeadPos), FootprintRadius);
			}
			PartDetail = FString::Printf(TEXT("\"asset\":\"%s\",\"head_part\":%s,\"min_z\":%.1f,\"max_z\":%.1f,\"ext\":[%.1f,%.1f]"),
				*Asset->GetName(), bHeadPart ? TEXT("true") : TEXT("false"), Local.Min.Z - RefHeadPos.Z, Local.Max.Z - RefHeadPos.Z,
				Extent.X, Extent.Y);
		}
		else
		{
			const FBox Box = Mesh->Bounds.GetBox();
			for (int32 Corner = 0; Corner < 8; ++Corner)
			{
				Consider(CornerOf(Box, Corner));
			}
		}
		if (PartTop < TNumericLimits<float>::Max())
		{
			TopY = FMath::Min(TopY, PartTop);
		}
		if (OutParts)
		{
			OutParts->Add(FString::Printf(TEXT("{\"name\":\"%s\",\"top_y\":%.1f,%s}"), *Mesh->GetName(), PartTop, *PartDetail));
		}
	}
	if (OutParts)
	{
		OutParts->Add(FString::Printf(TEXT("{\"head_y\":%.1f,\"top_y\":%.1f}"), Head.Y, TopY));
	}
	OutViewportTop = FVector2D(Head.X, TopY);
	return true;
}

FString UOfficeEmployeePanelWidget::TestGetVisualTopParts() const
{
	TArray<FString> Parts;
	FVector2D Top;
	APlayerController* PC = GetOwningPlayer();
	if (!GetWorkerVisualTop(PC, FocusWorker.Get(), Top, &Parts))
	{
		return TEXT("[]");
	}
	return TEXT("[") + FString::Join(Parts, TEXT(",")) + TEXT("]");
}

FVector4 UOfficeEmployeePanelWidget::TestGetHeadChevronAnchor() const
{
	const UCanvasPanelSlot* ChevronSlot = HeadChevron ? Cast<UCanvasPanelSlot>(HeadChevron->Slot) : nullptr;
	if (!ChevronSlot || HeadChevron->GetVisibility() == ESlateVisibility::Collapsed)
	{
		return FVector4(-1.f, -1.f, -1.f, -1.f);
	}
	const FVector2D Tip = ChevronSlot->GetPosition();
	return FVector4(Tip.X, Tip.Y, HeadChevronTopLocal.X, HeadChevronTopLocal.Y);
}

bool UOfficeEmployeePanelWidget::TestPreviewHeadwear(FName AppearanceId)
{
	AFunnyOfficeworker* Worker = Cast<AFunnyOfficeworker>(FocusWorker.Get());
	if (!Worker || !Worker->PreviewEquipmentAppearance(EEmployeeEquipmentSlot::Head, EEquipmentAppearanceMode::Override, AppearanceId))
	{
		return false;
	}
	EquipPreviewWorker = Worker;
	return true;
}

void UOfficeEmployeePanelWidget::HideHeadChevron()
{
	for (USelectionChevronWidget* Chevron : { HeadChevronShadow.Get(), HeadChevron.Get() })
	{
		if (Chevron && Chevron->GetVisibility() != ESlateVisibility::Collapsed)
		{
			Chevron->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

bool UOfficeEmployeePanelWidget::ComputeEquipAnchors(AOfficeworker* Worker, TArray<FVector2D>& OutAbsolute,
	FVector2D& OutHeadTopAbsolute, FVector2D& OutCenterAbsolute) const
{
	USkeletalMeshComponent* Body = Worker ? Worker->GetMesh() : nullptr;
	APlayerController* PC = GetOwningPlayer();
	if (!Body || !PC)
	{
		return false;
	}
	const FGeometry ViewportGeo = UWidgetLayoutLibrary::GetViewportWidgetGeometry(const_cast<UOfficeEmployeePanelWidget*>(this));
	auto Project = [PC, &ViewportGeo](const FVector& WorldPos, FVector2D& OutAbs) -> bool
	{
		FVector2D ViewportPos;
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, WorldPos, ViewportPos, true))
		{
			return false;
		}
		OutAbs = ViewportGeo.LocalToAbsolute(ViewportPos);
		return true;
	};
	auto Bone = [Body](const TCHAR* Name, FVector& OutLocation) -> bool
	{
		if (Body->GetBoneIndex(FName(Name)) == INDEX_NONE)
		{
			return false;
		}
		OutLocation = Body->GetSocketLocation(FName(Name));
		return true;
	};

	FVector HeadBone, SpineBone;
	if (!Bone(TEXT("Head"), HeadBone) || !Bone(TEXT("Spine2"), SpineBone))
	{
		return false;
	}
	FVector HeadEnd;
	const FVector HeadTop = Bone(TEXT("HeadTop_End"), HeadEnd)
		? FMath::Lerp(HeadBone, HeadEnd, 0.8f) : HeadBone + FVector::UpVector * OfficeEquipTab::HeadTopFallbackCm;
	const FVector Forward = Worker->GetActorForwardVector();

	// 좌우 짝 뼈는 화면에서 그 칸 쪽에 보이는 것을 고른다 — 직원이 카메라를 보면 오른손이 화면 왼쪽이다
	auto PickSide = [&Bone, &Project](const TCHAR* LeftName, const TCHAR* RightName, bool bWantScreenRight,
		const FVector& Fallback, FVector& OutLocation)
	{
		FVector LeftLocation, RightLocation;
		FVector2D LeftAbs, RightAbs;
		const bool bLeft = Bone(LeftName, LeftLocation) && Project(LeftLocation, LeftAbs);
		const bool bRight = Bone(RightName, RightLocation) && Project(RightLocation, RightAbs);
		if (bLeft && bRight)
		{
			OutLocation = ((LeftAbs.X > RightAbs.X) == bWantScreenRight) ? LeftLocation : RightLocation;
		}
		else
		{
			OutLocation = bLeft ? LeftLocation : (bRight ? RightLocation : Fallback);
		}
	};
	// 어깨 = 위팔 시작(Arm). Shoulder 뼈는 쇄골이라 목 가까이에 있어 가방·의상 앵커가 목에 몰린다(d1 실측)
	const FVector CameraRight = PC->PlayerCameraManager
		? FRotationMatrix(PC->PlayerCameraManager->GetCameraRotation()).GetUnitAxis(EAxis::Y) : FVector::RightVector;
	FVector HandLocation, FootLocation, RightArmLocation, LeftArmLocation;
	PickSide(TEXT("LeftHand"), TEXT("RightHand"), /*bWantScreenRight*/ true, SpineBone, HandLocation);
	PickSide(TEXT("LeftFoot"), TEXT("RightFoot"), /*bWantScreenRight*/ false, Worker->GetActorLocation(), FootLocation);
	PickSide(TEXT("LeftArm"), TEXT("RightArm"), /*bWantScreenRight*/ true,
		SpineBone + CameraRight * OfficeEquipTab::BagFallbackSideCm, RightArmLocation);
	PickSide(TEXT("LeftArm"), TEXT("RightArm"), /*bWantScreenRight*/ false,
		SpineBone - CameraRight * OfficeEquipTab::BagFallbackSideCm, LeftArmLocation);
	FVector LowerSpine;
	const FVector ChestCenter = Bone(TEXT("Spine1"), LowerSpine) ? FMath::Lerp(SpineBone, LowerSpine, 0.5f) : SpineBone;
	FVector ChestSide = (LeftArmLocation - SpineBone) * OfficeEquipTab::ChestSideAlpha;
	ChestSide.Z = 0.f;

	FVector WorldAnchors[OfficeEquipTab::PartCount];
	WorldAnchors[static_cast<int32>(EEmployeeEquipmentSlot::Head)] = HeadTop;
	WorldAnchors[static_cast<int32>(EEmployeeEquipmentSlot::Eyewear)] =
		FMath::Lerp(HeadBone, HeadTop, OfficeEquipTab::EyeHeightAlpha) + Forward * OfficeEquipTab::EyeForwardCm;
	// 의상 = 가슴 가운데에서 왼쪽 칸 쪽으로 조금 — 가방 앵커(오른쪽 어깨)와 겹치지 않게
	WorldAnchors[static_cast<int32>(EEmployeeEquipmentSlot::Outfit)] = ChestCenter + ChestSide + Forward * OfficeEquipTab::ChestForwardCm;
	// 손목 뼈보다 손바닥 쪽
	WorldAnchors[static_cast<int32>(EEmployeeEquipmentSlot::Hands)] = HandLocation - FVector::UpVector * OfficeEquipTab::HandDropCm;
	WorldAnchors[static_cast<int32>(EEmployeeEquipmentSlot::Feet)] = FootLocation;
	// 가방은 등 뒤라 정면에선 안 보인다 — 화면 오른쪽 어깨 끈 자리(목업 앵커와 같은 자리)
	WorldAnchors[static_cast<int32>(EEmployeeEquipmentSlot::Back)] = FMath::Lerp(SpineBone, RightArmLocation, OfficeEquipTab::BagShoulderAlpha);

	OutAbsolute.SetNum(OfficeEquipTab::PartCount);
	for (int32 Index = 0; Index < OfficeEquipTab::PartCount; ++Index)
	{
		if (!Project(WorldAnchors[Index], OutAbsolute[Index]))
		{
			return false;
		}
	}
	return Project(HeadTop + FVector::UpVector * 6.f, OutHeadTopAbsolute) && Project(SpineBone, OutCenterAbsolute);
}

void UOfficeEmployeePanelWidget::TickEquipmentWorld(float DeltaTime)
{
	const bool bTab = IsEquipmentTabShown() && IsActivated() && SelectedEmployeeId != INDEX_NONE;
	AOfficeworker* Worker = bTab ? FocusWorker.Get() : nullptr;

	if (BenchSlotRow)
	{
		const ESlateVisibility BenchWanted = (bTab && !FocusWorker.IsValid()) ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
		if (BenchSlotRow->GetVisibility() != BenchWanted) BenchSlotRow->SetVisibility(BenchWanted);
	}
	if (IsEquipDetailShown())
	{
		SyncEquipDetailHeight();
	}

	TArray<FVector2D> AnchorsAbs;
	FVector2D HeadTopAbs, CenterAbs;
	const bool bAnchors = Worker && WorldLayer && ComputeEquipAnchors(Worker, AnchorsAbs, HeadTopAbs, CenterAbs);
	const FGeometry LayerGeo = WorldLayer ? WorldLayer->GetCachedGeometry() : FGeometry();
	const FVector2D LayerSize = LayerGeo.GetLocalSize();
	const bool bLayout = bAnchors && LayerSize.X > 1.f && LayerSize.Y > 1.f;

	// 장착 순간 — 빛 고리(부위 앵커) + 머리 위 글자, 끝나면 칸 복귀
	if (bEquipMomentActive)
	{
		EquipMomentElapsed += DeltaTime;
		if (EquipMomentElapsed >= OfficeEquipTab::MomentDuration || !bTab)
		{
			EndEquipMoment();
		}
		else
		{
			const float Time = EquipMomentElapsed;
			const float FadeOut = FMath::Clamp((OfficeEquipTab::MomentDuration - Time) / OfficeEquipTab::MomentFadeOutTime, 0.f, 1.f);
			float FlashScale = 1.f;
			float FlashOpacity = 0.f;
			if (Time < OfficeEquipTab::FlashInTime)
			{
				const float In = Time / OfficeEquipTab::FlashInTime;
				FlashScale = FMath::Lerp(OfficeEquipTab::FlashStartScale, 1.f, OfficeEquipTab::EaseOutCubic(In));
				FlashOpacity = FMath::Min(1.f, In * 2.f);
			}
			else if (Time < OfficeEquipTab::FlashEndTime)
			{
				const float Out = (Time - OfficeEquipTab::FlashInTime) / (OfficeEquipTab::FlashEndTime - OfficeEquipTab::FlashInTime);
				FlashOpacity = 1.f - Out * Out;
			}
			if (EquipCalloutLines)
			{
				if (bLayout && FlashOpacity > 0.f)
				{
					const FVector2D FlashLocal = EquipCalloutLines->GetCachedGeometry().AbsoluteToLocal(AnchorsAbs[static_cast<int32>(EquipMomentPart)]);
					EquipCalloutLines->SetFlash(FlashLocal, FlashScale, FlashOpacity);
				}
				else
				{
					EquipCalloutLines->ClearFlash();
				}
			}
			if (EquipToast)
			{
				const float ToastIn = OfficeEquipTab::EaseOutCubic((Time - OfficeEquipTab::ToastDelay) / OfficeEquipTab::ToastInTime);
				EquipToast->SetRenderOpacity(ToastIn * FadeOut);
				EquipToast->SetRenderTranslation(FVector2D(0.f, OfficeEquipTab::ToastRisePx * (1.f - ToastIn)));
				if (bLayout)
				{
					if (UCanvasPanelSlot* ToastSlot = Cast<UCanvasPanelSlot>(EquipToast->Slot))
					{
						const FVector2D HeadLocal = LayerGeo.AbsoluteToLocal(HeadTopAbs);
						ToastSlot->SetPosition(FVector2D(HeadLocal.X, HeadLocal.Y - OfficeEquipTab::ToastAboveHeadPx));
					}
				}
			}
		}
	}

	const bool bCardOpen = IsEquipDetailShown() || IsEnhanceCardShown();
	const bool bShow = bLayout && !bCardOpen && !bEquipMomentActive && !bCameraFramingPending;
	if (!bShow)
	{
		if (bWorldSlotsShown) SetWorldSlotsShown(false);
		return;
	}

	TArray<FVector2D> AnchorLocal;
	AnchorLocal.SetNum(OfficeEquipTab::PartCount);
	for (int32 Index = 0; Index < OfficeEquipTab::PartCount; ++Index)
	{
		AnchorLocal[Index] = LayerGeo.AbsoluteToLocal(AnchorsAbs[Index]);
	}
	const float HeadTopY = LayerGeo.AbsoluteToLocal(HeadTopAbs).Y;
	const float CenterX = LayerGeo.AbsoluteToLocal(CenterAbs).X;
	const float TorsoY = AnchorLocal[static_cast<int32>(EEmployeeEquipmentSlot::Outfit)].Y;
	const float FootY = AnchorLocal[static_cast<int32>(EEmployeeEquipmentSlot::Feet)].Y;
	const float BodySpan = FMath::Max(FootY - HeadTopY, 1.f);
	const float ColumnGap = OfficeEquipTab::ColumnOffset * FMath::Max(1.f, LayerSize.X / OfficeEquipTab::DesignWidth);

	// 발밑 돌리기 고리 + 글자 (아래 행 칸 배치보다 먼저 — 겹치면 그 행을 내린다)
	FVector2D RingCenter = FVector2D::ZeroVector;
	FVector2D RingSize = FVector2D::ZeroVector;
	// 살펴보기로 세운 직원만 돌릴 수 있다(채용 입장 연출 중이면 살펴보기가 거절된다)
	const bool bRing = RotateRing && RotateCaption && RotateWorker.Get() == Worker && Inspect.GetWorker() == Worker
		&& ComputeRotateHint(Worker, LayerGeo, RingCenter, RingSize);
	const float CaptionTop = RingCenter.Y + RingSize.Y * 0.5f + OfficeEquipTab::RotateCaptionGap;
	const float CaptionBottom = CaptionTop + (bRing ? static_cast<float>(RotateCaption->GetDesiredSize().Y) : 0.f);
	const float RingReach = RingSize.X * 0.5f + OfficeEquipTab::RotateArrowReach;

	float Rows[3];
	Rows[1] = TorsoY;
	Rows[0] = FMath::Min(HeadTopY - BodySpan * OfficeEquipTab::RowOutsideRatio, Rows[1] - OfficeEquipTab::RowMinGap);
	Rows[2] = FMath::Max(FootY + BodySpan * OfficeEquipTab::RowOutsideRatio, Rows[1] + OfficeEquipTab::RowMinGap);
	// 아래 행은 호로 안쪽에 들어와 있다 — 그 행의 안쪽 가장자리로 고리와 겹치는지 본다
	if (bRing && ColumnGap * OfficeEquipTab::RowArc[2] - OfficeEquipTab::SlotFaceSize * 0.5f < RingReach + OfficeEquipTab::RotateSlotClearance)
	{
		Rows[2] = FMath::Max(Rows[2], CaptionBottom + OfficeEquipTab::SlotBoxSize * 0.5f + OfficeEquipTab::RotateSlotClearance);
	}
	Rows[0] = FMath::Max(Rows[0], OfficeEquipTab::RowTopMargin);
	Rows[2] = FMath::Min(Rows[2], LayerSize.Y - OfficeEquipTab::RowBottomMargin);

	if (bRing)
	{
		if (UCanvasPanelSlot* RingSlot = Cast<UCanvasPanelSlot>(RotateRing->Slot))
		{
			RingSlot->SetPosition(RingCenter);
			RingSlot->SetSize(RingSize);
		}
		if (UCanvasPanelSlot* CaptionSlot = Cast<UCanvasPanelSlot>(RotateCaption->Slot))
		{
			CaptionSlot->SetPosition(FVector2D(RingCenter.X, CaptionTop));
		}
		const float BodyHalf = FMath::Max(OfficeEquipTab::RotateBodyMinHalfWidth, BodySpan * OfficeEquipTab::RotateBodyHalfWidthRatio);
		FSlateRect Hit(FMath::Min(CenterX - BodyHalf, RingCenter.X - RingReach), HeadTopY - OfficeEquipTab::RotateHeadPad,
			FMath::Max(CenterX + BodyHalf, RingCenter.X + RingReach), FMath::Max(CaptionBottom, RingCenter.Y + RingSize.Y * 0.5f));
		// 빈칸 닫기 버튼 안쪽만 — 레일·패널 위 누름은 가로채지 않는다
		if (GapCloseButton)
		{
			const FGeometry& GapGeo = GapCloseButton->GetCachedGeometry();
			const FVector2D GapTopLeft = LayerGeo.AbsoluteToLocal(GapGeo.LocalToAbsolute(FVector2D::ZeroVector));
			const FVector2D GapBottomRight = LayerGeo.AbsoluteToLocal(GapGeo.LocalToAbsolute(GapGeo.GetLocalSize()));
			Hit = Hit.IntersectionWith(FSlateRect(GapTopLeft.X, GapTopLeft.Y, GapBottomRight.X, GapBottomRight.Y));
		}
		RotateHitRectLocal = Hit;
	}
	SetRotateHintShown(bRing);

	const FGeometry LinesGeo = EquipCalloutLines ? EquipCalloutLines->GetCachedGeometry() : FGeometry();
	const bool bLinesReady = EquipCalloutLines && LinesGeo.GetLocalSize().X > 1.f;
	auto ToLines = [&LayerGeo, &LinesGeo](const FVector2D& LayerPoint)
	{
		return LinesGeo.AbsoluteToLocal(LayerGeo.LocalToAbsolute(LayerPoint));
	};

	LastWorldSlotCenters.SetNum(OfficeEquipTab::PartCount);
	LastWorldAnchors = AnchorLocal;
	for (int32 Index = 0; Index < OfficeEquipTab::PartCount; ++Index)
	{
		const OfficeEquipTab::FPartLayout& Layout = OfficeEquipTab::PartLayouts[Index];
		const float Offset = ColumnGap * OfficeEquipTab::RowArc[Layout.Row];
		const float SlotX = CenterX + (Layout.bLeftColumn ? -Offset : Offset);
		const float SlotY = Rows[Layout.Row];
		LastWorldSlotCenters[Index] = FVector2D(SlotX, SlotY);
		if (WorldSlotBoxes.IsValidIndex(Index) && WorldSlotBoxes[Index])
		{
			// 칸 상자는 위 가운데 정렬(0.5, 0) — 칸 가운데가 행 높이에 오도록 상자 반만큼 올린다
			if (UCanvasPanelSlot* BoxSlot = Cast<UCanvasPanelSlot>(WorldSlotBoxes[Index]->Slot))
			{
				BoxSlot->SetPosition(FVector2D(SlotX, SlotY - OfficeEquipTab::SlotBoxSize * 0.5f));
			}
		}
	}

	// 선은 고른 부위 하나만 — 「전체」면 없다. 칸 안쪽 가장자리 → 앵커 22 앞, 조절점 = 가로 가운데 · 세로 15%
	LastCurvePoints.Reset();
	if (bLinesReady && !bEquipAllParts)
	{
		const int32 Index = static_cast<int32>(EquipSelectedPart);
		const OfficeEquipTab::FPartLayout& Layout = OfficeEquipTab::PartLayouts[Index];
		const FVector2D SlotCenter = LastWorldSlotCenters[Index];
		const float Toward = Layout.bLeftColumn ? 1.f : -1.f;
		const FVector2D Start(SlotCenter.X + Toward * (OfficeEquipTab::SlotFaceSize * 0.5f + OfficeEquipTab::LineGapFromSlot), SlotCenter.Y);
		const FVector2D Anchor = AnchorLocal[Index];
		const FVector2D Back = Start - Anchor;
		const double BackLength = Back.Size();
		const FVector2D End = BackLength > OfficeEquipTab::CurveStopShort ? Anchor + Back / BackLength * OfficeEquipTab::CurveStopShort : Anchor;
		const FVector2D Control((Start.X + End.X) * 0.5f, Start.Y + (End.Y - Start.Y) * OfficeEquipTab::CurveControlRise);
		LastCurvePoints = { Start, Control, End, Anchor };
		FEquipmentCallout Callout;
		Callout.Start = ToLines(Start);
		Callout.Control = ToLines(Control);
		Callout.End = ToLines(End);
		Callout.Anchor = ToLines(Anchor);
		EquipCalloutLines->SetCallout(Callout);
	}
	else if (EquipCalloutLines)
	{
		EquipCalloutLines->ClearCallout();
	}
	if (!bWorldSlotsShown)
	{
		SetWorldSlotsShown(true);
	}
}

// ===== 직원 돌리기 (장비 탭 — 몸·발밑 고리를 좌우로 끌면 요만 돈다) =====

bool UOfficeEmployeePanelWidget::ComputeRotateHint(AOfficeworker* Worker, const FGeometry& LayerGeo, FVector2D& OutCenter, FVector2D& OutSize) const
{
	APlayerController* PC = GetOwningPlayer();
	if (!Worker || !PC || !PC->PlayerCameraManager)
	{
		return false;
	}
	const UCapsuleComponent* Capsule = Worker->GetCapsuleComponent();
	const FVector Floor = Worker->GetActorLocation() - FVector::UpVector * (Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.f);
	const FVector CameraRight = FRotationMatrix(PC->PlayerCameraManager->GetCameraRotation()).GetUnitAxis(EAxis::Y);
	const FGeometry ViewportGeo = UWidgetLayoutLibrary::GetViewportWidgetGeometry(const_cast<UOfficeEmployeePanelWidget*>(this));
	const FVector FloorPoints[3] = {
		Floor, Floor - CameraRight * OfficeEquipTab::RotateRingRadiusCm, Floor + CameraRight * OfficeEquipTab::RotateRingRadiusCm };
	FVector2D LayerPoints[3];
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FVector2D ViewportPos;
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, FloorPoints[Index], ViewportPos, true))
		{
			return false;
		}
		LayerPoints[Index] = LayerGeo.AbsoluteToLocal(ViewportGeo.LocalToAbsolute(ViewportPos));
	}
	const float Width = static_cast<float>(FVector2D::Distance(LayerPoints[1], LayerPoints[2]));
	if (Width < 1.f)
	{
		return false;
	}
	OutCenter = LayerPoints[0];
	OutSize = FVector2D(Width, Width / OfficeEquipTab::RotateRingFlatten);
	return true;
}

bool UOfficeEmployeePanelWidget::CanRotateWorker() const
{
	const AOfficeworker* Worker = RotateWorker.Get();
	return bRotateHintPlaced && IsActivated() && !IsCloseRequested() && Worker
		&& Inspect.GetWorker() == Worker && FocusWorker.Get() == Worker && IsEquipmentTabShown();
}

bool UOfficeEmployeePanelWidget::IsRotatePointer(const FPointerEvent& PointerEvent) const
{
	return RotatePointerIndex != INDEX_NONE && RotatePointerIndex == static_cast<int32>(PointerEvent.GetPointerIndex())
		&& RotateUserIndex == static_cast<int32>(PointerEvent.GetUserIndex()) && bRotateTouch == PointerEvent.IsTouchEvent();
}

FReply UOfficeEmployeePanelWidget::BeginRotateDrag(const FPointerEvent& PointerEvent)
{
	if (!CanRotateWorker() || !WorldLayer)
	{
		return FReply::Unhandled();
	}
	const FVector2D Local = WorldLayer->GetCachedGeometry().AbsoluteToLocal(PointerEvent.GetScreenSpacePosition());
	if (!RotateHitRectLocal.ContainsPoint(Local))
	{
		return FReply::Unhandled();
	}
	// 한 손가락으로 돌리는 중 같은 영역에 닿은 다른 손가락은 삼킨다 — 빈칸 닫기로 새지 않게
	if (RotatePointerIndex != INDEX_NONE)
	{
		return FReply::Handled();
	}
	const TSharedPtr<SWidget> CaptureWidget = GetCachedWidget();
	if (!CaptureWidget.IsValid())
	{
		return FReply::Unhandled();
	}
	RotatePointerIndex = static_cast<int32>(PointerEvent.GetPointerIndex());
	RotateUserIndex = static_cast<int32>(PointerEvent.GetUserIndex());
	bRotateTouch = PointerEvent.IsTouchEvent();
	bRotateDragging = false;
	RotatePressPosition = PointerEvent.GetScreenSpacePosition();
	RotateLastPosition = RotatePressPosition;
	return FReply::Handled().CaptureMouse(CaptureWidget.ToSharedRef());
}

FReply UOfficeEmployeePanelWidget::MoveRotateDrag(const FPointerEvent& PointerEvent)
{
	if (!IsRotatePointer(PointerEvent))
	{
		return FReply::Unhandled();
	}
	AOfficeworker* Worker = RotateWorker.Get();
	if (!CanRotateWorker() || !Worker || !GapCloseButton)
	{
		CancelRotateDrag();
		return FReply::Handled().ReleaseMouseCapture();
	}
	const FVector2D Current = PointerEvent.GetScreenSpacePosition();
	if (!bRotateDragging)
	{
		// 끌기 문턱 전에는 탭 후보 — 문턱까지 움직인 거리도 넘는 순간 한꺼번에 돌린다
		if (!FSlateApplication::Get().HasTraveledFarEnoughToTriggerDrag(PointerEvent, RotatePressPosition))
		{
			return FReply::Handled();
		}
		bRotateDragging = true;
		if (RotateRing) RotateRing->SetDragging(true);
	}
	// 빈칸 폭 = 반 바퀴, 세로 이동은 버린다. 로컬 단위라 DPI·화면비가 달라도 같은 비율
	const FGeometry& GapGeo = GapCloseButton->GetCachedGeometry();
	const float GapWidth = FMath::Max(1.f, static_cast<float>(GapGeo.GetLocalSize().X));
	const float DeltaX = static_cast<float>(GapGeo.AbsoluteToLocal(Current).X - GapGeo.AbsoluteToLocal(RotateLastPosition).X);
	RotateLastPosition = Current;
	// 오른쪽으로 끌면 몸 앞면이 오른쪽으로 — 요 감소
	RotateYawOffset = FMath::UnwindDegrees(RotateYawOffset - OfficeEquipTab::RotateDegreesPerGapWidth * DeltaX / GapWidth);
	Worker->SetActorRotation(FRotator(0.f, RotateBaseYaw + RotateYawOffset, 0.f));
	return FReply::Handled();
}

FReply UOfficeEmployeePanelWidget::EndRotateDrag(const FPointerEvent& PointerEvent)
{
	if (!IsRotatePointer(PointerEvent))
	{
		return FReply::Unhandled();
	}
	RotatePointerIndex = RotateUserIndex = INDEX_NONE;
	bRotateDragging = false;
	if (RotateRing) RotateRing->SetDragging(false);
	// 몸·고리는 돌리기 손잡이라 짧은 탭도 닫지 않는다 — 돌리려다 덜 끈 손가락이 패널을 닫지 않게
	return FReply::Handled().ReleaseMouseCapture();
}

void UOfficeEmployeePanelWidget::CancelRotateDrag()
{
	const int32 PointerIndex = RotatePointerIndex;
	const int32 UserIndex = RotateUserIndex;
	RotatePointerIndex = RotateUserIndex = INDEX_NONE;
	bRotateDragging = false;
	if (RotateRing) RotateRing->SetDragging(false);
	if (PointerIndex == INDEX_NONE || !FSlateApplication::IsInitialized())
	{
		return;
	}
	const TSharedPtr<FSlateUser> SlateUser = FSlateApplication::Get().GetUser(UserIndex);
	if (SlateUser && SlateUser->DoesWidgetHaveCapture(GetCachedWidget(), static_cast<uint32>(PointerIndex)))
	{
		SlateUser->ReleaseCapture(static_cast<uint32>(PointerIndex));
	}
}

void UOfficeEmployeePanelWidget::ResetWorkerRotation()
{
	CancelRotateDrag();
	AOfficeworker* Worker = RotateWorker.Get();
	// 살펴보기가 끝난(자리로 돌아간) 직원은 건드리지 않는다
	if (Worker && Inspect.GetWorker() == Worker && !FMath::IsNearlyZero(RotateYawOffset))
	{
		Worker->SetActorRotation(FRotator(0.f, RotateBaseYaw, 0.f));
	}
	RotateYawOffset = 0.f;
}

void UOfficeEmployeePanelWidget::CaptureRotateBase(AOfficeworker* Worker)
{
	CancelRotateDrag();
	RotateYawOffset = 0.f;
	RotateWorker = (Worker && Inspect.GetWorker() == Worker) ? Worker : nullptr;
	RotateBaseYaw = RotateWorker.IsValid() ? static_cast<float>(Worker->GetActorRotation().Yaw) : 0.f;
}

FReply UOfficeEmployeePanelWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.IsTouchEvent() || InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const FReply Reply = BeginRotateDrag(InMouseEvent);
		if (Reply.IsEventHandled())
		{
			return Reply;
		}
	}
	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UOfficeEmployeePanelWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FReply Reply = MoveRotateDrag(InMouseEvent);
	return Reply.IsEventHandled() ? Reply : Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UOfficeEmployeePanelWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const FReply Reply = EndRotateDrag(InMouseEvent);
		if (Reply.IsEventHandled())
		{
			return Reply;
		}
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UOfficeEmployeePanelWidget::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	const FReply Reply = MoveRotateDrag(InGestureEvent);
	return Reply.IsEventHandled() ? Reply : Super::NativeOnTouchMoved(InGeometry, InGestureEvent);
}

FReply UOfficeEmployeePanelWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	const FReply Reply = EndRotateDrag(InGestureEvent);
	return Reply.IsEventHandled() ? Reply : Super::NativeOnTouchEnded(InGeometry, InGestureEvent);
}

void UOfficeEmployeePanelWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	if (RotateUserIndex == static_cast<int32>(CaptureLostEvent.UserIndex) && RotatePointerIndex == static_cast<int32>(CaptureLostEvent.PointerIndex))
	{
		// 각도는 그대로 두고 잡은 포인터만 놓는다
		CancelRotateDrag();
	}
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

FString UOfficeEmployeePanelWidget::TestGetRotateState() const
{
	auto RectJson = [](const UWidget* Target) -> FString
	{
		if (!Target || !Target->IsVisible())
		{
			return TEXT("null");
		}
		const FGeometry& Geo = Target->GetCachedGeometry();
		const FVector2D TopLeft = Geo.LocalToAbsolute(FVector2D::ZeroVector);
		const FVector2D BottomRight = Geo.LocalToAbsolute(Geo.GetLocalSize());
		return FString::Printf(TEXT("[%.1f,%.1f,%.1f,%.1f]"), TopLeft.X, TopLeft.Y, BottomRight.X, BottomRight.Y);
	};
	FString HitJson = TEXT("null");
	if (WorldLayer && bRotateHintPlaced)
	{
		const FGeometry& LayerGeo = WorldLayer->GetCachedGeometry();
		const FVector2D TopLeft = LayerGeo.LocalToAbsolute(FVector2D(RotateHitRectLocal.Left, RotateHitRectLocal.Top));
		const FVector2D BottomRight = LayerGeo.LocalToAbsolute(FVector2D(RotateHitRectLocal.Right, RotateHitRectLocal.Bottom));
		HitJson = FString::Printf(TEXT("[%.1f,%.1f,%.1f,%.1f]"), TopLeft.X, TopLeft.Y, BottomRight.X, BottomRight.Y);
	}
	TArray<FString> SlotRects;
	for (const TObjectPtr<UWidget>& Box : WorldSlotBoxes)
	{
		SlotRects.Add(RectJson(Box));
	}
	const AOfficeworker* Worker = RotateWorker.Get();
	const APlayerController* PC = GetOwningPlayer();
	const FVector CameraLocation = (PC && PC->PlayerCameraManager) ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	const APawn* CameraPawn = GetOwningPlayerPawn();
	const FVector PawnLocation = CameraPawn ? CameraPawn->GetActorLocation() : FVector::ZeroVector;
	const float LayerScale = WorldLayer ? static_cast<float>(WorldLayer->GetCachedGeometry().Scale) : 1.f;
	return FString::Printf(TEXT("{\"hint\":%s,\"can_rotate\":%s,\"dragging\":%s,\"pointer\":%d,\"offset\":%.2f,\"base_yaw\":%.2f,\"actor_yaw\":%.2f,")
		TEXT("\"ring\":%s,\"caption\":%s,\"hit\":%s,\"gap\":%s,\"slots\":[%s],\"layer_scale\":%.4f,")
		TEXT("\"camera\":[%.2f,%.2f,%.2f],\"pawn\":[%.2f,%.2f,%.2f],\"ring_dragging\":%s}"),
		bRotateHintPlaced ? TEXT("true") : TEXT("false"), CanRotateWorker() ? TEXT("true") : TEXT("false"),
		bRotateDragging ? TEXT("true") : TEXT("false"), RotatePointerIndex, RotateYawOffset, RotateBaseYaw,
		Worker ? Worker->GetActorRotation().Yaw : 0.0,
		*RectJson(RotateRing), *RectJson(RotateCaption), *HitJson, *RectJson(GapCloseButton), *FString::Join(SlotRects, TEXT(",")), LayerScale,
		CameraLocation.X, CameraLocation.Y, CameraLocation.Z, PawnLocation.X, PawnLocation.Y, PawnLocation.Z,
		(RotateRing && RotateRing->IsDragging()) ? TEXT("true") : TEXT("false"));
}

// ===== 상세 카드 =====

void UOfficeEmployeePanelWidget::HandleEquipItemPicked(const FGuid& InstanceId)
{
	if (bEquipCommitting)
	{
		return;
	}
	if (!InstanceId.IsValid())
	{
		CloseEquipDetail();
		return;
	}
	ShowEquipDetail(InstanceId);
}

bool UOfficeEmployeePanelWidget::IsEquipDetailShown() const
{
	return EquipDetailFrame && CardHost && EquipDetailFrame->GetParent() == CardHost;
}

void UOfficeEmployeePanelWidget::ShowEquipDetail(const FGuid& InstanceId)
{
	UGameInstance* GI = GetGameInstance();
	UEmployeeEquipmentManagerSubsystem* Equipment = GI ? GI->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
	const UTableManagerSubsystem* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Equipment || !Tables || !CardHost || SelectedEmployeeId == INDEX_NONE)
	{
		return;
	}
	FEquipmentDetailContent Content;
	if (!CGEquipmentDetail::BuildContent(*Equipment, *Tables, EmployeeManager, SelectedEmployeeId, InstanceId, Content))
	{
		CloseEquipDetail();
		return;
	}
	const FEmployeeEquipmentInstance* Item = Equipment->FindInstance(InstanceId);
	const FEmployeeEquipmentDefinitionRow* Definition = Item ? Tables->FindEmployeeEquipmentDefinition(Item->DefinitionId) : nullptr;
	const FEmployeeEquipmentEconomyRow* Economy = Tables->GetEmployeeEquipmentEconomy();
	if (Item && Economy && Item->EnhanceLevel < Economy->MaxEnhanceLevel)
	{
		Content.Secondary = LOCTEXT("EquipEnhance", "장비 강화");
	}

	if (!EquipDetailCard)
	{
		const TSubclassOf<UUserWidget> CardClass = Tables->GetWidgetClass(EWidgetType::EquipmentDetailWindow);
		if (!CardClass)
		{
			UE_LOG(LogTemp, Error, TEXT("[OfficeEmployeePanel] EquipmentDetailWindow 클래스 미등록 — DT_WidgetClass 확인"));
			return;
		}
		EquipDetailCard = Cast<UEquipmentDetailWindowWidget>(CreateWidget(GetOwningPlayer(), CardClass.Get()));
		if (!EquipDetailCard || !EquipDetailCardBox || !EquipDetailFrame)
		{
			return;
		}
		// 구역 제목 「장비」·자체 닫기·자체 판은 직원창 칼럼용 — 판·여백·X 는 패널 틀(EquipDetailFrame)이 낸다(이 인스턴스만 접음)
		for (const FName& HiddenName : EquipDetailCollapsedWidgets)
		{
			if (UWidget* Hidden = EquipDetailCard->GetWidgetFromName(HiddenName))
			{
				Hidden->SetVisibility(ESlateVisibility::Collapsed);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[OfficeEmployeePanel] 상세 창에 %s 가 없습니다 — 접기 생략"), *HiddenName.ToString());
			}
		}
		if (UWidget* Header = EquipDetailHeaderWidget.IsNone() ? nullptr : EquipDetailCard->GetWidgetFromName(EquipDetailHeaderWidget))
		{
			const auto Reserve = [this](FMargin InPad) { InPad.Right += EquipDetailHeaderReserve; return InPad; };
			if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(Header->Slot)) VSlot->SetPadding(Reserve(VSlot->GetPadding()));
			else if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(Header->Slot)) HSlot->SetPadding(Reserve(HSlot->GetPadding()));
			else if (UOverlaySlot* OSlot = Cast<UOverlaySlot>(Header->Slot)) OSlot->SetPadding(Reserve(OSlot->GetPadding()));
			else UE_LOG(LogTemp, Warning, TEXT("[OfficeEmployeePanel] 상세 창 머리 줄 %s 슬롯에 여백을 줄 수 없습니다"), *EquipDetailHeaderWidget.ToString());
		}
		EquipDetailCardBox->SetHeightOverride(OfficeEquipTab::DetailFallbackHeight);
		EquipDetailCardBox->AddChild(EquipDetailCard);
	}

	// 카드 자리는 한 번에 하나 — ★ 강화 카드가 떠 있으면 내린다
	CloseEnhanceCard();
	for (int32 Index = CardHost->GetChildrenCount() - 1; Index >= 0; --Index)
	{
		UWidget* Child = CardHost->GetChildAt(Index);
		if (Child && Child != EquipDetailFrame)
		{
			Child->RemoveFromParent();
		}
	}
	if (EquipDetailFrame->GetParent() != CardHost)
	{
		if (UOverlaySlot* CardSlot = CardHost->AddChildToOverlay(EquipDetailFrame))
		{
			CardSlot->SetHorizontalAlignment(HAlign_Fill);
			CardSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	// 재사용 위젯의 델리게이트는 매 표시마다 다시 묶는다
	EquipDetailCard->OnPrimary.BindUObject(this, &ThisClass::HandleEquipDetailPrimary);
	EquipDetailCard->OnSecondary.BindUObject(this, &ThisClass::HandleEquipDetailSecondary);
	EquipDetailCard->OnClose.BindWeakLambda(this, [this]() { CloseEquipDetail(); });
	EquipDetailCard->Configure(Content);
	EquipDetailInstanceId = InstanceId;
	SyncEquipDetailHeight();

	// 미리 입혀 보기 — 이미 이 직원이 입은 장비면 지금 모습이 곧 결과다
	AFunnyOfficeworker* Worker = Cast<AFunnyOfficeworker>(FocusWorker.Get());
	if (EquipPreviewWorker.IsValid() && EquipPreviewWorker.Get() != Worker)
	{
		ClearEquipPreview();
	}
	const bool bEquippedHere = Equipment->GetInstanceEmployee(InstanceId) == SelectedEmployeeId;
	if (Worker && Definition && !bEquippedHere)
	{
		if (Worker->PreviewEquipmentAppearance(Definition->EquipmentSlot, EEquipmentAppearanceMode::Override, Definition->AppearanceId))
		{
			EquipPreviewWorker = Worker;
		}
	}
	else
	{
		ClearEquipPreview();
	}
	SetWorldSlotsShown(false);
}

void UOfficeEmployeePanelWidget::SyncEquipDetailHeight()
{
	if (!EquipDetailCard || !EquipDetailCardBox)
	{
		return;
	}
	// 카드 루트가 캔버스라 카드 자체의 desired 는 0 이다 — 안쪽 DetailDock 의 desired 높이로 상자를 맞춘다
	const UWidget* Dock = EquipDetailCard->GetWidgetFromName(TEXT("DetailDock"));
	float Height = Dock ? Dock->GetDesiredSize().Y : 0.f;
	if (Height < 1.f)
	{
		Height = OfficeEquipTab::DetailFallbackHeight;
	}
	// 틀(판) 높이 = 상자 + 상자 슬롯 위아래 여백(공용 창 안쪽 여백을 덜어 내느라 음수일 수 있다)
	const UOverlaySlot* BoxSlot = Cast<UOverlaySlot>(EquipDetailCardBox->Slot);
	const float FrameChrome = BoxSlot ? static_cast<float>(BoxSlot->GetPadding().Top + BoxSlot->GetPadding().Bottom) : 0.f;
	const float Available = WorldLayer ? WorldLayer->GetCachedGeometry().GetLocalSize().Y - OfficeEquipTab::DetailVerticalMargin
		- FrameChrome : 0.f;
	if (Available > 1.f)
	{
		Height = FMath::Min(Height, Available);
	}
	if (!FMath::IsNearlyEqual(EquipDetailCardBox->GetHeightOverride(), Height, 0.5f))
	{
		EquipDetailCardBox->SetHeightOverride(Height);
	}
}

bool UOfficeEmployeePanelWidget::CloseEquipDetail()
{
	const bool bWasShown = IsEquipDetailShown();
	EquipDetailInstanceId.Invalidate();
	if (EquipDetailCard)
	{
		EquipDetailCard->OnPrimary.Unbind();
		EquipDetailCard->OnSecondary.Unbind();
		EquipDetailCard->OnClose.Unbind();
	}
	if (EquipDetailFrame)
	{
		EquipDetailFrame->RemoveFromParent();
	}
	ClearEquipPreview();
	if (bWasShown && EquipmentList)
	{
		EquipmentList->ClearSelection();
	}
	return bWasShown;
}

void UOfficeEmployeePanelWidget::HandleEquipDetailCloseClicked()
{
	CloseEquipDetail();
}

void UOfficeEmployeePanelWidget::ClearEquipPreview()
{
	if (AFunnyOfficeworker* Worker = EquipPreviewWorker.Get())
	{
		Worker->ClearEquipmentAppearancePreview();
	}
	EquipPreviewWorker.Reset();
}

void UOfficeEmployeePanelWidget::HandleEquipDetailPrimary()
{
	UGameInstance* GI = GetGameInstance();
	UEmployeeEquipmentManagerSubsystem* Equipment = GI ? GI->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
	const UTableManagerSubsystem* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Equipment || !Tables || !EmployeeManager || !EmployeeManager->FindEmployee(SelectedEmployeeId))
	{
		return;
	}
	const FGuid InstanceId = EquipDetailInstanceId;
	const FEmployeeEquipmentInstance* Item = Equipment->FindInstance(InstanceId);
	const FEmployeeEquipmentDefinitionRow* Definition = Item ? Tables->FindEmployeeEquipmentDefinition(Item->DefinitionId) : nullptr;
	if (!Definition)
	{
		return;
	}
	// 다른 직원이 낀 장비는 한 번 묻고 가져온다 ― 그 직원 칸이 말없이 비지 않게
	const int32 Holder = Equipment->GetInstanceEmployee(InstanceId);
	if (Holder != INDEX_NONE && Holder != SelectedEmployeeId && !bTransferConfirmed)
	{
		const int32 TargetEmployeeId = SelectedEmployeeId;
		CGEquipmentDetail::ShowTransferConfirm(*GI, Holder, TargetEmployeeId, *Definition,
			FSimpleDelegate::CreateWeakLambda(this, [this, InstanceId, TargetEmployeeId]()
		{
			if (SelectedEmployeeId != TargetEmployeeId || EquipDetailInstanceId != InstanceId)
			{
				return;
			}
			TGuardValue<bool> Confirmed(bTransferConfirmed, true);
			HandleEquipDetailPrimary();
		}));
		return;
	}
	const int32 ItemLevel = Item->EnhanceLevel;

	// 장착/해제 둘 다 SetSlot — 해제는 InstanceId 를 비워 보낸다 (직원창 HandleInlineDetailPrimary 와 같음)
	const bool bEquippedHere = Holder == SelectedEmployeeId;
	FEmployeeEquipmentRequest Request = Equipment->MakeRequest(EEquipmentCommand::SetSlot);
	Request.EmployeeId = SelectedEmployeeId;
	Request.EquipmentSlot = Definition->EquipmentSlot;
	if (!bEquippedHere)
	{
		Request.InstanceId = InstanceId;
		Request.ExpectedSourceEmployeeId = Equipment->GetInstanceEmployee(InstanceId);
	}
	FEmployeeEquipmentResult Result;
	{
		TGuardValue<bool> CommitGuard(bEquipCommitting, true);
		Result = Equipment->Execute(Request);
	}
	if (!Result.bCommitted)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeEmployeePanel] 장착 실패 Employee=%d Error=%s"), SelectedEmployeeId, *Result.ErrorCode.ToString());
		ShowEquipDetail(InstanceId);
		return;
	}
	const FText Title = FText::Format(LOCTEXT("EquipMomentTitle", "{0} 장착"), Definition->DisplayName);
	const FText Effect = OfficeEquipTab::PrimaryEffectLine(*Tables, *Definition, ItemLevel);
	CloseEquipDetail();
	if (!bEquippedHere)
	{
		BeginEquipMoment(Definition->EquipmentSlot, Title, Effect);
	}
}

void UOfficeEmployeePanelWidget::HandleEquipDetailSecondary()
{
	ShowEquipEnhanceDialog(EEquipmentMaterialMode::Core);
}

// ===== 장비 강화 (워크스페이스 ShowEnhance 흐름) =====

void UOfficeEmployeePanelWidget::ShowEquipEnhanceDialog(EEquipmentMaterialMode Mode)
{
	UGameInstance* GI = GetGameInstance();
	UEmployeeEquipmentManagerSubsystem* Equipment = GI ? GI->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
	const UTableManagerSubsystem* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const UItemInventoryManager* Items = GI ? GI->GetSubsystem<UItemInventoryManager>() : nullptr;
	const FGuid InstanceId = EquipDetailInstanceId.IsValid() ? EquipDetailInstanceId : EquipDialogInstanceId;
	const FEmployeeEquipmentInstance* Item = Equipment ? Equipment->FindInstance(InstanceId) : nullptr;
	const FEmployeeEquipmentDefinitionRow* Definition = Item && Tables ? Tables->FindEmployeeEquipmentDefinition(Item->DefinitionId) : nullptr;
	const FEmployeeEquipmentEconomyRow* Economy = Tables ? Tables->GetEmployeeEquipmentEconomy() : nullptr;
	if (!Definition || !Economy)
	{
		return;
	}
	EquipDialogInstanceId = InstanceId;
	EquipPendingRequest = Equipment->MakeRequest(EEquipmentCommand::Enhance);
	EquipPendingRequest.InstanceId = Item->InstanceId;
	EquipPendingRequest.ExpectedLevel = Item->EnhanceLevel;
	EquipPendingRequest.MaterialMode = Mode;
	int32 CoreCost = 0;
	int32 DuplicateCount = 0;
	const bool bQuoted = Equipment->QuoteEnhance(InstanceId, Mode, CoreCost, DuplicateCount);
	if (Mode == EEquipmentMaterialMode::Duplicate)
	{
		TArray<FGuid> Eligible;
		for (const TPair<FGuid, FEmployeeEquipmentInstance>& Pair : Equipment->GetState().Instances)
		{
			if (Pair.Key != InstanceId && Pair.Value.DefinitionId == Item->DefinitionId && Pair.Value.EnhanceLevel == 0
				&& !Pair.Value.bLocked && Equipment->GetInstanceEmployee(Pair.Key) < 0)
			{
				Eligible.Add(Pair.Key);
			}
		}
		Eligible.Sort([](const FGuid& A, const FGuid& B) { return A.ToString() < B.ToString(); });
		for (int32 Index = 0; Index < FMath::Min(DuplicateCount, Eligible.Num()); ++Index)
		{
			EquipPendingRequest.InstanceIds.Add(Eligible[Index]);
		}
	}

	FEmployeeEquipmentDialogView View;
	View.Title = LOCTEXT("EquipmentEnhanceTitle", "장비 강화");
	View.bShowCurrent = false;
	View.SelectedGrade = Definition->Grade;
	View.SelectedVisualGrade = CGEquipmentIcon::VisualGrade(Tables, *Definition);
	const FEmployeeEquipmentGradeRow* Grade = Tables->FindEmployeeEquipmentGrade(Definition->Grade);
	const FEmployeeEquipmentSlotRow* PartRow = Tables->FindEmployeeEquipmentSlot(Definition->EquipmentSlot);
	View.SelectedHeader = FText::Format(LOCTEXT("EnhanceHeader", "{0} · {1}\n{2} +{3}"),
		Grade ? Grade->DisplayName : FText::GetEmpty(), PartRow ? PartRow->DisplayName : FText::GetEmpty(),
		Definition->DisplayName, FText::AsNumber(Item->EnhanceLevel));
	if (!Definition->Icon.IsNull())
	{
		View.SelectedIcon = Definition->Icon.LoadSynchronous();
	}
	const FEquipmentEffectLoadout Before = FEmployeeEquipmentEffectEvaluator::BuildItemEffects(
		*Definition, Item->EnhanceLevel, Tables->GetEmployeeEquipmentEffects(), *Economy);
	const FEquipmentEffectLoadout After = FEmployeeEquipmentEffectEvaluator::BuildItemEffects(
		*Definition, FMath::Min(Item->EnhanceLevel + 1, Economy->MaxEnhanceLevel), Tables->GetEmployeeEquipmentEffects(), *Economy);
	TArray<FName> EffectKeys;
	Before.BasicValues.GetKeys(EffectKeys);
	EffectKeys.Sort(FNameLexicalLess());
	for (const FName Key : EffectKeys)
	{
		if (const FEmployeeEquipmentEffectRow* EffectRow = Tables->FindEmployeeEquipmentEffect(Key))
		{
			FEquipmentDeltaRow& Row = View.EffectRows.AddDefaulted_GetRef();
			Row.Name = EffectRow->DisplayName;
			Row.After = FText::FromString(CGEquipmentDetail::FormatEffectValue(*EffectRow, Before.BasicValues[Key]));
		}
	}
	if (const FEmployeeEquipmentEffectRow* Primary = Tables->FindEmployeeEquipmentEffect(Definition->PrimaryEffectId))
	{
		const float BeforeValue = Before.BasicValues.FindRef(Definition->PrimaryEffectId);
		const float AfterValue = After.BasicValues.FindRef(Definition->PrimaryEffectId);
		const int32 Digits = FMath::Max(CGEquipmentDetail::FractionDigits(CGEquipmentDetail::DisplayValue(*Primary, BeforeValue)),
			CGEquipmentDetail::FractionDigits(CGEquipmentDetail::DisplayValue(*Primary, AfterValue)));
		View.Comparison = FText::Format(LOCTEXT("EnhanceComparison", "{0} {1} → {2}\n주옵션 효과만 강화됩니다. 보조옵션·고유효과는 유지됩니다."), Primary->DisplayName,
			FText::FromString(CGEquipmentDetail::FormatEffectValue(*Primary, BeforeValue, Digits)),
			FText::FromString(CGEquipmentDetail::FormatEffectValue(*Primary, AfterValue, Digits)));
	}
	const int32 OwnedCores = Items ? Items->GetItemCount(EItemType::EmployeeEquipmentCore) : 0;
	View.Notice = Mode == EEquipmentMaterialMode::Core
		? FText::Format(LOCTEXT("CoreCost", "코어 {0}개 / 보유 {1}개 · 성공률 100%"), FText::AsNumber(CoreCost), FText::AsNumber(OwnedCores))
		: FText::Format(LOCTEXT("DuplicateCost", "동일 장비 +0 {0}개 / 선택 {1}개 · 성공률 100%"), FText::AsNumber(DuplicateCount), FText::AsNumber(EquipPendingRequest.InstanceIds.Num()));
	const bool bMaxed = Item->EnhanceLevel >= Economy->MaxEnhanceLevel;
	View.Primary = bMaxed ? LOCTEXT("MaxEnhance", "최대 강화") : LOCTEXT("EnhanceCommit", "강화하기");
	const bool bHaveMaterials = Mode == EEquipmentMaterialMode::Core
		? OwnedCores >= CoreCost
		: EquipPendingRequest.InstanceIds.Num() == DuplicateCount;
	View.bPrimaryEnabled = bQuoted && bHaveMaterials && !bMaxed;
	View.Secondary = Mode == EEquipmentMaterialMode::Core ? LOCTEXT("UseDuplicate", "동일 장비 사용") : LOCTEXT("UseCore", "코어 사용");
	View.Tertiary = LOCTEXT("Cancel", "취소");

	if (!EquipDialog)
	{
		const TSubclassOf<UUserWidget> DialogClass = Tables->GetWidgetClass(EWidgetType::EmployeeEquipmentDialog);
		if (!DialogClass)
		{
			UE_LOG(LogTemp, Error, TEXT("[OfficeEmployeePanel] EmployeeEquipmentDialog 클래스 미등록 — DT_WidgetClass 확인"));
			return;
		}
		EquipDialog = Cast<UEmployeeEquipmentDialogWidget>(CreateWidget(GetOwningPlayer(), DialogClass.Get()));
		if (!EquipDialog)
		{
			return;
		}
	}
	if (!EquipDialog->IsInViewport())
	{
		// 워크스페이스 다이얼로그와 같은 층 — 패널(스택) 위, 확인 다이얼로그(10000) 아래
		EquipDialog->AddToViewport(120);
	}
	EquipDialog->OnPrimary.Unbind();
	EquipDialog->OnSecondary.Unbind();
	EquipDialog->OnTertiary.Unbind();
	EquipDialog->OnExtra.Unbind();
	EquipDialog->OnItemClicked.Unbind();
	EquipDialog->OnSelectionChanged.Unbind();
	EquipDialog->OnClose.BindWeakLambda(this, [this]() { CloseEquipDialog(); });
	EquipDialog->OnPrimary.BindUObject(this, &ThisClass::ExecuteEquipEnhance);
	EquipDialog->OnSecondary.BindUObject(this, &ThisClass::ShowEquipEnhanceDialog,
		Mode == EEquipmentMaterialMode::Core ? EEquipmentMaterialMode::Duplicate : EEquipmentMaterialMode::Core);
	EquipDialog->OnTertiary.BindWeakLambda(this, [this]() { CloseEquipDialog(); });
	EquipDialog->Configure(View);
	EquipDialog->SetKeyboardFocus();
}

void UOfficeEmployeePanelWidget::ExecuteEquipEnhance()
{
	UGameInstance* GI = GetGameInstance();
	UEmployeeEquipmentManagerSubsystem* Equipment = GI ? GI->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
	if (!Equipment || !EquipDialog)
	{
		return;
	}
	EquipDialog->SetPrimaryEnabled(false);
	const FEmployeeEquipmentResult Result = Equipment->Execute(EquipPendingRequest);
	if (!Result.bCommitted)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeEmployeePanel] 장비 강화 실패 Error=%s"), *Result.ErrorCode.ToString());
		EquipDialog->SetNotice(LOCTEXT("EnhanceFailed", "장비·재화 상태 또는 이용 조건이 변경되어 실행하지 못했습니다. 창을 닫고 다시 확인하십시오."));
		return;
	}
	// 목록·상세 카드·칸은 OnEquipmentChanged 가 이미 새 강화 단계로 다시 그렸다
	CloseEquipDialog();
}

bool UOfficeEmployeePanelWidget::CloseEquipDialog()
{
	if (!EquipDialog)
	{
		return false;
	}
	const bool bWasShown = EquipDialog->IsInViewport();
	EquipDialog->OnPrimary.Unbind();
	EquipDialog->OnSecondary.Unbind();
	EquipDialog->OnTertiary.Unbind();
	EquipDialog->OnClose.Unbind();
	EquipDialog->RemoveFromParent();
	EquipDialogInstanceId.Invalidate();
	return bWasShown;
}

// ===== 장착 순간 =====

void UOfficeEmployeePanelWidget::BeginEquipMoment(EEmployeeEquipmentSlot Part, const FText& Title, const FText& Effect)
{
	bEquipMomentActive = true;
	EquipMomentElapsed = 0.f;
	EquipMomentPart = Part;
	SetWorldSlotsShown(false);
	if (EquipToastTitleText) EquipToastTitleText->SetText(Title);
	if (EquipToastEffectText)
	{
		EquipToastEffectText->SetText(Effect);
		EquipToastEffectText->SetVisibility(Effect.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (EquipToast)
	{
		EquipToast->SetRenderOpacity(0.f);
		EquipToast->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UOfficeEmployeePanelWidget::EndEquipMoment()
{
	bEquipMomentActive = false;
	EquipMomentElapsed = 0.f;
	if (EquipCalloutLines) EquipCalloutLines->ClearFlash();
	if (EquipToast) EquipToast->SetVisibility(ESlateVisibility::Collapsed);
}

// ===== 장비 뽑기 =====

void UOfficeEmployeePanelWidget::HandleEquipGachaClicked()
{
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const TSubclassOf<UUserWidget> GachaClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::EquipmentGachaPanel) : nullptr;
	if (!GachaClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeEmployeePanel] EquipmentGachaPanel 클래스 미등록 — DT_WidgetClass 확인"));
		return;
	}
	const int32 EmployeeId = SelectedEmployeeId;
	PushPromptAndReturn(GachaClass.Get(), [EmployeeId](UCommonActivatableWidget& Widget)
	{
		if (UEquipmentGachaPanelWidget* GachaPanel = Cast<UEquipmentGachaPanelWidget>(&Widget))
		{
			GachaPanel->Open(EmployeeId);
		}
	}, OfficeEquipTab::TabIndex);
}

// ===== 자동화 진입점 =====

void UOfficeEmployeePanelWidget::SelectEquipmentPart(int32 PartIndex)
{
	if (PartIndex >= 0 && PartIndex < OfficeEquipTab::PartCount)
	{
		HandleEquipSlotClicked(PartIndex);
	}
}

void UOfficeEmployeePanelWidget::SelectEquipmentChip(int32 ChipIndex)
{
	SelectEquipChip(ChipIndex);
}

bool UOfficeEmployeePanelWidget::PickEquipmentItem(int32 OrderIndex)
{
	UGameInstance* GI = GetGameInstance();
	const UEmployeeEquipmentManagerSubsystem* Equipment = GI ? GI->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
	const UTableManagerSubsystem* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Equipment || !Tables || !EquipmentList)
	{
		return false;
	}
	struct FOrderedItem
	{
		FGuid Id;
		EEmployeeEquipmentGrade Grade;
		int32 Level;
	};
	TArray<FOrderedItem> Ordered;
	for (const TPair<FGuid, FEmployeeEquipmentInstance>& Pair : Equipment->GetState().Instances)
	{
		const FEmployeeEquipmentDefinitionRow* Definition = Tables->FindEmployeeEquipmentDefinition(Pair.Value.DefinitionId);
		if (Definition && (bEquipAllParts || Definition->EquipmentSlot == EquipSelectedPart))
		{
			Ordered.Add({ Pair.Key, Definition->Grade, Pair.Value.EnhanceLevel });
		}
	}
	// 목록과 같은 순서 — 등급 내림차순 → 강화 내림차순 → ID
	Ordered.Sort([](const FOrderedItem& A, const FOrderedItem& B)
	{
		if (A.Grade != B.Grade) return A.Grade > B.Grade;
		if (A.Level != B.Level) return A.Level > B.Level;
		return A.Id < B.Id;
	});
	return Ordered.IsValidIndex(OrderIndex) && EquipmentList->SelectItem(Ordered[OrderIndex].Id, true);
}

void UOfficeEmployeePanelWidget::PressEquipmentDetailPrimary()
{
	if (IsEquipDetailShown())
	{
		HandleEquipDetailPrimary();
	}
}

void UOfficeEmployeePanelWidget::PressEquipmentDetailSecondary()
{
	if (IsEquipDetailShown())
	{
		HandleEquipDetailSecondary();
	}
}

void UOfficeEmployeePanelWidget::ConfirmEquipmentDialog()
{
	if (EquipDialog && EquipDialog->IsInViewport())
	{
		ExecuteEquipEnhance();
	}
}

// ===== 튜토리얼 앵커 (장비 장착 안내) =====

bool UOfficeEmployeePanelWidget::IsEquipmentTabActive() const
{
	return IsActivated() && SelectedEmployeeId != INDEX_NONE && IsEquipmentTabShown();
}

UWidget* UOfficeEmployeePanelWidget::GetEquipmentTabButtonWidget() const
{
	return IsActivated() ? TabEquipmentButton.Get() : nullptr;
}

UWidget* UOfficeEmployeePanelWidget::GetEmptyEquipmentSlotWidget() const
{
	if (!IsEquipmentTabActive()) return nullptr;
	UGameInstance* GI = GetGameInstance();
	const UEmployeeEquipmentManagerSubsystem* Equipment = GI ? GI->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
	const UTableManagerSubsystem* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Equipment || !Tables) return nullptr;

	// 칸 버튼 자체는 늘 Visible 이고 접히는 건 부모 상자다 — 지금 떠 있는 줄(월드 칸/대기 직원 칸)만 고른다
	const bool bBench = BenchSlotRow && BenchSlotRow->IsVisible();
	if (!bBench && !bWorldSlotsShown) return nullptr;
	const TArray<TObjectPtr<UIconWithButtonWidget>>& PartButtons = bBench ? BenchSlotButtons : WorldSlotButtons;

	TArray<int32> OwnedPerPart;
	OwnedPerPart.Init(0, OfficeEquipTab::PartCount);
	for (const TPair<FGuid, FEmployeeEquipmentInstance>& Pair : Equipment->GetState().Instances)
	{
		const FEmployeeEquipmentDefinitionRow* Definition = Tables->FindEmployeeEquipmentDefinition(Pair.Value.DefinitionId);
		const int32 PartIndex = Definition ? static_cast<int32>(Definition->EquipmentSlot) : INDEX_NONE;
		if (OwnedPerPart.IsValidIndex(PartIndex)) ++OwnedPerPart[PartIndex];
	}

	UWidget* FirstEmpty = nullptr;
	for (int32 Index = 0; Index < OfficeEquipTab::PartCount; ++Index)
	{
		UIconWithButtonWidget* PartButton = PartButtons.IsValidIndex(Index) ? PartButtons[Index].Get() : nullptr;
		if (!PartButton || Equipment->GetEquippedInstance(SelectedEmployeeId, static_cast<EEmployeeEquipmentSlot>(Index))) continue;
		if (OwnedPerPart[Index] == 0) return PartButton;
		if (!FirstEmpty) FirstEmpty = PartButton;
	}
	return FirstEmpty;
}

UWidget* UOfficeEmployeePanelWidget::GetEquipmentGachaButtonWidget() const
{
	// [장비 뽑기]는 빈 목록 안내(EquipEmptyNotice) 안에 있다 — 안내가 접히면 버튼도 화면에 없다
	if (!IsEquipmentTabActive() || !EquipEmptyNotice || !EquipEmptyNotice->IsVisible()) return nullptr;
	return EquipGachaButton.Get();
}

UWidget* UOfficeEmployeePanelWidget::GetUnequippedEquipmentEntryWidget() const
{
	if (!IsEquipmentTabActive() || !EquipmentList || IsEquipDetailShown()) return nullptr;
	UGameInstance* GI = GetGameInstance();
	const UEmployeeEquipmentManagerSubsystem* Equipment = GI ? GI->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
	const UTableManagerSubsystem* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Equipment || !Tables) return nullptr;

	// 목록과 같은 순서(등급 → 강화 → ID, PickEquipmentItem 기준)의 첫 미장착 장비
	FGuid BestId;
	EEmployeeEquipmentGrade BestGrade = EEmployeeEquipmentGrade::Common;
	int32 BestLevel = 0;
	for (const TPair<FGuid, FEmployeeEquipmentInstance>& Pair : Equipment->GetState().Instances)
	{
		const FEmployeeEquipmentDefinitionRow* Definition = Tables->FindEmployeeEquipmentDefinition(Pair.Value.DefinitionId);
		if (!Definition || Equipment->GetInstanceEmployee(Pair.Key) != INDEX_NONE) continue;
		if (!bEquipAllParts && Definition->EquipmentSlot != EquipSelectedPart) continue;
		const bool bBetter = !BestId.IsValid()
			|| Definition->Grade > BestGrade
			|| (Definition->Grade == BestGrade && (Pair.Value.EnhanceLevel > BestLevel
				|| (Pair.Value.EnhanceLevel == BestLevel && Pair.Key < BestId)));
		if (bBetter)
		{
			BestId = Pair.Key;
			BestGrade = Definition->Grade;
			BestLevel = Pair.Value.EnhanceLevel;
		}
	}
	return BestId.IsValid() ? EquipmentList->GetEntryWidgetForInstance(BestId) : nullptr;
}

UWidget* UOfficeEmployeePanelWidget::GetAllEquipmentPartsChipWidget() const
{
	if (!IsEquipmentTabActive() || bEquipAllParts || EquipChipButtons.IsEmpty()) return nullptr;
	return EquipChipButtons[0].Get();
}

UWidget* UOfficeEmployeePanelWidget::GetEquipDetailEquipButtonWidget() const
{
	if (!IsEquipmentTabActive() || !IsEquipDetailShown() || !EquipDetailCard) return nullptr;
	UGameInstance* GI = GetGameInstance();
	const UEmployeeEquipmentManagerSubsystem* Equipment = GI ? GI->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
	if (!Equipment || Equipment->GetInstanceEmployee(EquipDetailInstanceId) == SelectedEmployeeId) return nullptr;
	return EquipDetailCard->GetPrimaryButtonWidget();
}

FString UOfficeEmployeePanelWidget::GetEquipmentDebugState() const
{
	UGameInstance* GI = GetGameInstance();
	const UEmployeeEquipmentManagerSubsystem* Equipment = GI ? GI->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
	const UTableManagerSubsystem* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const UItemInventoryManager* Items = GI ? GI->GetSubsystem<UItemInventoryManager>() : nullptr;
	TArray<FString> Fields;
	Fields.Add(FString::Printf(TEXT("\"employee\":%d,\"part\":%d,\"tab\":%s,\"slots_shown\":%s,\"detail\":%s,\"enhance_card\":%s,\"dialog\":%s,\"moment\":%s,\"moment_t\":%.3f,\"bench_row\":%s,\"framing_pending\":%s,\"worker\":%s,\"preview\":%s"),
		SelectedEmployeeId, static_cast<int32>(EquipSelectedPart), IsEquipmentTabShown() ? TEXT("true") : TEXT("false"),
		bWorldSlotsShown ? TEXT("true") : TEXT("false"), IsEquipDetailShown() ? TEXT("true") : TEXT("false"),
		IsEnhanceCardShown() ? TEXT("true") : TEXT("false"), (EquipDialog && EquipDialog->IsInViewport()) ? TEXT("true") : TEXT("false"),
		bEquipMomentActive ? TEXT("true") : TEXT("false"), EquipMomentElapsed,
		(BenchSlotRow && BenchSlotRow->IsVisible()) ? TEXT("true") : TEXT("false"),
		bCameraFramingPending ? TEXT("true") : TEXT("false"), FocusWorker.IsValid() ? TEXT("true") : TEXT("false"),
		EquipPreviewWorker.IsValid() ? TEXT("true") : TEXT("false")));
	Fields.Add(FString::Printf(TEXT("\"cores\":%d"), Items ? Items->GetItemCount(EItemType::EmployeeEquipmentCore) : -1));
	if (EquipDetailCardBox)
	{
		Fields.Add(FString::Printf(TEXT("\"detail_height\":%.1f"), EquipDetailCardBox->GetHeightOverride()));
	}

	TArray<FString> Owned, Equipped;
	if (Equipment && Tables)
	{
		TArray<int32> Counts;
		Counts.Init(0, OfficeEquipTab::PartCount);
		for (const TPair<FGuid, FEmployeeEquipmentInstance>& Pair : Equipment->GetState().Instances)
		{
			if (const FEmployeeEquipmentDefinitionRow* Definition = Tables->FindEmployeeEquipmentDefinition(Pair.Value.DefinitionId))
			{
				++Counts[static_cast<int32>(Definition->EquipmentSlot)];
			}
		}
		for (int32 Index = 0; Index < OfficeEquipTab::PartCount; ++Index)
		{
			Owned.Add(FString::FromInt(Counts[Index]));
			const FEmployeeEquipmentInstance* Item = Equipment->GetEquippedInstance(SelectedEmployeeId, static_cast<EEmployeeEquipmentSlot>(Index));
			Equipped.Add(Item ? FString::Printf(TEXT("\"%s+%d\""), *Item->DefinitionId.ToString(), Item->EnhanceLevel) : TEXT("null"));
		}
	}
	Fields.Add(FString::Printf(TEXT("\"owned\":[%s],\"equipped\":[%s]"), *FString::Join(Owned, TEXT(",")), *FString::Join(Equipped, TEXT(","))));

	TArray<FString> SlotCenters, Anchors, Curve;
	for (const FVector2D& Point : LastWorldSlotCenters) SlotCenters.Add(OfficeEquipTab::VecJson(Point));
	for (const FVector2D& Point : LastWorldAnchors) Anchors.Add(OfficeEquipTab::VecJson(Point));
	for (const FVector2D& Point : LastCurvePoints) Curve.Add(OfficeEquipTab::VecJson(Point));
	const FVector2D LayerSize = WorldLayer ? WorldLayer->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
	Fields.Add(FString::Printf(TEXT("\"layer\":%s,\"slot_centers\":[%s],\"anchors\":[%s],\"curve\":[%s]"),
		*OfficeEquipTab::VecJson(LayerSize), *FString::Join(SlotCenters, TEXT(",")), *FString::Join(Anchors, TEXT(",")), *FString::Join(Curve, TEXT(","))));
	TArray<FString> ChipsOn, Equipped3;
	for (int32 Chip = 0; Chip < EquipChipOn.Num(); ++Chip)
	{
		ChipsOn.Add((EquipChipOn[Chip] && EquipChipOn[Chip]->IsVisible()) ? TEXT("1") : TEXT("0"));
	}
	for (int32 Index = 0; Index < WorldSlotEquipped.Num(); ++Index)
	{
		Equipped3.Add((WorldSlotEquipped[Index] && WorldSlotEquipped[Index]->IsVisible()) ? TEXT("1") : TEXT("0"));
	}
	Fields.Add(FString::Printf(TEXT("\"all\":%s,\"chips_on\":[%s],\"slot_equipped_face\":[%s],\"visible_items\":%d"),
		bEquipAllParts ? TEXT("true") : TEXT("false"), *FString::Join(ChipsOn, TEXT(",")), *FString::Join(Equipped3, TEXT(",")),
		EquipmentList ? EquipmentList->GetVisibleItemCount() : -1));
	return TEXT("{") + FString::Join(Fields, TEXT(",")) + TEXT("}");
}

#undef LOCTEXT_NAMESPACE
