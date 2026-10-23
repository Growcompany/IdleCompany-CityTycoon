// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/OfflineReportModalWidget.h"

#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Enum/WidgetType.h"
#include "Global/GlobalUtilFunctions.h"
#include "Manager/ProjectOperationManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Player/MainMapPlayerController.h"
#include "Table/ResourceInfo.h"
#include "UI/Element/Building/OfflineGainRowWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Panel/InGameLayerWidget.h"

namespace
{
	// 카운트업 — 축하가 착지한 뒤에 손실을 말하기 위한 기준 타이밍(설계 SOT §6)
	constexpr float HeroCountUpDelay = 0.25f;
	constexpr float HeroCountUpDuration = 0.6f;

	const FLinearColor OfflineLossInk(1.0f, 0.35f, 0.35f, 1.0f);
}

void UOfflineReportModalWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// CommonUI 풀이 같은 인스턴스를 재사용한다 — 이전 리포트의 수거 플래그가 남으면 다음 리포트가 통째로 막힌다
	bVaultCollectedThisReport = false;

	// 데이터는 델리게이트로 수신 — 푸시(구독) 직후 Consume 이 1회 발화 → SetReportData
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USaveLoadManager* SL = GI->GetSubsystem<USaveLoadManager>())
		{
			SL->OnOfflineGainsDetailed.AddUObject(this, &UOfflineReportModalWidget::SetReportData);
		}
	}

	if (CollectAllButton)
	{
		CollectAllButton->OnClicked().AddUObject(this, &UOfflineReportModalWidget::HandleCollectAllClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnCloseClicked.AddDynamic(this, &UOfflineReportModalWidget::HandleCloseButtonClicked);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UOfflineReportModalWidget::HandleBackgroundClicked);
	}
}

void UOfflineReportModalWidget::NativeDestruct()
{
	// 수거 UI 가 MainMap 에서 사라졌으므로 어떤 경로로 닫혀도 금고를 비운다(버튼·배경 탭·백 액션·스택 pop·레벨 전환).
	// ⚠ NativeOnDeactivated 가 아니라 여기인 이유: CommonUI 스택은 위젯이 "덮일" 때도 DeactivateWidget 을 부른다
	// (CommonActivatableWidgetContainer::SetSwitcherIndex) → 승격 오버레이 push 만으로 금고가 조용히 비워졌다.
	// 덮임은 슬롯을 Release 하지 않으므로 NativeDestruct 가 안 불린다. 닫힘·레벨 전환만 여기 온다.
	// ReleaseWidget 은 1 ticker 틱 뒤 Reset 이라 닫힘보다 1프레임 늦다 → GameInstance null 가드 유지 + Super 앞에서 수거.
	CollectVaultSilentlyIfNeeded();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (USaveLoadManager* SL = GI->GetSubsystem<USaveLoadManager>())
		{
			SL->OnOfflineGainsDetailed.RemoveAll(this);
		}
	}

	if (CollectAllButton)
	{
		CollectAllButton->OnClicked().RemoveAll(this);
	}
	if (CloseButton)
	{
		CloseButton->OnCloseClicked.RemoveDynamic(this, &UOfflineReportModalWidget::HandleCloseButtonClicked);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UOfflineReportModalWidget::HandleBackgroundClicked);
	}

	Super::NativeDestruct();
}

void UOfflineReportModalWidget::SetReportData(float InTotalGained, float InOfflineSeconds,
	const TArray<FOfflineGainEntry>& InEntries, bool bInCapReached, int64 InBrickGained)
{
	TotalGained = InTotalGained;
	OfflineSeconds = InOfflineSeconds;
	bCapReached = bInCapReached;

	// 리포트 1건 = 가드 1회. 풀 재사용 인스턴스라 NativeConstruct 가 다시 안 불릴 수 있어 여기서도 리셋한다.
	bVaultCollectedThisReport = false;

	// ===== 벽돌 줄 =====
	// 공장이 정산 시점에 이미 인벤토리로 넣었다 — 보고만 한다(수령 동작 없음).
	// 풀 재사용 인스턴스라 0 일 때 반드시 접어야 지난 리포트의 숫자가 남지 않는다.
	if (BrickRow)
	{
		BrickRow->SetVisibility(InBrickGained > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (BrickAmountText && InBrickGained > 0)
	{
		BrickAmountText->SetText(FText::Format(
			NSLOCTEXT("OfflineReport", "BrickGainFmt", "벽돌 {0}개를 받았습니다"),
			UGlobalUtilFunctions::AbbreviateNumber(InBrickGained)));
	}

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;

	// ===== 시간 라인 =====
	// OfflineSeconds 는 서버가 12h 로 자른 값이라 "12시간 만에 돌아오셨습니다" 가 거짓일 수 있다 → 캡 도달 시 교체
	if (TimeAwayText)
	{
		FString TimeStr;
		if (bCapReached)
		{
			TimeStr = TEXT("12시간 이상 자리를 비우셨습니다");
		}
		else
		{
			const int32 TotalMinutes = FMath::FloorToInt(OfflineSeconds / 60.0f);
			const int32 Hours = TotalMinutes / 60;
			const int32 Minutes = TotalMinutes % 60;
			if (Hours > 0 && Minutes > 0)
			{
				TimeStr = FString::Printf(TEXT("%d시간 %d분 만에 돌아오셨습니다"), Hours, Minutes);
			}
			else if (Hours > 0)
			{
				TimeStr = FString::Printf(TEXT("%d시간 만에 돌아오셨습니다"), Hours);
			}
			else
			{
				TimeStr = FString::Printf(TEXT("%d분 만에 돌아오셨습니다"), FMath::Max(1, Minutes));
			}
		}
		TimeAwayText->SetText(FText::FromString(TimeStr));
	}

	// ===== 히어로 =====
	const bool bZeroGain = (TotalGained <= 0.0f);

	if (HeroCaptionText)
	{
		// 이 돈은 지갑이 아니라 각 건물 금고로 들어간다 — "획득"이라 쓰면 지갑을 확인한 유저가 배신감을 느낀다
		HeroCaptionText->SetText(FText::FromString(bZeroGain
			? TEXT("금고가 가득 차 이번에는 적립되지 않았습니다")
			: TEXT("각 건물 금고에 적립되었습니다")));
	}

	if (TotalGainText && TableMgr)
	{
		if (bZeroGain)
		{
			// 0원을 축하색(그린)으로 칠하면 거짓 — 뮤트로 둔다
			TotalGainText->SetColorAndOpacity(FSlateColor(FLinearColor::FromSRGBColor(FColor(0xB9, 0xC2, 0xCF))));
		}
		else
		{
			bool bResOk = false;
			const FResourceInfo ResInfo = TableMgr->GetResourceInfo(EResourceType::Money, bResOk);
			if (bResOk)
			{
				TotalGainText->SetColorAndOpacity(FSlateColor(ResInfo.UIColor));
			}
		}
	}

	// Money 아이콘도 DT 경유 (코드에 경로/색을 굽지 않는다)
	if (MoneyIcon && TableMgr)
	{
		bool bIconOk = false;
		const FResourceInfo ResInfo = TableMgr->GetResourceInfo(EResourceType::Money, bIconOk);
		if (bIconOk && !ResInfo.Icon.IsNull())
		{
			if (UTexture2D* MoneyTex = ResInfo.Icon.LoadSynchronous())
			{
				MoneyIcon->SetBrushFromTexture(MoneyTex);
			}
		}
	}

	// 적립 0 이면 축하 연출 전면 오프 — 카운트업 없이 최종값 즉시 표시
	bCountUpActive = !bZeroGain;
	CountUpElapsed = 0.0f;
	ApplyTotalText(bZeroGain ? 0.0f : 0.0f);

	// ===== 빌딩별 행 =====
	float TotalLoss = 0.0f;

	// 최대 수익원이 첫 줄 = 축하 우선. 손실 행을 하단에 모으지 않는다(문제 섹션처럼 읽혀 잔소리가 됨)
	TArray<FOfflineGainEntry> SortedEntries = InEntries;
	SortedEntries.Sort([](const FOfflineGainEntry& A, const FOfflineGainEntry& B)
	{
		return A.ActualGain > B.ActualGain;
	});

	for (const FOfflineGainEntry& Entry : SortedEntries)
	{
		TotalLoss += Entry.LossByVault;
	}

	if (RowBox)
	{
		RowBox->ClearChildren();

		TSubclassOf<UUserWidget> RowClass = TableMgr
			? TableMgr->GetWidgetClass(EWidgetType::OfflineGainRow)
			: nullptr;

		if (!RowClass)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[OfflineReportModal] EWidgetType::OfflineGainRow 클래스 없음 — DT_WidgetClass 행 확인"));
		}
		else
		{
			for (const FOfflineGainEntry& Entry : SortedEntries)
			{
				if (UOfflineGainRowWidget* RowWidget = CreateWidget<UOfflineGainRowWidget>(this, RowClass))
				{
					RowWidget->SetRowData(Entry);
					// C++ 로 추가한 슬롯은 패딩 0 → 행이 딱 붙는다. 스펙 §3 간격 10 을 슬롯 하단 패딩으로.
					if (UVerticalBoxSlot* BoxSlot = RowBox->AddChildToVerticalBox(RowWidget))
					{
						BoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
					}
				}
			}
		}
	}

	// ===== 손실 밴드 (안내만 — 강화는 각 빌딩에서. 딥링크/CTA 없음) =====
	const bool bHasLoss = TotalLoss > 0.0f;
	if (LossBand)
	{
		LossBand->SetVisibility(bHasLoss ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (LossBandAmountText && bHasLoss)
	{
		LossBandAmountText->SetText(UGlobalUtilFunctions::AbbreviateNumber(
			static_cast<int64>(TotalLoss), ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor));
		LossBandAmountText->SetColorAndOpacity(FSlateColor(OfflineLossInk));
	}

	if (CapNote)
	{
		CapNote->SetVisibility(bCapReached ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UOfflineReportModalWidget::ApplyTotalText(float Value)
{
	if (!TotalGainText)
	{
		return;
	}

	const FText Abbrev = UGlobalUtilFunctions::AbbreviateNumber(
		static_cast<int64>(Value), ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor);

	// 적립 0 은 "+0" 이 아니라 "0" — 더하기 기호가 붙으면 벌었다는 인상이 남는다
	TotalGainText->SetText(TotalGained <= 0.0f
		? Abbrev
		: FText::FromString(FString::Printf(TEXT("+%s"), *Abbrev.ToString())));
}

void UOfflineReportModalWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bCountUpActive)
	{
		return;
	}

	CountUpElapsed += InDeltaTime;
	const float Elapsed = CountUpElapsed - HeroCountUpDelay;
	if (Elapsed <= 0.0f)
	{
		return;
	}

	const float Alpha = FMath::Clamp(Elapsed / HeroCountUpDuration, 0.0f, 1.0f);
	const float Eased = 1.0f - FMath::Pow(1.0f - Alpha, 3.0f);  // CubicOut
	ApplyTotalText(TotalGained * Eased);

	if (Alpha >= 1.0f)
	{
		bCountUpActive = false;
	}
}

void UOfflineReportModalWidget::HandleCollectAllClicked()
{
	// 가드가 Deactivate 보다 앞에 와야 한다 — 뒤에 두면 이미 수거된 리포트에서 모달만 닫히고 아무 반응이 없다.
	if (!ShouldCollectOfflineVault(bVaultCollectedThisReport))
	{
		return;
	}
	bVaultCollectedThisReport = true;

	// 닫기 먼저 → 수거 나중. 스택 잔류 방지 + 코인 플라이아웃이 모달에 가리지 않게 (순서 고정)
	DeactivateWidget();

	// 수거+코인 플라이아웃은 InGameLayer(HUD) 소유 — UIManager 가 추적하는 단일 인스턴스를 직접 호출.
	// (델리게이트 브로드캐스트는 InGameLayer 이중구독 시 2회 수거되어 두 번째가 "빈 금고" 토스트를 띄웠음)
	if (UGameInstance* GI = GetGameInstance())
	{
		UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
		UInGameLayerWidget* InGame = UIMgr ? UIMgr->GetInGameLayer() : nullptr;
		if (InGame)
		{
			InGame->ExecuteAllVaultCollection();
		}
		// 포그라운드 복귀가 OfficeMap 에서 일어나면 InGameLayer 가 없다 — 가드는 이미 섰으니 연출 없이 직접 수거한다
		else if (UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>())
		{
			OpMgr->CollectAllStoredRevenue();
		}
	}
}

void UOfflineReportModalWidget::HandleCloseClicked()
{
	// 금고 비우기는 NativeDestruct 가 맡는다 — 백 액션·스택 pop 까지 한 지점이 덮게
	DeactivateWidget();
}

void UOfflineReportModalWidget::CollectVaultSilentlyIfNeeded()
{
	if (!ShouldCollectOfflineVault(bVaultCollectedThisReport))
	{
		return;
	}
	bVaultCollectedThisReport = true;

	// 연출이 붙은 InGameLayer::ExecuteAllVaultCollection 이 아니라 매니저 직접 호출 —
	// 닫기는 보상 연출이 아니므로 코인 플라이아웃·토스트를 띄우지 않는다.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>())
		{
			OpMgr->CollectAllStoredRevenue();
		}
	}
}

void UOfflineReportModalWidget::HandleCloseButtonClicked()
{
	HandleCloseClicked();
}

void UOfflineReportModalWidget::HandleBackgroundClicked()
{
	HandleCloseClicked();
}

void UOfflineReportModalWidget::NativeOnDeactivated()
{
	// UI 모드 복원 (호출자 책임 쌍). 강화 딥링크는 push 후 다시 GoToUIMode 하므로 순서상 안전
	if (UWorld* WorldPtr = GetWorld())
	{
		if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(WorldPtr->GetFirstPlayerController()))
		{
			if (PC->GetCurrentInputMode() == EInputMode::UI)
			{
				PC->GoToNormalMode();
			}
		}
	}

	Super::NativeOnDeactivated();
}
