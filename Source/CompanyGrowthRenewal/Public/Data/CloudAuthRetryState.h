#pragma once

#include "CoreMinimal.h"

/**
 * 재인증 루프 종단 상태 (보안 리뷰 낮음13).
 * 서버가 **갓 발급된 토큰에도 401** 을 주면(프로젝트 불일치·시계 스큐·클레임 규칙 변경) 무한 루프다:
 * 401 → 교환 → 성공 → 카운터 0 → 재시도 → 401 → … 한 바퀴마다 `/auth/exchange` + `signInWithCustomToken` 이 영구히.
 * 그래서 "교환 성공"이 아니라 **"요청이 실제로 받아들여졌을 때"** 만 카운터를 리셋한다.
 * 순수 상태 기계라 자동화가 HTTP 없이 검증한다.
 */
struct FCloudAuthRetryState
{
	/** 갓 받은 토큰으로 이만큼 연속 거절되면 요청을 끊는다 */
	static constexpr int32 PostReauthRejectionThreshold = 3;

	// 재인증 직후의 401 횟수. 토큰 재발급으로 리셋되지 않는다
	int32 PostReauthRejections = 0;

	// 토큰을 새로 적용한 뒤 아직 "성공한 인증 요청"을 한 번도 못 본 상태
	bool bTokenFresh = false;

	// 백오프 잠금. true 면 교환·갱신·요청을 모두 중단한다(3초 폴링 재진입 차단)
	bool bBackoffLocked = false;

	/** 토큰 교환/갱신 성공. ⚠ 카운터를 리셋하지 않는다 ― 그게 무한 루프의 원인이었다 */
	void OnTokensApplied()
	{
		bTokenFresh = true;
	}

	/** 인증이 붙은 요청이 실제로 200 을 받았다 ― 여기서만 리셋한다 */
	void OnRequestAccepted()
	{
		PostReauthRejections = 0;
		bTokenFresh = false;
	}

	/** 인증이 붙은 요청이 401 을 받았다. 반환값 true = 이번 거절로 백오프 잠금에 들어갔다 */
	bool OnRequestRejected()
	{
		if (!bTokenFresh)
		{
			return false;
		}

		++PostReauthRejections;
		if (PostReauthRejections >= PostReauthRejectionThreshold && !bBackoffLocked)
		{
			bBackoffLocked = true;
			return true;
		}
		return false;
	}

	/** 포그라운드 복귀 등 사용자 계기로만 푼다(설계서 §3.2). 자동 해제는 루프를 되살린다 */
	void ClearBackoff()
	{
		PostReauthRejections = 0;
		bTokenFresh = false;
		bBackoffLocked = false;
	}
};
