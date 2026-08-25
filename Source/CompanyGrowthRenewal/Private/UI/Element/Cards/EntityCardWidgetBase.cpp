// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Cards/EntityCardWidgetBase.h"
#include "UI/Element/Cards/EntityCardFrameWidget.h"
#include "UI/Element/Cards/CardInfoWidget.h"
#include "Components/Image.h"
#include "Engine/AssetManager.h"

void UEntityCardWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	// 버튼 이벤트 바인딩
	OnClicked().AddUObject(this, &UEntityCardWidgetBase::OnButtonClicked);
}

void UEntityCardWidgetBase::NativeDestruct()
{
	Super::NativeDestruct();

	OnClicked().RemoveAll(this);
}

void UEntityCardWidgetBase::NativeOnHovered()
{
	Super::NativeOnHovered();
	bIsHovered = true;
	RefreshTactileTransform();
}

void UEntityCardWidgetBase::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();
	bIsHovered = false;
	// 카드 밖에서 손을 떼면 Released 가 안 오므로 여기서 같이 푼다.
	bIsPressed = false;
	RefreshTactileTransform();
}

void UEntityCardWidgetBase::NativeOnPressed()
{
	Super::NativeOnPressed();
	bIsPressed = true;
	RefreshTactileTransform();
}

void UEntityCardWidgetBase::NativeOnReleased()
{
	Super::NativeOnReleased();
	bIsPressed = false;
	RefreshTactileTransform();
}

void UEntityCardWidgetBase::RefreshTactileTransform()
{
	// 모바일엔 호버가 없다 — 실제로 쓰이는 채널은 Sink 쪽이므로 여기에 힘을 싣는다.
	const float Lift = (bIsHovered && !bIsPressed) ? 3.0f : 0.0f;
	const float Sink = bIsPressed ? 4.0f : 0.0f;

	FWidgetTransform CardXf;
	CardXf.Scale = FVector2D(bIsPressed ? 0.985f : 1.0f);
	CardXf.Translation = FVector2D(0.0f, Sink - Lift);
	SetRenderTransform(CardXf);

	// 그림자/측면은 카드가 움직인 만큼 반대로 상쇄해 화면상 제자리에 남긴다.
	// 같이 따라 움직이면 부양감도 눌림도 생기지 않는다.
	if (UIE_EntityCardFrame)
	{
		UIE_EntityCardFrame->SetShadowLift(Lift);
		UIE_EntityCardFrame->SetPressSink(Sink);
	}
}

void UEntityCardWidgetBase::NativeOnEntryReleased()
{
	IUserObjectListEntry::NativeOnEntryReleased();

	// 눌린 채 스크롤로 밀려나면 트랜스폼이 남은 상태로 다음 건물에 재사용된다.
	bIsHovered = false;
	bIsPressed = false;
	RefreshTactileTransform();
}

void UEntityCardWidgetBase::OnButtonClicked()
{
	// 잠금 상태면 클릭 무시
	if (bIsCurrentlyLocked)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EntityCardWidgetBase] Clicked but LOCKED - ignoring!"));
		return;
	}

	// 자식 클래스의 구현 호출
	HandleCardClicked();
}

void UEntityCardWidgetBase::UpdateLockUI(bool bIsLocked, const FText& LockReason)
{
	bIsCurrentlyLocked = bIsLocked;

	if (UIE_EntityCardFrame)
	{
		UIE_EntityCardFrame->SetLocked(bIsLocked, LockReason);
	}
}

void UEntityCardWidgetBase::OnIconLoaded(FSoftObjectPath LoadedPath)
{
	UObject* LoadedObj = LoadedPath.ResolveObject();
	if (!LoadedObj)
	{
		LoadedObj = LoadedPath.TryLoad();
	}

	if (UTexture2D* Tex = Cast<UTexture2D>(LoadedObj))
	{
		if (UImage* Image = GetEntityImage())
		{
			Image->SetBrushFromTexture(Tex);
		}
	}
}

UImage* UEntityCardWidgetBase::GetEntityImage() const
{
	if (UIE_EntityCardFrame)
	{
		return UIE_EntityCardFrame->GetEntityImage();
	}
	return nullptr;
}

void UEntityCardWidgetBase::SetSelected(bool bInSelected)
{
	bIsSelected = bInSelected;

	// CommonButtonBase의 기본 Selected 스타일 사용
	SetSelectedInternal(bInSelected, false);
}
