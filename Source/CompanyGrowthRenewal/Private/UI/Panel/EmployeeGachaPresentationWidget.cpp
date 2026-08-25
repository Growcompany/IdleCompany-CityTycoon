// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/EmployeeGachaPresentationWidget.h"
#include "Manager/MissionManagerSubsystem.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Manager/RecruitmentManagerSubsystem.h"
#include "Manager/EmployeeManager.h"
#include "Office/WorkstationActorBase.h"
#include "Manager/EntityManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Manager/SettingsManagerSubsystem.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Data/EmployeeTypes.h"
#include "Enum/CompanyType.h"
#include "Enum/WidgetType.h"
#include "UI/UISoundTags.h"
#include "UI/Gacha/GachaCaptureStage.h"
#include "UI/Element/Cards/EmployeeIdCardMiniWidget.h"
#include "Utils/FWidgetAnimationUtils.h"
#include "Utils/GachaBatchMath.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
namespace EmpFx
{
	// 스테이징 확정값 (specs/2026-07-11 §3, 치수 = 리빌 컬럼 720x1180 @2560x1440 캔버스)
	constexpr float BuildBase = 1.15f;        // ⚠ PIE 재조율 1순위 (특성 가챠는 0.9→0.55 하향 전례)
	constexpr float BuildMulStep = 0.14f;     // tier 당 가산 → 신화 1.70x
	constexpr float FlashDuration = 0.19f;
	constexpr float EjectDuration = 0.55f;
	constexpr float EjectPunch = 1.08f;
	constexpr float SettlePad = 0.07f;
	constexpr float StampDelay = 0.30f;
	constexpr float StampInDuration = 0.28f;
	constexpr float StampShakeDelay = 0.05f;
	constexpr float StampShakeDuration = 0.24f;
	constexpr float StampShakeAmp = 10.f;
	constexpr float ButtonDelay = 0.55f;
	constexpr float ButtonFadeDuration = 0.35f;
	constexpr float ButtonRise = 24.f;
	constexpr float DimFadeDuration = 0.3f;
	constexpr float SlotGlowMaxOpacity = 0.9f;
	constexpr float PeekTravel = 210.f;       // 티징 상승 거리 (PeekClip 안, 트리와 동기)
	constexpr float PeekVibAmp = 2.4f;
	constexpr float PeekVibHz = 6.25f;
	constexpr int32 GlintTierMin = 2;         // 레어+
	constexpr float GlintDelay = 0.5f;
	// 시트 광택 스윕 — 코팅 카드 광 (테두리 순환광 TrailLine 은 기각: 마스크 부정합+인쇄물 재질 상충)
	constexpr float ShinePeriod = 2.7f;
	constexpr float ShineSweep = 0.8f;
	constexpr float ShineMaxA = 0.55f;
	constexpr int32 HaloTierMin = 3;          // 에픽+
	constexpr float HaloMaxOpacity = 0.45f;
	constexpr int32 BeamTierMin = 4;          // 레전드리+
	constexpr float BeamMaxOpacity = 0.42f;
	constexpr float ShockwaveDuration = 0.55f;
	constexpr float CardW = 560.f;            // 트리 IdCardBox SizeBox 와 동기
	constexpr float CardH = 773.f;
	// ⚠ 컷아웃 마스크 노브(Threshold/MaskBoost)는 머티리얼 기본값(0.04/12)을 존중 — 민감화(0.02/24)는
	// 배경 알파 노이즈가 흑백 점으로 뚫림, 통짜 사진(Threshold -1)은 배경판 부재로 빈 씬 노이즈 (2026-07-11 실측 2건)
	constexpr float SlotToCardY = 331.f;      // 슬롯(y224) → 카드 중심(y555) 사출 이동 거리
	constexpr float DustGravity = 1050.f;

	// 멀티 리빌 (스펙 §5.1) — 사이드 축소/간격 + 도장 후 딜링 타이밍
	constexpr float SideScale = 0.58f;        // 무대 SideActorScale 과 동기 (사진 원근 일치)
	constexpr float CardGapPx = 24.f;
	constexpr float SideYOffset = 0.f;
	constexpr float DealStagger = 0.10f;
	constexpr float DealDur = 0.32f;
	constexpr float DealStartPad = 0.14f;
	constexpr float DealPopScale = 0.82f;     // 딜링 시작 스케일 배수 (목업 재현)
	// 사이드 배치 기준점 폴백 — 권위는 런타임 IdCardBox 캔버스 슬롯 (트리 SC_Card = 360,555)
	constexpr float CardCenterX = 360.f;
	constexpr float CardCenterY = 555.f;

	// 런타임 생성 UImage 레이아웃 크기는 Brush.ImageSize 로 고정 —
	// SetDesiredSizeOverride 는 Slate 미생성 시점 호출이 유실됨 (특성 가챠 5차 실측)
	void SetImageLayoutSize(UImage* Img, const FVector2D& Size)
	{
		if (!Img) { return; }
		FSlateBrush Brush = Img->GetBrush();
		Brush.ImageSize = Size;
		Img->SetBrush(Brush);
	}
} // namespace EmpFx — ⚠ 유니티 빌드에서 타 cpp 익명 상수와 합쳐지므로 파일 고유 네임스페이스 필수 (2026-07-11 특성 가챠와 충돌 실측)
} // namespace

UEmployeeGachaPresentationWidget::UEmployeeGachaPresentationWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 빛가루 도트 — 하드 참조 (쿠킹 자동 포함)
	static ConstructorHelpers::FObjectFinder<UTexture2D> DustF(TEXT("/Game/CompanyGrowth/UI/Textures/Effect/Glow/circle_05.circle_05"));
	if (DustF.Succeeded()) { DustTexture = DustF.Object; }
}

UWidget* UEmployeeGachaPresentationWidget::GetConfirmButtonWidget() const
{
	return ConfirmButton;
}

void UEmployeeGachaPresentationWidget::SetPreferredWorkstation(AWorkstationActorBase* Workstation)
{
	PreferredWorkstationWeak = Workstation;
}

void UEmployeeGachaPresentationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RecruitmentManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<URecruitmentManagerSubsystem>() : nullptr;

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().RemoveAll(this);
		ConfirmButton->OnClicked().AddUObject(this, &UEmployeeGachaPresentationWidget::OnConfirmClicked);
	}
	if (UIE_CloseButton && !UIE_CloseButton->OnCloseClicked.IsBound())
	{
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UEmployeeGachaPresentationWidget::OnCloseClicked);
	}

	// 스테이징 FX 텍스처 주입 — DT_UIVFXTexture 단일 진실 (WBP엔 빈 Image만 배치, 기존 행 재사용)
	if (UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr)
	{
		TableMgr->ApplyUIVFXTexture(TEXT("GachaCardGlow"), SlotGlow);       // Glow_Oval (300x200)
		TableMgr->ApplyUIVFXTexture(TEXT("GachaCardGlow"), CardBackGlow);
		TableMgr->ApplyUIVFXTexture(TEXT("GachaShockwave"), ShockwaveRing); // circle_rings_a (800)
		TableMgr->ApplyUIVFXTexture(TEXT("GachaBeam"), BeamImage);          // cone_composed_a (512)
		TableMgr->ApplyUIVFXTexture(TEXT("CTAShine"), CardShine);          // Shine_Effect — 시트 광택
	}
	// 시트 광 폭만 고정, 세로는 슬롯 VAlign_Fill 이 카드 높이로 늘림
	EmpFx::SetImageLayoutSize(CardShine, FVector2D(240.f, 32.f));

	ResetStagingVisuals();

	// M4 미션 가이드 — [확인] 버튼 하이라이트 타겟 등록
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>() : nullptr)
	{
		MissionMgr->RegisterGachaPresentation(this);
	}
}

void UEmployeeGachaPresentationWidget::NativeDestruct()
{
	if (ConfirmButton) ConfirmButton->OnClicked().RemoveAll(this);
	if (UIE_CloseButton) UIE_CloseButton->OnCloseClicked.RemoveAll(this);

	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>() : nullptr)
	{
		MissionMgr->UnregisterGachaPresentation(this);
	}

	// 명시적 버튼 없이 닫혀도 결과 유실 방지 (idempotent)
	IsMulti() ? BankAllResults() : BankCurrentResult();

	if (!bCurrentBanked)
	{
		UE_LOG(LogTemp, Error, TEXT("[GachaReveal] 미적립 상태로 파괴 — 직원 %d명 유실 (대표 ID %d)"),
			IsMulti() ? BatchResults.Num() : 1, CurrentResult.ResultEmployee.EmployeeID);
	}

	// 캡처 OFF + 무대 정리 (성능 핵심 — 연출 닫히면 매프레임 캡처 중단)
	EndCharacterCapture();

	Super::NativeDestruct();
}

void UEmployeeGachaPresentationWidget::SetupPull(const FGachaResultData& Result, EGachaTier Tier, int32 BuildingIndex)
{
	CurrentResult = Result;
	CurrentTier = Tier;
	CurrentBuildingIndex = BuildingIndex;
	bCurrentBanked = false;

	DisplayCard(Result);
	ShowCharacterCapture(Result); // 사원증 증명사진 = 라이브 댄스 (RT)
	BeginStaging();
}

void UEmployeeGachaPresentationWidget::SetupPullBatch(const TArray<FGachaResultData>& Results,
	EGachaTier Tier, int32 BuildingIndex)
{
	if (Results.Num() == 1) { SetupPull(Results[0], Tier, BuildingIndex); return; }
	// 빈 결과는 적립할 게 없다 — 파괴 안전망(NativeDestruct 뱅킹)이 빈 CurrentResult 로 오발하지 않게 먼저 닫아둔다
	if (Results.Num() == 0) { bCurrentBanked = true; RemoveFromParent(); return; }

	BatchResults = Results;
	CurrentTier = Tier;
	CurrentBuildingIndex = BuildingIndex;
	bCurrentBanked = false;

	TArray<int32> Tiers;
	Tiers.Reserve(BatchResults.Num());
	for (const FGachaResultData& R : BatchResults) { Tiers.Add(TierOf(R.PotentialRarity)); }
	DisplayOrder = GachaBatchMath::ComputeDisplayOrder(Tiers);

	// 센터 = 베스트 — 기존 단발 경로(카드/이펙트/빌드업 배수)가 그대로 탄다
	CurrentResult = BatchResults[DisplayOrder[0]];
	DisplayCard(CurrentResult);

	// 센터 사번은 DisplayCard 의 +1 이 아니라 뽑기순번 기준 — 덮어쓴다
	if (UTextBlock* BandLabel = Cast<UTextBlock>(GetWidgetFromName(TEXT("BandLabel"))))
	{
		const int32 Base = RecruitmentManager ? RecruitmentManager->GetTotalHiredCount(CurrentBuildingIndex) : 0;
		BandLabel->SetText(FText::FromString(FString::Printf(TEXT("ID NO.%03d"), Base + 1 + DisplayOrder[0])));
	}

	// 무대/머티리얼이 없어도 카드 자체는 나와야 한다 — 사진만 빠진 상태로 강등
	SpawnSideCards();

	// 무대: 슬롯 순서(C,R1,L1,R2,L2)로 직원 전달 → 와이드 RT 1장 + 카드별 UV 윈도우
	if (CharacterImage && CaptureStageClass && GetWorld())
	{
		CaptureStage = AGachaCaptureStage::AcquireShared(this, GetWorld(), CaptureStageClass);
		TArray<FEmployeeInstance> SlotEmployees;
		SlotEmployees.Reserve(DisplayOrder.Num());
		for (int32 Idx : DisplayOrder) { SlotEmployees.Add(BatchResults[Idx].ResultEmployee); }

		if (UTextureRenderTarget2D* Rt = CaptureStage ? CaptureStage->BeginRevealBatch(SlotEmployees) : nullptr)
		{
			TSoftObjectPtr<UMaterialInterface> MatPath(FSoftObjectPath(
				TEXT("/Game/CompanyGrowth/UI/Materials/M_GachaRevealRT_Window.M_GachaRevealRT_Window")));
			UMaterialInterface* Mat = MatPath.LoadSynchronous();
			if (Mat)
			{
				auto MakeSlotMID = [this, Mat, Rt](int32 SlotIdx) -> UMaterialInstanceDynamic*
				{
					UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
					if (!MID) { return nullptr; }
					MID->SetTextureParameterValue(FName("RT"), Rt);
					// 창의 권위는 무대 — 카드가 상수로 다시 계산하면 리그 튜닝 시 사진 프레이밍만 조용히 어긋난다
					const FVector4 W = CaptureStage->GetLiveSlotUVWindow(SlotIdx);
					MID->SetVectorParameterValue(FName("UVWindow"), FLinearColor(
						static_cast<float>(W.X), static_cast<float>(W.Y),
						static_cast<float>(W.Z), static_cast<float>(W.W)));
					return MID;
				};
				CharacterImage->SetBrushFromMaterial(MakeSlotMID(0));
				for (int32 s = 0; s < SideCards.Num(); ++s)
				{
					if (SideCards[s]) { SideCards[s]->SetPhotoMaterial(MakeSlotMID(s + 1)); }
				}
			}
			else
			{
				// 폴백(윈도우 머티리얼 미생성): 센터만 와이드 RT 직접 표시(알파 반전), 사이드 사진은 생략
				FSlateBrush Brush = CharacterImage->GetBrush();
				Brush.SetResourceObject(Rt);
				CharacterImage->SetBrush(Brush);
			}
			CharacterImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	BeginStaging(); // RevealRarity = CurrentResult(베스트) 기준 — 기존 로직 그대로
}

void UEmployeeGachaPresentationWidget::SpawnSideCards()
{
	SideCards.Reset();
	if (!StageCanvas || DisplayOrder.Num() < 2) { return; }

	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UEmployeeManager* EmpMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr;
	// DT 행이 아직 없으면 사이드 생략 — 센터 단독 연출로 강등(크래시 금지)
	TSubclassOf<UUserWidget> CardClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::EmployeeIdCardMini) : nullptr;
	if (!CardClass) { return; }

	// 배치 권위 = 센터 사원증 슬롯. 사이드는 같은 앵커/정렬/위치에서 출발하고 X 는 딜링이 RenderTranslation 으로
	FAnchors CardAnchors(0.f, 0.f);
	FVector2D CardAlignment(0.5f, 0.5f);
	FVector2D CardPosition(EmpFx::CardCenterX, EmpFx::CardCenterY);
	int32 SideZOrder = 5;
	if (UCanvasPanelSlot* CenterSlot = Cast<UCanvasPanelSlot>(IdCardBox ? IdCardBox->Slot : nullptr))
	{
		if (CenterSlot->Parent == StageCanvas)
		{
			CardAnchors = CenterSlot->GetAnchors();
			CardAlignment = CenterSlot->GetAlignment();
			CardPosition = CenterSlot->GetPosition();
			SideZOrder = CenterSlot->GetZOrder() - 1; // 센터보다 뒤 — "뒤에서 미끄러져 나오는" 문법
		}
	}

	const int32 Base = RecruitmentManager ? RecruitmentManager->GetTotalHiredCount(CurrentBuildingIndex) : 0;
	const ECompanyType CompanyType = GetBuildingCompanyType();

	for (int32 k = 1; k < DisplayOrder.Num(); ++k)
	{
		const FGachaResultData& R = BatchResults[DisplayOrder[k]];
		UEmployeeIdCardMiniWidget* MiniCard = GetOwningPlayer()
			? CreateWidget<UEmployeeIdCardMiniWidget>(GetOwningPlayer(), CardClass)
			: CreateWidget<UEmployeeIdCardMiniWidget>(this, CardClass);
		if (!MiniCard) { continue; }

		const FText DeptText = TableMgr->GetDepartmentDisplayName(CompanyType, R.ResultEmployee.Department);
		const FString RankStr = EmpMgr
			? EmpMgr->GetRankDisplayName(UEmployeeTypeHelper::GetRankFromEnhancementLevel(R.ResultEmployee.EnhancementLevel))
			: FString();
		MiniCard->SetCardData(R, Base + 1 + DisplayOrder[k], DeptText, RankStr);

		if (UCanvasPanelSlot* CardSlot = StageCanvas->AddChildToCanvas(MiniCard))
		{
			CardSlot->SetAnchors(CardAnchors);
			CardSlot->SetAlignment(CardAlignment);
			CardSlot->SetAutoSize(true);
			CardSlot->SetPosition(CardPosition);
			CardSlot->SetZOrder(SideZOrder);
		}
		// 공개 전 Collapsed — opacity 0 만으론 자식이 비치는 사례 (기존 실측)
		MiniCard->SetVisibility(ESlateVisibility::Collapsed);
		MiniCard->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		SideCards.Add(MiniCard);
	}
}

void UEmployeeGachaPresentationWidget::ShowCharacterCapture(const FGachaResultData& Result)
{
	if (!CharacterImage || !CaptureStageClass || !GetWorld())
	{
		return;
	}

	// 라이브 캡처 무대는 월드 동시 1개 — 직원창 히어로 라이브 뷰와 공유(AcquireShared 가 인계/스폰).
	CaptureStage = AGachaCaptureStage::AcquireShared(this, GetWorld(), CaptureStageClass);
	UTextureRenderTarget2D* Rt = CaptureStage ? CaptureStage->BeginReveal(Result.ResultEmployee) : nullptr;

	if (Rt)
	{
		// RT 알파가 반전(캐릭터=투명/배경=불투명)이라 UI 머티리얼에서 OneMinus(a) 로 보정 → 캐릭터 불투명/배경 투명
		// 런타임 에셋 로드는 TSoftObjectPtr + LoadSynchronous (LoadObject 문자열 경로는 모바일 쿠킹 시 깨짐)
		TSoftObjectPtr<UMaterialInterface> MatPath(FSoftObjectPath(
			TEXT("/Game/CompanyGrowth/UI/Materials/M_GachaRevealRT.M_GachaRevealRT")));
		UMaterialInterface* Mat = MatPath.LoadSynchronous();
		if (Mat)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
			MID->SetTextureParameterValue(FName("RT"), Rt);
			CharacterImage->SetBrushFromMaterial(MID);
		}
		else
		{
			FSlateBrush Brush = CharacterImage->GetBrush();
			Brush.SetResourceObject(Rt); // 폴백(머티리얼 미생성): 직접 표시(알파 반전 상태)
			CharacterImage->SetBrush(Brush);
		}
		CharacterImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UEmployeeGachaPresentationWidget::EndCharacterCapture()
{
	if (IsValid(CaptureStage))
	{
		CaptureStage->ReleaseShared(this); // 직원창이 인계했으면 no-op — 그쪽이 반납
	}
	CaptureStage = nullptr;
}

void UEmployeeGachaPresentationWidget::DisplayCard(const FGachaResultData& Result)
{
	const FEmployeeInstance& Emp = Result.ResultEmployee;

	if (ResultNameText)
	{
		ResultNameText->SetText(FText::FromString(Emp.EmployeeName));
	}

	// 사번 — 이 건물 누적 채용 순번 (표시 시점 = 적립 전이라 +1). Bungee 폰트 = ASCII 만.
	if (UTextBlock* BandLabel = Cast<UTextBlock>(GetWidgetFromName(TEXT("BandLabel"))))
	{
		const int32 Ordinal = (RecruitmentManager ? RecruitmentManager->GetTotalHiredCount(CurrentBuildingIndex) : 0) + 1;
		BandLabel->SetText(FText::FromString(FString::Printf(TEXT("ID NO.%03d"), Ordinal)));
	}

	if (ResultRarityText)
	{
		ResultRarityText->SetText(FText::FromString(FLootBoxRarityUtility::GetKoreanName(Result.PotentialRarity)));
		// 라이트 플레이트 가독 보정 — 미니 사원증과 동일 잉크 (스펙 §5)
		ResultRarityText->SetColorAndOpacity(FSlateColor(FLootBoxRarityUtility::GetRarityInkOnLight(Result.PotentialRarity)));
	}

	// 부서 (산업별 표시명 — 배치 빌딩의 CompanyType 기반, DT 단일 진실)
	if (ResultDepartmentText)
	{
		FText DeptText;
		if (UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr)
		{
			DeptText = TableMgr->GetDepartmentDisplayName(GetBuildingCompanyType(), Emp.Department);
		}
		ResultDepartmentText->SetText(DeptText);
	}

	// 직급 (강화 레벨 → 직급 표시명)
	if (ResultRankText)
	{
		if (UEmployeeManager* EmployeeManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr)
		{
			FString RankStr = EmployeeManager->GetRankDisplayName(
				UEmployeeTypeHelper::GetRankFromEnhancementLevel(Emp.EnhancementLevel));
			ResultRankText->SetText(FText::FromString(RankStr));
		}
	}
}

ECompanyType UEmployeeGachaPresentationWidget::GetBuildingCompanyType() const
{
	if (CurrentBuildingIndex == INDEX_NONE)
	{
		return ECompanyType::None;
	}
	if (UEntityManager* EntityMgr = GetWorld() ? GetWorld()->GetSubsystem<UEntityManager>() : nullptr)
	{
		if (ABuildingBaseActor* Building = EntityMgr->GetBuildingByIndex(CurrentBuildingIndex))
		{
			return Building->GetCompanyType();
		}
	}
	return ECompanyType::None;
}

void UEmployeeGachaPresentationWidget::BankCurrentResult()
{
	if (bCurrentBanked || !RecruitmentManager)
	{
		return;
	}
	// 실패(초상화 캡처 중) 시 '처리 중입니다' 토스트는 HireEmployeeFromCard 가 띄움 — 재탭이 재시도
	if (RecruitmentManager->ConfirmGachaHire(CurrentResult, PreferredWorkstationWeak.Get()))
	{
		bCurrentBanked = true;
		ShowPlacementToast();
	}
}

void UEmployeeGachaPresentationWidget::BankAllResults()
{
	if (bCurrentBanked || !RecruitmentManager)
	{
		return;
	}

	// 부분 채용 금지 — 초상화 큐가 남아있으면 루프 진입 전에 전체 보류, 재탭이 재시도 (스펙 §6)
	UEmployeeManager* EmpMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr;
	if (EmpMgr && EmpMgr->IsPortraitCapturing())
	{
		if (UUIManagerSubsystem* UIMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr)
		{
			UIMgr->ShowNotification(NSLOCTEXT("Recruitment", "PortraitBusy", "직원 등록을 처리 중입니다"),
				2.0f, ENotificationType::Warning);
		}
		return;
	}

	int32 Seated = 0, Benched = 0;
	const bool bAllHired = RecruitmentManager->ConfirmGachaHireBatch(BatchResults, PreferredWorkstationWeak.Get(), Seated, Benched);
	if (Seated + Benched > 0)
	{
		if (!bAllHired)
		{
			// 정원/캡처 게이트를 전부 선통과한 뒤라 도달 불가 경로 — 도달했다면 게이트가 새는 것
			UE_LOG(LogTemp, Error, TEXT("[GachaReveal] 배치 채용 부분 실패 — 요청 %d / 처리 %d"),
				BatchResults.Num(), Seated + Benched);
		}
		bCurrentBanked = true;
		if (UUIManagerSubsystem* UIMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr)
		{
			UIMgr->ShowNotification(FText::FromString(FString::Printf(
				TEXT("직원 %d명을 채용했습니다 ― %d명 착석, %d명 대기"), Seated + Benched, Seated, Benched)),
				3.0f, ENotificationType::Success);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[GachaReveal] 배치 채용 전량 실패 — 결과 %d건 폐기 (빌딩 %d)"),
			BatchResults.Num(), CurrentBuildingIndex);
		// 전량 실패 원인은 정원 초과뿐 = 오버레이 안에서 해소 불가 → 재탭도 영원히 실패한다.
		// 유저를 창에 가두느니 폐기하고 닫게 한다(캡처 게이트는 시간이 해소하므로 위에서 이미 return).
		bCurrentBanked = true;
	}
}

void UEmployeeGachaPresentationWidget::ShowPlacementToast() const
{
	UEmployeeManager* EmpMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr;
	const FEmployeeInstance* Emp = EmpMgr ? EmpMgr->GetEmployeeData(CurrentResult.ResultEmployee.EmployeeID) : nullptr;
	if (UUIManagerSubsystem* UIMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr)
	{
		const bool bSeated = (Emp && Emp->bIsAssigned);
		UIMgr->ShowNotification(bSeated
			? NSLOCTEXT("Recruitment", "AutoSeated", "새 직원이 자리에 앉았습니다")
			: NSLOCTEXT("Recruitment", "AutoSeatBenched", "빈 책상이 없어 대기 명단에 보관했습니다"),
			3.0f,
			bSeated ? ENotificationType::Success : ENotificationType::Warning);
	}
}

void UEmployeeGachaPresentationWidget::OnConfirmClicked()
{
	IsMulti() ? BankAllResults() : BankCurrentResult();
	if (!bCurrentBanked)
	{
		return; // 초상화 캡처 중 — 오버레이 유지, 재탭 유도
	}
	RemoveFromParent(); // 뷰포트 오버레이 (스택 X). NativeDestruct 의 뱅킹 안전망 유지.
}

void UEmployeeGachaPresentationWidget::OnCloseClicked()
{
	IsMulti() ? BankAllResults() : BankCurrentResult();
	if (!bCurrentBanked)
	{
		return;
	}
	RemoveFromParent();
}

// ===== 페이즈 스테이징 =====

int32 UEmployeeGachaPresentationWidget::TierOf(ELootBoxRarity R)
{
	switch (R)
	{
	case ELootBoxRarity::Common:    return 0;
	case ELootBoxRarity::Unusual:   return 1;
	case ELootBoxRarity::Rare:      return 2;
	case ELootBoxRarity::Epic:      return 3;
	case ELootBoxRarity::Legendary: return 4;
	case ELootBoxRarity::Mythic:    return 5;
	default:                        return 0;
	}
}

float UEmployeeGachaPresentationWidget::BuildMulOf(ELootBoxRarity R)
{
	return 1.f + EmpFx::BuildMulStep * TierOf(R); // 일반 1.00x ~ 신화 1.70x (스펙 §4)
}

void UEmployeeGachaPresentationWidget::BeginStaging()
{
	RevealRarity = CurrentResult.PotentialRarity;
	RarityColor = FLootBoxRarityUtility::GetRarityColor(RevealRarity);
	bReduceMotion = USettingsManagerSubsystem::IsReduceMotion(this);
	const int32 Tier = TierOf(RevealRarity);

	BuildDuration = EmpFx::BuildBase * BuildMulOf(RevealRarity);
	RevealStartTime = BuildDuration + EmpFx::FlashDuration * 0.4f;
	SettleTime = RevealStartTime + EmpFx::EjectDuration + EmpFx::SettlePad;
	StampTime = SettleTime + EmpFx::StampDelay;
	ButtonsTime = SettleTime + EmpFx::ButtonDelay;

	// 멀티: 도장이 찍힌 뒤 좌우 딜링 → 버튼은 딜링 종료 기준. 연출감소면 stagger 0 (전원 동시 정착, 이동은 유지)
	DealStaggerEff = bReduceMotion ? 0.f : EmpFx::DealStagger;
	DealStartTime = 0.f;
	DealEndTime = 0.f;
	if (IsMulti() && SideCards.Num() > 0)
	{
		DealStartTime = StampTime + EmpFx::StampInDuration + EmpFx::DealStartPad;
		DealEndTime = DealStartTime + (SideCards.Num() - 1) * DealStaggerEff + EmpFx::DealDur;
		ButtonsTime = DealEndTime + EmpFx::ButtonDelay;
	}

	StageTime = 0.f;
	bFlashFired = false;
	bStampFired = false;
	bButtonsShown = false;
	bStagingActive = true;

	ResetStagingVisuals();
	RebuildDustPool();

	// 딤 강화 — 뒤 채용 패널 비침 방지 (특성 가챠 실측값 동기)
	if (DimBG) { DimBG->SetBrushTintColor(FSlateColor(FLinearColor(0.008f, 0.011f, 0.02f, 0.96f))); }

	// 등급색 텔레그래프 — 빌드업부터 결과 색 노출. FX 크기 = DT ImageSize 기준 RenderScale (레이아웃 불침투)
	if (SlotGlow)
	{
		SlotGlow->SetColorAndOpacity(RarityColor);
		SlotGlow->SetRenderScale(FVector2D(660.f / 300.f, 150.f / 200.f)); // Glow_Oval 300x200 → 660x150 (슬롯 560 기준)
	}
	if (ShockwaveRing) { ShockwaveRing->SetColorAndOpacity(RarityColor); }
	if (CardBackGlow)
	{
		CardBackGlow->SetColorAndOpacity(RarityColor);
		const float SpreadMul = 0.8f + 0.3f * Tier; // 등급 비례 번짐 폭 (특성 가챠 공식 동기)
		CardBackGlow->SetRenderScale(FVector2D(
			(EmpFx::CardW + 420.f * SpreadMul) / 300.f,
			(EmpFx::CardH + 140.f * SpreadMul) / 200.f));
	}
	if (BeamImage)
	{
		BeamImage->SetColorAndOpacity(FMath::Lerp(RarityColor, FLinearColor::White, 0.3f));
		BeamImage->SetRenderScale(FVector2D(110.f / 512.f, 980.f / 512.f)); // cone 512 → 110x980 기둥
	}
	// 포토링 = 등급색 (트리의 흰 아웃라인 브러시 x 틴트)
	if (PhotoRing) { PhotoRing->SetColorAndOpacity(RarityColor); }
	// 테두리 순환광(TrailLine)은 기각 — 마스크가 사원증 비율과 부정합 + 인쇄물 재질과 상충 (2026-07-11 PIE 판정)

	PlayUITag(CGUISoundTags::GachaBuildup);
}

void UEmployeeGachaPresentationWidget::ResetStagingVisuals()
{
	if (DimBG) { DimBG->SetRenderOpacity(0.f); }
	auto Hide = [](UImage* Img) { if (Img) { Img->SetRenderOpacity(0.f); } };
	Hide(SlotGlow); Hide(CardBackGlow); Hide(BeamImage); Hide(FlashOverlay); Hide(ShockwaveRing);
	// 공개 전 카드는 Collapsed — opacity 0 만으론 자식(포토플레이트/도장)이 비치는 사례 (2026-07-11 PIE)
	if (IdCardBox) { IdCardBox->SetVisibility(ESlateVisibility::Collapsed); IdCardBox->SetRenderOpacity(0.f); }
	if (StampBox) { StampBox->SetRenderOpacity(0.f); }
	if (CardShine) { CardShine->SetRenderOpacity(0.f); }
	if (PeekClip) { PeekClip->SetVisibility(ESlateVisibility::HitTestInvisible); }
	if (PeekCard) { PeekCard->SetRenderTranslation(FVector2D(0.f, EmpFx::PeekTravel)); }
	if (StageCanvas) { StageCanvas->SetRenderTranslation(FVector2D::ZeroVector); }

	// 여운 전까지 조작 차단 — 버튼/닫기 숨김
	auto HideButton = [](UWidget* W) { if (W) { W->SetVisibility(ESlateVisibility::Collapsed); W->SetRenderOpacity(0.f); } };
	HideButton(ConfirmButton);
	HideButton(UIE_CloseButton);
}

void UEmployeeGachaPresentationWidget::RebuildDustPool()
{
	for (UImage* Img : DustImages) { if (Img) { Img->RemoveFromParent(); } }
	DustImages.Reset();
	DustParams.Reset();

	const int32 Tier = TierOf(CurrentResult.PotentialRarity);
	if (!FXLayer || !WidgetTree || !DustTexture || bReduceMotion || Tier < EmpFx::BeamTierMin) { return; }

	const FLinearColor Gold = FLootBoxRarityUtility::GetRarityColor(ELootBoxRarity::Legendary);
	const FLinearColor Palette[4] = { Gold, FLinearColor::White, FLinearColor(1.f, 0.89f, 0.54f), RarityColor };
	const int32 Waves = (Tier >= 5) ? 3 : 2; // 레전 22 / 신화 33 (스펙 §4)
	constexpr int32 PerWave = 11;
	for (int32 Wv = 0; Wv < Waves; ++Wv)
	{
		for (int32 i = 0; i < PerWave; ++i)
		{
			UImage* Img = WidgetTree->ConstructWidget<UImage>();
			if (!Img) { continue; }
			Img->SetBrushFromTexture(DustTexture, false);
			FEmpGachaDustParam P;
			P.Angle = -PI * 0.5f + FMath::FRandRange(-1.0f, 1.0f); // 위쪽 ±57도 분출
			P.Delay = Wv * 0.5f + FMath::FRandRange(0.f, 0.15f);
			P.Life = FMath::FRandRange(1.0f, 1.6f);
			P.Speed = FMath::FRandRange(420.f, 980.f);
			P.Size = FMath::FRandRange(14.f, 34.f);
			EmpFx::SetImageLayoutSize(Img, FVector2D(P.Size, P.Size));
			Img->SetColorAndOpacity(Palette[FMath::RandRange(0, 3)]);
			Img->SetRenderOpacity(0.f);
			Img->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UCanvasPanelSlot* CSlot = FXLayer->AddChildToCanvas(Img)) { CSlot->SetAutoSize(true); }
			DustParams.Add(P);
			DustImages.Add(Img);
		}
	}
}

void UEmployeeGachaPresentationWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bStagingActive) { UpdateStaging(InDeltaTime); }
}

void UEmployeeGachaPresentationWidget::UpdateStaging(float DeltaTime)
{
	StageTime += DeltaTime;
	const float T = StageTime;
	const float B = BuildDuration;
	const int32 Tier = TierOf(RevealRarity);

	if (DimBG) { DimBG->SetRenderOpacity(FMath::Clamp(T / EmpFx::DimFadeDuration, 0.f, 1.f)); }

	// ① 빌드업 — 슬롯 등급광 램프 + 카드 티징 상승/진동, 플래시 직후 급소멸
	if (SlotGlow)
	{
		SlotGlow->SetRenderOpacity((T < B)
			? (0.08f + 0.82f * (T / B)) * EmpFx::SlotGlowMaxOpacity
			: FMath::Clamp(1.f - (T - B) / 0.12f, 0.f, 1.f) * EmpFx::SlotGlowMaxOpacity);
	}
	if (PeekCard && PeekClip && PeekClip->GetVisibility() != ESlateVisibility::Collapsed && T < B)
	{
		const float K = T / B;
		const float E = FWidgetAnimationUtils::EaseOutQuad(K);
		const float Vib = bReduceMotion ? 0.f : FMath::Sin(T * 2.f * PI * EmpFx::PeekVibHz) * EmpFx::PeekVibAmp * K;
		PeekCard->SetRenderTranslation(FVector2D(Vib, EmpFx::PeekTravel * (1.f - E)));
	}

	// ② 플래시 + 임팩트 사운드(1회) — 티징 카드 은퇴, 실카드 사출로 스왑
	if (T >= B && !bFlashFired)
	{
		bFlashFired = true;
		if (PeekClip) { PeekClip->SetVisibility(ESlateVisibility::Collapsed); }
		PlayUITag(CGUISoundTags::GachaFlash);
		PlayUITag(StingTagForTier(Tier));
	}
	if (FlashOverlay)
	{
		FlashOverlay->SetRenderOpacity((T >= B && T < B + EmpFx::FlashDuration) ? 0.92f * (1.f - (T - B) / EmpFx::FlashDuration) : 0.f);
	}
	if (ShockwaveRing)
	{
		if (Tier >= EmpFx::GlintTierMin && T >= B)
		{
			const float K = FMath::Clamp((T - B) / EmpFx::ShockwaveDuration, 0.f, 1.f);
			const float S = 0.25f + 2.4f * FWidgetAnimationUtils::EaseOutQuad(K); // DT 800px 기준
			ShockwaveRing->SetRenderScale(FVector2D(S, S));
			ShockwaveRing->SetRenderOpacity(0.9f * (1.f - K));
		}
		else { ShockwaveRing->SetRenderOpacity(0.f); }
	}

	// ③ 공개 — 사원증 사출: 슬롯에서 중앙으로 + 펀치 오버슈트 (목업 cardEject 재현)
	if (IdCardBox)
	{
		if (T < RevealStartTime)
		{
			IdCardBox->SetRenderOpacity(0.f);
		}
		else
		{
			if (IdCardBox->GetVisibility() == ESlateVisibility::Collapsed)
			{
				IdCardBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			}
			const float EK = FMath::Clamp((T - RevealStartTime) / EmpFx::EjectDuration, 0.f, 1.f);
			IdCardBox->SetRenderOpacity(FMath::Clamp(EK / 0.12f, 0.f, 1.f));
			const float Move = FWidgetAnimationUtils::EaseOutQuad(EK);
			const float S = (EK < 0.55f)
				? FMath::Lerp(0.5f, EmpFx::EjectPunch, FWidgetAnimationUtils::EaseOutQuad(EK / 0.55f))
				: FMath::Lerp(EmpFx::EjectPunch, 1.f, (EK - 0.55f) / 0.45f);
			IdCardBox->SetRenderTranslation(FVector2D(0.f, -EmpFx::SlotToCardY * (1.f - Move)));
			IdCardBox->SetRenderScale(FVector2D(S, S));
		}
	}

	const float TSettle = T - SettleTime;

	// ④ 여운 — 배후광 브리딩(에픽+), 빛기둥(레전+), 글린트(레어+). 브리딩 = 밝기만 (스케일 펄스 짜침 판정)
	if (CardBackGlow)
	{
		if (Tier >= EmpFx::HaloTierMin && TSettle >= 0.f)
		{
			const float MythicMul = (Tier >= 5) ? 1.15f : 1.f;
			const float Breathe = 0.85f + 0.15f * FMath::Sin(TSettle * 1.8f);
			CardBackGlow->SetRenderOpacity(FMath::Min(
				EmpFx::HaloMaxOpacity * MythicMul * Breathe * FMath::Clamp(TSettle / 0.5f, 0.f, 1.f), 0.85f));
		}
		else { CardBackGlow->SetRenderOpacity(0.f); }
	}
	if (BeamImage)
	{
		if (Tier >= EmpFx::BeamTierMin && TSettle >= 0.f)
		{
			const float Breathe = 0.9f + 0.1f * FMath::Sin(TSettle * 2.2f);
			BeamImage->SetRenderOpacity(EmpFx::BeamMaxOpacity * Breathe * FMath::Clamp(TSettle / 0.7f, 0.f, 1.f));
		}
		else { BeamImage->SetRenderOpacity(0.f); }
	}
	// 시트 광택 스윕 (레어+) — 흰 광이 카드를 주기적으로 훑음 (ShineClip 이 카드 밖 클립)
	if (CardShine && Tier >= EmpFx::GlintTierMin && TSettle >= EmpFx::GlintDelay)
	{
		const float Cycle = FMath::Fmod(TSettle - EmpFx::GlintDelay, EmpFx::ShinePeriod);
		if (Cycle < EmpFx::ShineSweep)
		{
			const float K = Cycle / EmpFx::ShineSweep;
			CardShine->SetRenderTranslation(FVector2D(FMath::Lerp(-300.f, EmpFx::CardW + 60.f, K), 0.f));
			CardShine->SetRenderOpacity(EmpFx::ShineMaxA * FMath::Sin(K * PI));
		}
		else
		{
			CardShine->SetRenderOpacity(0.f);
		}
	}

	// 도장 — 전 등급 공통 스탬프인 (꽝도 의식의 완결감, 스펙 §4)
	if (T >= StampTime && !bStampFired)
	{
		bStampFired = true;
		PlayUITag(CGUISoundTags::GachaStamp);
	}
	if (StampBox)
	{
		const float SK = (T - StampTime) / EmpFx::StampInDuration;
		if (SK <= 0.f)
		{
			StampBox->SetRenderOpacity(0.f);
		}
		else
		{
			StampBox->SetRenderOpacity(FWidgetAnimationUtils::StampInOpacity(SK));
			const float S = FWidgetAnimationUtils::StampInScale(SK);
			StampBox->SetRenderScale(FVector2D(S, S));
		}
	}
	// 도장 셰이크 — 연출감소 게이트, 딤 제외 StageCanvas 만 (루트 흔들면 딤 가장자리 노출)
	if (StageCanvas)
	{
		const float ShakeT0 = StampTime + EmpFx::StampShakeDelay;
		if (!bReduceMotion && T >= ShakeT0 && T < ShakeT0 + EmpFx::StampShakeDuration)
		{
			StageCanvas->SetRenderTranslation(FWidgetAnimationUtils::ShakeOffset(T - ShakeT0, EmpFx::StampShakeDuration, EmpFx::StampShakeAmp));
		}
		else
		{
			StageCanvas->SetRenderTranslation(FVector2D::ZeroVector);
		}
	}

	// 골드 빛가루 분수 (settle 이후, 레전+) — 초속 직선 + 중력 낙하 (특성 가챠 수식 동기)
	const FVector2D LayerSize = FXLayer ? FXLayer->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
	const FVector2D Center = LayerSize * 0.5f;
	for (int32 i = 0; i < DustImages.Num(); ++i)
	{
		UImage* Img = DustImages[i];
		if (!Img || !DustParams.IsValidIndex(i)) { continue; }
		const FEmpGachaDustParam& P = DustParams[i];
		const float K = TSettle - P.Delay;
		if (K < 0.f || K > P.Life || LayerSize.Y < 1.f)
		{
			Img->SetRenderOpacity(0.f);
			continue;
		}
		const float Kn = K / P.Life;
		const FVector2D Dir(FMath::Cos(P.Angle), FMath::Sin(P.Angle));
		const FVector2D Pos = Center + Dir * (P.Speed * K) + FVector2D(0.f, 0.5f * EmpFx::DustGravity * K * K)
			- FVector2D(P.Size * 0.5f, P.Size * 0.5f);
		if (UCanvasPanelSlot* CSlot = Cast<UCanvasPanelSlot>(Img->Slot)) { CSlot->SetPosition(Pos); }
		const float Shrink = 1.f - 0.4f * Kn;
		Img->SetRenderScale(FVector2D(Shrink, Shrink));
		Img->SetRenderOpacity((Kn < 0.55f) ? 1.f : (1.f - Kn) / 0.45f);
	}

	// 버튼/닫기 지연 등장 (+페이드/라이즈) — 그 전까지 오조작 차단. 펄스 없음 (특성 가챠 기각 승계)
	if (!bButtonsShown && T >= ButtonsTime)
	{
		bButtonsShown = true;
		if (ConfirmButton) { ConfirmButton->SetVisibility(ESlateVisibility::Visible); }
		if (UIE_CloseButton) { UIE_CloseButton->SetVisibility(ESlateVisibility::Visible); }
	}
	if (bButtonsShown)
	{
		const float K = FMath::Clamp((T - ButtonsTime) / EmpFx::ButtonFadeDuration, 0.f, 1.f);
		const FVector2D Rise(0.f, EmpFx::ButtonRise * (1.f - K));
		auto FadeIn = [K, Rise](UWidget* W) { if (W) { W->SetRenderOpacity(K); W->SetRenderTranslation(Rise); } };
		FadeIn(ConfirmButton);
		FadeIn(UIE_CloseButton);
	}

	if (IsMulti()) { UpdateDealing(); }
}

void UEmployeeGachaPresentationWidget::UpdateDealing()
{
	// 사이드는 캔버스 위치가 센터와 같고 RenderTranslation 으로 목표 X 까지 — "센터 뒤에서 미끄러져 나오는" 문법.
	// 전부 StageTime 파생(누적 상태 0)이라 탭 스킵이 시간 점프만으로 정확하다.
	for (int32 s = 0; s < SideCards.Num(); ++s)
	{
		UEmployeeIdCardMiniWidget* MiniCard = SideCards[s];
		if (!MiniCard) { continue; }

		const float T0 = DealStartTime + s * DealStaggerEff;
		if (StageTime < T0)
		{
			MiniCard->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		const float K = FMath::Clamp((StageTime - T0) / EmpFx::DealDur, 0.f, 1.f);
		const float E = FWidgetAnimationUtils::EaseOutBack(K);
		const float TargetX = GachaBatchMath::SlotXOffset(s + 1, EmpFx::CardW, EmpFx::SideScale, EmpFx::CardGapPx);
		MiniCard->SetVisibility(ESlateVisibility::HitTestInvisible);
		MiniCard->SetRenderOpacity(FMath::Clamp(K / 0.6f, 0.f, 1.f));
		MiniCard->SetRenderTranslation(FVector2D(TargetX * E, EmpFx::SideYOffset));
		MiniCard->SetRenderScale(FVector2D(EmpFx::SideScale * FMath::Lerp(EmpFx::DealPopScale, 1.f, E)));
	}
}

void UEmployeeGachaPresentationWidget::SkipToSettle()
{
	if (!bFlashFired)
	{
		bFlashFired = true;
		if (PeekClip) { PeekClip->SetVisibility(ESlateVisibility::Collapsed); }
		PlayUITag(StingTagForTier(TierOf(RevealRarity)));
	}
	// 이후 상태는 전부 StageTime 에서 stateless 재유도 — 시간만 점프
	StageTime = SettleTime;
}

FReply UEmployeeGachaPresentationWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 빌드업~공개 중 탭 = 스킵 (여운 상태로 시간 점프)
	if (bStagingActive && StageTime < SettleTime)
	{
		SkipToSettle();
		return FReply::Handled();
	}
	// 멀티 딜링 중 탭 = 딜링 끝으로 점프 (사이드 카드 상태도 StageTime 에서 재유도)
	if (bStagingActive && IsMulti() && StageTime >= DealStartTime && StageTime < DealEndTime)
	{
		StageTime = DealEndTime;
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UEmployeeGachaPresentationWidget::PlayUITag(const FGameplayTag& Tag)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* Snd = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			Snd->PlayUISound(Tag);
		}
	}
}

FGameplayTag UEmployeeGachaPresentationWidget::StingTagForTier(int32 Tier) const
{
	if (Tier >= 4) { return CGUISoundTags::GachaResultLegendary; }
	if (Tier == 3) { return CGUISoundTags::GachaResultEpic; }
	return CGUISoundTags::GachaResultRare;
}
