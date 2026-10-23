#include "UI/Element/Gacha/GachaRevealBoxWidget.h"

#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Manager/TableManagerSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"

// 이름 붙은 namespace ― 익명이면 unity 묶음 뒤쪽 파일의 같은 이름 지역 변수가 C4459(전역 가림)로 빌드를 깬다
namespace GachaRevealBoxDetail
{
	const FName GlowTextureParameter(TEXT("Glow"));
	const FName GlowColorParameter(TEXT("GlowColor"));
	const FName GlowOpacityParameter(TEXT("Opacity"));

	UTexture2D* LoadLayer(const TSoftObjectPtr<UTexture2D>& Path, EGachaBoxType BoxType)
	{
		UTexture2D* Texture = Path.IsNull() ? nullptr : Path.LoadSynchronous();
		if (!Texture)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GachaRevealBox] %s 층 텍스처를 불러오지 못함: %s"),
				*UEnum::GetValueAsString(BoxType), Path.IsNull() ? TEXT("(빈 경로)") : *Path.ToString());
		}
		return Texture;
	}

	// 빈 브러시 UImage 는 흰 판으로 그려진다 ― 그림이 없으면 보이지 않게만 한다
	void Show(UImage* Image, bool bVisible)
	{
		if (Image)
		{
			Image->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	void SetLayerBrush(UImage* Image, UTexture2D* Texture, const FVector2D& Size)
	{
		FSlateBrush Brush = Image->GetBrush();
		Brush.SetResourceObject(Texture);
		Brush.SetImageSize(Size);
		Image->SetBrush(Brush);
	}

	UTexture2D* FrameAt(const TArray<TObjectPtr<UTexture2D>>& Frames, int32 Index)
	{
		return Frames.IsEmpty() ? nullptr : Frames[FMath::Clamp(Index, 0, Frames.Num() - 1)].Get();
	}
}

namespace GachaRevealBoxLayout
{
	int32 FlapFrameIndex(float Alpha)
	{
		return FMath::RoundToInt(FMath::Clamp(Alpha, 0.f, 1.f) * static_cast<float>(FlapFrameCount - 1));
	}

	FVector2D SeamRatio(const FGachaBoxVisualRow& Row, EGachaSeam Which)
	{
		switch (Which)
		{
		case EGachaSeam::Left: return Row.SeamL;
		case EGachaSeam::Right: return Row.SeamR;
		case EGachaSeam::Top: return Row.SeamTop;
		default: return Row.BoxOrigin;
		}
	}
}

const UTableManagerSubsystem* UGachaRevealBoxWidget::ResolveTables() const
{
	if (TablesOverride.IsValid())
	{
		return TablesOverride.Get();
	}
	const UGameInstance* Instance = GetGameInstance();
	return Instance ? Instance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
}

void UGachaRevealBoxWidget::SetBoxType(EGachaBoxType InBoxType)
{
	using namespace GachaRevealBoxDetail;
	if (!LayerBox || !ClosedImage || !BodyImage || !GlowImage || !FlapBackImage || !FlapFrontImage)
	{
		UE_LOG(LogTemp, Error, TEXT("[GachaRevealBox] 위젯 트리가 없다 ― WBP(UIE_GachaRevealBox)로 만들었는지 확인"));
		return;
	}
	BoxType = InBoxType;
	// 새 상자는 닫힌 채 쉬는 상태에서 시작한다(재사용 위젯이 앞 공개의 뚜껑·빛을 물려받지 않게)
	LidAlpha = 0.f;
	FlapFrame = 0;
	GlowColor = FLinearColor::White;
	GlowOpacity = 0.f;
	ClearVisual();

	const UTableManagerSubsystem* Tables = ResolveTables();
	if (!Tables)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GachaRevealBox] TableManager 없음 ― %s 상자를 그리지 못함"), *UEnum::GetValueAsString(InBoxType));
		return;
	}
	// 행이 없으면 GetGachaBoxVisual 이 Warning 을 남긴다(MissingRowLoud) ― 여기서는 빈 상자로 둔다
	const FGachaBoxVisualRow* Row = Tables->GetGachaBoxVisual(InBoxType);
	if (!Row)
	{
		return;
	}
	if (Row->LayerSize.X <= 0.0 || Row->LayerSize.Y <= 0.0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GachaRevealBox] %s 행 LayerSize 가 비어 있음"), *UEnum::GetValueAsString(InBoxType));
		return;
	}
	for (const TArray<TSoftObjectPtr<UTexture2D>>* Frames : { &Row->FlapFront, &Row->FlapBack })
	{
		if (Frames->Num() != GachaRevealBoxLayout::FlapFrameCount)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GachaRevealBox] %s 날개 프레임 %d장(%d장 기대) ― 남는 각도는 가까운 프레임으로"),
				*UEnum::GetValueAsString(InBoxType), Frames->Num(), GachaRevealBoxLayout::FlapFrameCount);
		}
	}

	bHasVisual = true;
	LayerSize = Row->LayerSize;
	for (const EGachaSeam Seam : { EGachaSeam::Left, EGachaSeam::Right, EGachaSeam::Top, EGachaSeam::Origin })
	{
		SeamRatios[static_cast<int32>(Seam)] = GachaRevealBoxLayout::SeamRatio(*Row, Seam);
	}

	ClosedTexture = LoadLayer(Row->Closed, InBoxType);
	BodyTexture = LoadLayer(Row->Body, InBoxType);
	GlowTexture = LoadLayer(Row->Glow, InBoxType);
	for (const TSoftObjectPtr<UTexture2D>& Frame : Row->FlapFront)
	{
		FlapFrontTextures.Add(LoadLayer(Frame, InBoxType));
	}
	for (const TSoftObjectPtr<UTexture2D>& Frame : Row->FlapBack)
	{
		FlapBackTextures.Add(LoadLayer(Frame, InBoxType));
	}

	LayerBox->SetWidthOverride(static_cast<float>(LayerSize.X));
	LayerBox->SetHeightOverride(static_cast<float>(LayerSize.Y));
	SetRenderTransformPivot(SeamRatios[static_cast<int32>(EGachaSeam::Origin)]);
	SetLayerBrush(ClosedImage, ClosedTexture, LayerSize);
	SetLayerBrush(BodyImage, BodyTexture, LayerSize);
	SetLayerBrush(FlapBackImage, FrameAt(FlapBackTextures, 0), LayerSize);
	SetLayerBrush(FlapFrontImage, FrameAt(FlapFrontTextures, 0), LayerSize);

	FSlateBrush GlowBrush = GlowImage->GetBrush();
	GlowBrush.SetImageSize(LayerSize);
	GlowImage->SetBrush(GlowBrush);
	GlowMID = GlowImage->GetDynamicMaterial();
	if (!GlowMID)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GachaRevealBox] GlowImage 브러시가 머티리얼이 아님 ― UIE_GachaRevealBox 저작(M_UI_GachaBoxGlow) 확인"));
	}
	else if (GlowTexture)
	{
		GlowMID->SetTextureParameterValue(GlowTextureParameter, GlowTexture);
	}

	ApplyFlapFrames();
	ApplyGlowParameters();
	RefreshLayers();
}

void UGachaRevealBoxWidget::ClearVisual()
{
	bHasVisual = false;
	LayerSize = FVector2D::ZeroVector;
	for (FVector2D& Ratio : SeamRatios)
	{
		Ratio = FVector2D::ZeroVector;
	}
	ClosedTexture = nullptr;
	BodyTexture = nullptr;
	GlowTexture = nullptr;
	FlapFrontTextures.Reset();
	FlapBackTextures.Reset();
	for (UImage* Image : { ClosedImage.Get(), BodyImage.Get(), FlapBackImage.Get(), FlapFrontImage.Get() })
	{
		GachaRevealBoxDetail::SetLayerBrush(Image, nullptr, FVector2D::ZeroVector);
	}
	LayerBox->ClearWidthOverride();
	LayerBox->ClearHeightOverride();
	RefreshLayers();
}

void UGachaRevealBoxWidget::SetLid(float Alpha)
{
	LidAlpha = FMath::Clamp(Alpha, 0.f, 1.f);
	FlapFrame = GachaRevealBoxLayout::FlapFrameIndex(LidAlpha);
	ApplyFlapFrames();
	RefreshLayers();
}

void UGachaRevealBoxWidget::SetInnerGlow(FLinearColor Color, float Opacity)
{
	GlowColor = Color;
	GlowOpacity = FMath::Clamp(Opacity, 0.f, 1.f);
	ApplyGlowParameters();
	RefreshLayers();
}

void UGachaRevealBoxWidget::ApplyFlapFrames()
{
	if (!bHasVisual || !FlapBackImage || !FlapFrontImage)
	{
		return;
	}
	FlapBackImage->SetBrushResourceObject(GachaRevealBoxDetail::FrameAt(FlapBackTextures, FlapFrame));
	FlapFrontImage->SetBrushResourceObject(GachaRevealBoxDetail::FrameAt(FlapFrontTextures, FlapFrame));
}

void UGachaRevealBoxWidget::ApplyGlowParameters()
{
	if (GlowMID)
	{
		GlowMID->SetVectorParameterValue(GachaRevealBoxDetail::GlowColorParameter, GlowColor);
		GlowMID->SetScalarParameterValue(GachaRevealBoxDetail::GlowOpacityParameter, GlowOpacity);
	}
}

void UGachaRevealBoxWidget::RefreshLayers()
{
	using namespace GachaRevealBoxDetail;
	// 층 합성은 위 가장자리 1px 테가 빠진다(Task 4 굽기) ― 쉬는 동안은 닫힌 그림 한 장
	const bool bLayered = bHasVisual && (LidAlpha > 0.f || GlowOpacity > 0.f);
	Show(ClosedImage, bHasVisual && !bLayered && ClosedTexture);
	Show(BodyImage, bLayered && BodyTexture);
	Show(GlowImage, bLayered && GlowOpacity > 0.f && GlowMID && GlowTexture);
	Show(FlapBackImage, bLayered && FrameAt(FlapBackTextures, FlapFrame));
	Show(FlapFrontImage, bLayered && FrameAt(FlapFrontTextures, FlapFrame));
}

FVector2D UGachaRevealBoxWidget::GetSeamPoint(EGachaSeam Which) const
{
	if (!bHasVisual)
	{
		return FVector2D::ZeroVector;
	}
	// LayerBox = 루트 캔버스 (0,0)·자동 크기(LayerSize) 저작이라 층 칸이 곧 위젯 로컬 ― 지오메트리 왕복은 렌더 배율 0 에서 NaN
	return SeamRatios[static_cast<int32>(Which)] * LayerSize;
}
