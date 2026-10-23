#pragma once

#include "CoreMinimal.h"
#include "Data/LiveOpsSaveData.h"

// 접속·하루 첫 한가한 순간에 스스로 여는 라이브옵스 창. 한 번에 하나, 앞 창이 닫혀야 다음
enum class EEntryPopup : uint8
{
	None,
	Attendance,
	Notice,
};

struct FEntryPopupRules
{
	// 화면이 한가해진 뒤 이만큼 더 한가해야 연다 ― 창을 닫고 다음 창을 여는 사이(메뉴 → 출석)에 끼어들지 않는다
	static constexpr double SettleSeconds = 1.0;

	// 알림 글씨가 창 위에 겹치지 않게 사라질 때까지 기다리되, 알림이 줄줄이 와도 이 이상은 미루지 않는다
	static constexpr double ToastWaitMaxSeconds = 8.0;

	// 출석(받을 것이 있으면) → 새 공지 순서, 각각 하루 한 번. 튜토리얼을 막 끝낸 세션은 출석 하나만 ― 긴 튜토리얼 뒤 창을 줄줄이 띄우지 않는다
	static EEntryPopup ResolveNext(bool bCanClaimAttendance, bool bUnreadNotice, const FString& TodayKey,
		const FEntryPopupSaveData& Save, bool bTutorialThisSession)
	{
		if (TodayKey.IsEmpty())
		{
			return EEntryPopup::None;
		}
		if (bCanClaimAttendance && Save.LastAttendanceDayKey != TodayKey)
		{
			return EEntryPopup::Attendance;
		}
		if (bUnreadNotice && !bTutorialThisSession && Save.LastNoticeDayKey != TodayKey)
		{
			return EEntryPopup::Notice;
		}
		return EEntryPopup::None;
	}

	static void MarkShown(EEntryPopup Popup, const FString& TodayKey, FEntryPopupSaveData& Save)
	{
		if (Popup == EEntryPopup::Attendance)
		{
			Save.LastAttendanceDayKey = TodayKey;
		}
		else if (Popup == EEntryPopup::Notice)
		{
			Save.LastNoticeDayKey = TodayKey;
		}
	}
};
