// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Element/Effects/FloatingNumberWidget.h"

#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Enum/WidgetType.h"
#include "Manager/TableManagerSubsystem.h"

float UFloatingNumberWidget::EaseOutBack(float A)
{
	// 0,1 에서 정확히 0,1 / 중간에 1을 넘겨 오버슈트(팝)
	const float C1 = 1.70158f;
	const float C3 = C1 + 1.0f;
	const float T = A - 1.0f;
	return 1.0f + C3 * T * T * T + C1 * T * T;
}

float UFloatingNumberWidget::EaseOutCubic(float A)
{
	const float T = 1.0f - A;
	return 1.0f - T * T * T;
}

UFloatingNumberWidget* UFloatingNumberWidget::Spawn(UUserWidget* Owner, UCanvasPanel* Host, const FVector2D& AnchorAbsPos, const FText& Text, FLinearColor Color, int32 StackIndex)
{
	if (!Owner || !Host) return nullptr;

	UGameInstance* GI = Owner->GetGameInstance();
	if (!GI) return nullptr;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return nullptr;

	TSubclassOf<UUserWidget> WidgetClass = TableMgr->GetWidgetClass(EWidgetType::FloatingNumber);
	if (!WidgetClass)
	{
		// DT_WidgetClass 에 FloatingNumber 미등록 — loud failure (데이터 주도 원칙)
		UE_LOG(LogTemp, Warning, TEXT("[FloatingNumber] EWidgetType::FloatingNumber 클래스 미등록 (DT_WidgetClass 확인)"));
		return nullptr;
	}

	UFloatingNumberWidget* Widget = CreateWidget<UFloatingNumberWidget>(Owner, WidgetClass);
	if (!Widget) return nullptr;

	// 아이콘 절대(스크린) 좌표 → Host 캔버스 로컬 (AbsoluteToLocal 패턴)
	const FGeometry CanvasGeo = Host->GetCachedGeometry();
	FVector2D BasePos = CanvasGeo.AbsoluteToLocal(AnchorAbsPos);

	// 아이콘 오른쪽에 좌측정렬로 띄움 + 연타 시 위로 살짝 분산
	const float IconRightGap = 40.f;   // 아이콘 오른쪽 시작 간격(캔버스 로컬 px) — 조정 지점
	const int32 Slot = StackIndex % 3;
	BasePos += FVector2D(IconRightGap, -Slot * 8.f);

	UPanelSlot* PanelSlot = Host->AddChild(Widget);
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PanelSlot))
	{
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetAlignment(FVector2D(0.0f, 0.5f));  // 좌측-센터 피벗 = 아이콘 오른쪽에 좌측정렬
		CanvasSlot->SetPosition(BasePos);
	}

	Widget->Play(Text, Color, StackIndex);
	return Widget;
}

void UFloatingNumberWidget::Play(const FText& Text, FLinearColor Color, int32 InStackIndex)
{
	StackIndex = InStackIndex;
	Elapsed = 0.f;
	bPlaying = true;

	if (NumberText)
	{
		NumberText->SetText(Text);
		NumberText->SetColorAndOpacity(FSlateColor(Color));
	}

	// 첫 틱 전 풀사이즈 1프레임 깜빡임 방지
	FWidgetTransform Init;
	Init.Scale = FVector2D(0.5f, 0.5f);
	SetRenderTransform(Init);
	SetRenderOpacity(1.0f);
}

void UFloatingNumberWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bPlaying) return;

	Elapsed += InDeltaTime;
	const float T = FMath::Clamp(Elapsed / Lifetime, 0.f, 1.f);

	// Pop: 0~PopDuration 동안 0.5 → 1.0 (오버슈트)
	float Scale = 1.0f;
	if (Elapsed < PopDuration)
	{
		const float PopA = FMath::Clamp(Elapsed / PopDuration, 0.f, 1.f);
		Scale = FMath::Lerp(0.5f, 1.0f, EaseOutBack(PopA));
	}

	// Rise: 전체 구간 Y 0 → -RiseDistance (감속)
	const float RiseY = -RiseDistance * EaseOutCubic(T);

	FWidgetTransform Xform;
	Xform.Translation = FVector2D(0.f, RiseY);
	Xform.Scale = FVector2D(Scale, Scale);
	Xform.Shear = FVector2D::ZeroVector;
	Xform.Angle = 0.f;
	SetRenderTransform(Xform);

	// Fade: FadeStart~Lifetime
	if (Elapsed >= FadeStart)
	{
		const float FadeA = FMath::Clamp((Elapsed - FadeStart) / (Lifetime - FadeStart), 0.f, 1.f);
		SetRenderOpacity(1.0f - FadeA);
	}

	if (T >= 1.0f)
	{
		bPlaying = false;
		RemoveFromParent();
	}
}
