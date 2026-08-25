// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Buttons/MaterialIconButtonWidget.h"
#include "Components/Button.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Styling/SlateBrush.h"

void UMaterialIconButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 에디터 미리보기에서도 아이콘이 보이도록 설정
	SetupDynamicMaterial();
}

void UMaterialIconButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 런타임에서 Material 설정
	SetupDynamicMaterial();

	// 버튼 이벤트 바인딩
	if (MainButton)
	{
		MainButton->OnClicked.AddDynamic(this, &UMaterialIconButtonWidget::HandleButtonClicked);
		MainButton->OnHovered.AddDynamic(this, &UMaterialIconButtonWidget::HandleButtonHovered);
		MainButton->OnUnhovered.AddDynamic(this, &UMaterialIconButtonWidget::HandleButtonUnhovered);
	}
}

void UMaterialIconButtonWidget::NativeDestruct()
{
	// 이벤트 바인딩 해제
	if (MainButton)
	{
		MainButton->OnClicked.RemoveDynamic(this, &UMaterialIconButtonWidget::HandleButtonClicked);
		MainButton->OnHovered.RemoveDynamic(this, &UMaterialIconButtonWidget::HandleButtonHovered);
		MainButton->OnUnhovered.RemoveDynamic(this, &UMaterialIconButtonWidget::HandleButtonUnhovered);
	}

	Super::NativeDestruct();
}

void UMaterialIconButtonWidget::SetupDynamicMaterial()
{
	if (!MainButton)
	{
		return;
	}

	FButtonStyle Style = MainButton->GetStyle();
	UObject* ResourceObject = Style.Normal.GetResourceObject();

	if (!ResourceObject)
	{
		return;
	}

	UMaterialInterface* BaseMat = Cast<UMaterialInterface>(ResourceObject);

	if (!BaseMat)
	{
		UE_LOG(LogTemp, Error, TEXT("[MaterialIconButton] BaseMat is null!"));
		return;
	}

	// 이미 Dynamic Material이면 Parent를 가져와서 사용
	if (UMaterialInstanceDynamic* ExistingMID = Cast<UMaterialInstanceDynamic>(BaseMat))
	{
		// 이미 다른 위젯이 Dynamic으로 바꿔놓은 경우 - Parent에서 새로 생성
		BaseMat = ExistingMID->Parent;
		if (!BaseMat)
		{
			UE_LOG(LogTemp, Error, TEXT("[MaterialIconButton] MID Parent is null!"));
			return;
		}
	}

	// 이미 같은 Material로 DynamicMat을 생성했다면 재사용
	if (DynamicMat && CachedBaseMaterial == BaseMat)
	{
		// 텍스처만 업데이트
		if (IconTexture)
		{
			DynamicMat->SetTextureParameterValue(TextureParameterName, IconTexture);
		}
		return;
	}

	// 새로운 Dynamic Material Instance 생성
	// GetTransientPackage()를 Outer로 사용하여 UI Domain 검증 이슈 해결
	CachedBaseMaterial = BaseMat;
	DynamicMat = UMaterialInstanceDynamic::Create(BaseMat, GetTransientPackage());

	if (!DynamicMat)
	{
		UE_LOG(LogTemp, Error, TEXT("[MaterialIconButton] Failed to create DynamicMat!"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[MaterialIconButton] DynamicMat created successfully: %s"), *DynamicMat->GetName());

	// 텍스처 파라미터 설정
	if (IconTexture)
	{
		DynamicMat->SetTextureParameterValue(TextureParameterName, IconTexture);
	}

	// 버튼 스타일에 적용
	ApplyDynamicMaterialToButton();
}

void UMaterialIconButtonWidget::ApplyDynamicMaterialToButton()
{
	if (!MainButton || !DynamicMat)
	{
		return;
	}

	FButtonStyle Style = MainButton->GetStyle();

	// Brush 설정을 UI Material용으로 명시적 세팅
	auto ConfigureBrush = [this](FSlateBrush& Brush)
	{
		Brush.SetResourceObject(DynamicMat);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageType = ESlateBrushImageType::FullColor;
		Brush.Tiling = ESlateBrushTileType::NoTile;

		if (ImageSize.X > 0 && ImageSize.Y > 0)
		{
			Brush.SetImageSize(ImageSize);
		}
	};

	ConfigureBrush(Style.Normal);
	ConfigureBrush(Style.Hovered);
	ConfigureBrush(Style.Pressed);
	ConfigureBrush(Style.Disabled);

	MainButton->SetStyle(Style);
}

void UMaterialIconButtonWidget::SetImageSize(FVector2D NewSize)
{
	ImageSize = NewSize;

	if (!MainButton)
	{
		return;
	}

	FButtonStyle Style = MainButton->GetStyle();

	if (NewSize.X > 0 && NewSize.Y > 0)
	{
		Style.Normal.SetImageSize(NewSize);
		Style.Hovered.SetImageSize(NewSize);
		Style.Pressed.SetImageSize(NewSize);
		Style.Disabled.SetImageSize(NewSize);
	}

	MainButton->SetStyle(Style);
}

void UMaterialIconButtonWidget::SetIcon(UTexture2D* NewTexture)
{
	IconTexture = NewTexture;

	if (DynamicMat)
	{
		DynamicMat->SetTextureParameterValue(TextureParameterName, NewTexture);
	}
	else
	{
		// DynamicMat이 없으면 새로 설정
		SetupDynamicMaterial();
	}
}

void UMaterialIconButtonWidget::SetIconFromSoft(TSoftObjectPtr<UTexture2D> SoftIcon)
{
	if (!SoftIcon.IsNull())
	{
		UTexture2D* LoadedTexture = SoftIcon.LoadSynchronous();
		SetIcon(LoadedTexture);
	}
}

void UMaterialIconButtonWidget::SetButtonEnabled(bool bEnabled)
{
	if (MainButton)
	{
		MainButton->SetIsEnabled(bEnabled);
	}
}

bool UMaterialIconButtonWidget::IsButtonEnabled() const
{
	if (MainButton)
	{
		return MainButton->GetIsEnabled();
	}
	return false;
}

void UMaterialIconButtonWidget::SetScalarParameter(FName ParameterName, float Value)
{
	if (DynamicMat)
	{
		DynamicMat->SetScalarParameterValue(ParameterName, Value);
	}
}

void UMaterialIconButtonWidget::SetVectorParameter(FName ParameterName, FLinearColor Value)
{
	if (DynamicMat)
	{
		DynamicMat->SetVectorParameterValue(ParameterName, Value);
	}
}

void UMaterialIconButtonWidget::HandleButtonClicked()
{
	OnClicked.Broadcast();
}

void UMaterialIconButtonWidget::HandleButtonHovered()
{
	OnHovered.Broadcast();
}

void UMaterialIconButtonWidget::HandleButtonUnhovered()
{
	OnUnhovered.Broadcast();
}
