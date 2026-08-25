// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Chat/ChatMessageWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Manager/PlayFabManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/ProfileImageData.h"

namespace
{
	// 이름 잉크 (linear) — 남 #93A6C0 / 나 #3D9BE0(액센트 블루)
	const FLinearColor NameInkOther(0.291740f, 0.381320f, 0.527170f);
	const FLinearColor NameInkMine(0.047000f, 0.328000f, 0.745000f);

	// ISO 8601(UTC) → 로컬 HH:MM. 파싱 실패하면 빈 문자열로 두어 칸을 조용히 비운다.
	FString FormatLocalHourMinute(const FString& Iso8601)
	{
		FDateTime Utc;
		if (Iso8601.IsEmpty() || !FDateTime::ParseIso8601(*Iso8601, Utc))
		{
			return FString();
		}

		const FTimespan LocalOffset = FDateTime::Now() - FDateTime::UtcNow();
		return (Utc + LocalOffset).ToString(TEXT("%H:%M"));
	}
}

void UChatMessageWidget::SetMessageData(const FChatMessage& Message)
{
	if (SenderNameText)
	{
		SenderNameText->SetText(FText::FromString(Message.SenderName));
		SenderNameText->SetColorAndOpacity(IsFromLocalPlayer(Message.SenderId) ? NameInkMine : NameInkOther);
	}

	if (MessageContentText)
	{
		MessageContentText->SetText(FText::FromString(Message.Content));
	}

	if (TimestampText)
	{
		TimestampText->SetText(FText::FromString(FormatLocalHourMinute(Message.CreatedAt)));
	}

	ApplyProfileImage(Message.ProfileImageID);
}

void UChatMessageWidget::ApplyProfileImage(int32 ProfileImageID)
{
	if (!EntityImage)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		return;
	}

	bool bSuccess = false;
	const FProfileImageData ImgData = TableMgr->GetProfileImageData(ProfileImageID, bSuccess);
	if (!bSuccess || ImgData.Icon.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ChatMessage] DT_ProfileImage에 ImageID %d 행이 없거나 Icon이 비어 있음"), ProfileImageID);
		return;
	}

	if (UTexture2D* Texture = ImgData.Icon.LoadSynchronous())
	{
		EntityImage->SetBrushFromTexture(Texture);
	}
}

bool UChatMessageWidget::IsFromLocalPlayer(const FString& SenderId) const
{
	if (SenderId.IsEmpty())
	{
		return false;
	}

	UGameInstance* GI = GetGameInstance();
	UPlayFabManagerSubsystem* PFMgr = GI ? GI->GetSubsystem<UPlayFabManagerSubsystem>() : nullptr;

	return PFMgr && PFMgr->IsLoggedIn() && PFMgr->GetUserInfo().PlayFabId == SenderId;
}
