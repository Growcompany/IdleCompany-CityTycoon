#pragma once
#include "CoreMinimal.h"

/**
 * 한국어 조사 판정 — 받침 유무로 갈리는 이/가 · 은/는 · 을/를 · 와/과 · (으)로.
 *
 * FText::Format 인자라면 엔진 수식어 `{0}|hpp(이,가)` 가 더 짧고 그쪽을 먼저 쓴다. 다만 두 경우엔 안 통한다:
 *   1. 숫자로 끝나는 값 — hpp 의 받침식 `(C - 0xAC00) % 28 != 0` 이 숫자에서 음수 나머지를 내
 *      항상 "받침 있음"이 된다. 즉 2·4·5·9 로 끝나면 "42을" 처럼 틀린다.
 *   2. FString 치환 경로 — 리뷰 코멘트 토큰처럼 ReplaceInline 으로 끼우는 값엔 hpp 자체가 없다.
 *
 * 위젯·매니저 의존 0 — 테스트가 곧 계약이다(CGR.Text.KoreanJosa).
 */
namespace KoreanJosa
{
	// 숫자 발음 받침: 0 영 · 1 일 · 3 삼 · 6 육 · 7 칠 · 8 팔 = 있음 / 2 이 · 4 사 · 5 오 · 9 구 = 없음
	inline bool DigitHasBatchim(TCHAR Digit)
	{
		return Digit == TEXT('0') || Digit == TEXT('1') || Digit == TEXT('3')
			|| Digit == TEXT('6') || Digit == TEXT('7') || Digit == TEXT('8');
	}

	// 발음 받침이 ㄹ 인 끝자리: 1 일 · 7 칠 · 8 팔
	inline bool DigitEndsWithRieul(TCHAR Digit)
	{
		return Digit == TEXT('1') || Digit == TEXT('7') || Digit == TEXT('8');
	}

	// 마지막 글자 받침. 라틴으로 끝나면 없음 취급("QA가")
	inline bool HasBatchim(const FString& Word)
	{
		if (Word.IsEmpty()) { return false; }
		const TCHAR C = Word[Word.Len() - 1];
		if (C >= TEXT('0') && C <= TEXT('9')) { return DigitHasBatchim(C); }
		if (C < 0xAC00 || C > 0xD7A3) { return false; }
		return ((C - 0xAC00) % 28) != 0;
	}

	// ㄹ 받침. 한글 완성형 종성 인덱스 8 이 ㄹ — "(으)로" 만 이걸 받침 없는 쪽으로 센다("서울로")
	inline bool EndsWithRieul(const FString& Word)
	{
		if (Word.IsEmpty()) { return false; }
		const TCHAR C = Word[Word.Len() - 1];
		if (C >= TEXT('0') && C <= TEXT('9')) { return DigitEndsWithRieul(C); }
		if (C < 0xAC00 || C > 0xD7A3) { return false; }
		return ((C - 0xAC00) % 28) == 8;
	}

	// 받침에 맞는 조사 하나. (으)로 는 Consonant 가 "으로" 인 걸로 식별한다 — 엔진 hpp 도 같은 방식이다
	inline const TCHAR* Pick(const FString& Word, const TCHAR* Vowel, const TCHAR* Consonant)
	{
		const bool bEuro = (FCString::Strcmp(Consonant, TEXT("으로")) == 0);
		const bool bBatchim = HasBatchim(Word) && !(bEuro && EndsWithRieul(Word));
		return bBatchim ? Consonant : Vowel;
	}

	inline FString Attach(const FString& Word, const TCHAR* Vowel, const TCHAR* Consonant)
	{
		return Word + Pick(Word, Vowel, Consonant);
	}

	/**
	 * "{토큰}조사" 를 "값 + 맞는 조사" 로 바꾼다. CSV 는 자연스러운 한 쪽만 적으면 된다("{최저분야}가").
	 * 값이 비면 조사까지 지운다 — 토큰만 비우면 "가 발목을 잡는다" 로 문장이 시작한다.
	 */
	inline void ReplaceTokenWithJosa(FString& InOut, const FString& Token, const FString& Value)
	{
		struct FPair { const TCHAR* Vowel; const TCHAR* Consonant; };
		// 2글자인 "으로" 를 "로" 보다 먼저 지워야 한다 — 순서가 뒤집히면 "{토큰}으로" 의 "으" 가 남는다
		static const FPair Pairs[] = {
			{ TEXT("가"), TEXT("이")       },  // 가 / 이
			{ TEXT("는"), TEXT("은")       },  // 는 / 은
			{ TEXT("를"), TEXT("을")       },  // 를 / 을
			{ TEXT("와"), TEXT("과")       },  // 와 / 과
			{ TEXT("로"), TEXT("으로") },  // 로 / 으로
		};

		for (const FPair& P : Pairs)
		{
			const FString Fixed = Value.IsEmpty() ? FString() : Attach(Value, P.Vowel, P.Consonant);
			InOut.ReplaceInline(*(Token + P.Consonant), *Fixed);
			InOut.ReplaceInline(*(Token + P.Vowel), *Fixed);
		}
		InOut.ReplaceInline(*Token, *Value);
	}
}
