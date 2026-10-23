// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/CloudEconomyData.h"
#include "Data/FacilityInstantFinishTransaction.h"
#include "CloudEconomySubsystem.generated.h"

class UResourceItemManager;
class UItemInventoryManager;
class UShopManagerSubsystem;
class UAttendanceSubsystem;
class URecruitmentManagerSubsystem;
class USaveLoadManager;
struct FCloudResponse;

/**
 * 경제가 잠긴 이유 3분류 (설계서 §7.2 · §7.7).
 * 세 상태는 **서로 다른 문구**를 받아야 한다 ― "동기화 중" 을 영구 차단에 띄우면 거짓말이고,
 * 아무것도 띄우지 않으면 플레이어가 재화 분실과 구분하지 못한다.
 */
UENUM(BlueprintType)
enum class ECloudBlockState : uint8
{
	// 잠기지 않았다(`bEconomyReady == true`)
	None,
	// 인증·상태조회가 진행 중 ― 기다리면 풀린다
	Syncing,
	// 실패했고 재시도로 풀린다(`MAINTENANCE`·백오프 잠금 포함)
	RetryPending,
	// 재시도로 풀리지 않는다(`FORBIDDEN` = prod 에서 비Shipping 빌드)
	PermanentlyBlocked
};

/**
 * 상태 적용 대상 묶음.
 * 매니저 5개를 순차로 건드리므로 "하나라도 없으면 아무것도 적용하지 않는다"를 한곳에서 판정해야 한다(반쪽 상태 방지).
 * 밖으로 뺀 이유 = 자동화가 전용 GameInstance 에 만든 매니저를 주입해 검증한다.
 */
struct COMPANYGROWTHRENEWAL_API FCloudEconomyApplyTargets
{
	UResourceItemManager* Resources = nullptr;
	UItemInventoryManager* Items = nullptr;
	UShopManagerSubsystem* Shop = nullptr;
	UAttendanceSubsystem* Attendance = nullptr;
	URecruitmentManagerSubsystem* Recruitment = nullptr;

	/** 적용 성공 뒤 캐시 미러 저장 1회. null 이면 저장을 건너뛴다(테스트) */
	USaveLoadManager* Save = nullptr;
};

/**
 * 서버 경제 상태 클라 레일 (설계서 §6.3 · §7.2).
 * - `FetchState()` → `GET /player/state` → `ApplyServerEconomyState()` → `bEconomyReady`
 * - 지출/크레딧/소모 요청의 단일 관문. 호출부 재배선은 1-B-4 다.
 * - `bEconomyReady == false` 동안 세 `Request*` 는 **요청을 보내지 않고** 즉시 실패한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UCloudEconomySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 경제 잠금/해제. UI 가 구독해 버튼을 비활성화한다(§7.2)
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnEconomyReady, bool);
	FOnEconomyReady OnEconomyReady;

	// 변경 요청 1건의 완료. 성공·실패 어느 쪽이든 정확히 1회 호출된다(빈 콜백 금지)
	DECLARE_DELEGATE_OneParam(FOnCloudOp, const FCloudOpResult&);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** `GET /player/state`. 인증·세이브 로드가 모두 끝난 뒤에만 실제로 보낸다(§7.2 ③) */
	void FetchState();

	UFUNCTION(BlueprintPure, Category = "CloudEconomy")
	bool IsEconomyReady() const { return bEconomyReady; }

	const FServerEconomyState& GetLastState() const { return LastState; }

	/** 검증 → 매니저 주입 → `SaveGameData()` 1회. 검증 실패면 매니저를 건드리지 않고 false */
	bool ApplyServerEconomyState(const FServerEconomyState& State);

	/** 위와 같은 규칙으로 주어진 대상에 적용한다(자동화 주입 경로) */
	static bool ApplyServerEconomyStateTo(
		const FServerEconomyState& State,
		const FCloudEconomyApplyTargets& Targets,
		FString& OutError);

	// ===== 변경 요청 (호출부는 1-B-4) =====

	/** `POST /wallet/spend`. Currency = `CloudCurrency::Diamond`|`Mileage`, Reason = `spendRules` 허용목록 */
	void RequestSpend(FName Currency, int64 Amount, FName Reason, const FCloudMeta& Meta, FOnCloudOp Done);
	void RequestFacilityInstantFinish(FName Reason, int64 Amount, const FCloudMeta& Meta, FOnCloudOp Done);
	bool HasPendingFacilityInstantFinish(FName Reason, int32 Country, int32 Identity) const;

	/** `POST /wallet/credit`. `trade.diamond` 는 캡 초과를 클램프하므로 표시엔 `result.credited` 를 쓸 것 */
	void RequestCredit(FName Source, const FString& ClaimKey, int64 Amount, FOnCloudOp Done);

	/** `POST /item/consume`. reason↔item 결속은 서버가 400 으로 지킨다(§6.3) */
	void RequestConsume(FName Reason, FName Item, int32 Amount, FOnCloudOp Done);

	// ===== 지급 응답 해석 (순수 판정 ― 자동화가 월드 없이 부른다) =====

	/**
	 * `ALREADY_CLAIMED`(409) 는 오류가 아니다 ― 이미 받은 것이다.
	 * 실패로 다루면 "지급 실패" 안내가 뜨고 플레이어는 버그로 읽는다.
	 */
	static bool IsClaimBenign(FName Code);

	/**
	 * 서버가 실제로 지급한 액수. `trade.diamond` 는 캡 초과를 거절이 아니라 **클램프**하므로
	 * 요청 상수를 표시하면 클램프를 숨긴다. 응답을 못 읽었으면 0 ― 없는 지급을 표시하지 않는다.
	 */
	static int32 ResolveCreditedAmount(int32 Requested, int32 ServerCredited);

	/** 응답 `result.credited` 를 읽어 위 규칙을 적용한다 */
	static int32 ReadCreditedAmount(const FCloudOpResult& OpResult, int32 Requested);

	// ===== 잠금 안내 (순수 판정 ― 자동화가 월드 없이 부른다) =====

	/**
	 * 잠긴 이유를 3분류한다. `FORBIDDEN` 이 최우선 ― 재시도가 진행 중이어도 풀리지 않으므로
	 * "동기화 중" 으로 보이면 거짓말이 된다. 그 다음이 진행 중, 나머지는 재시도 예정.
	 * `MAINTENANCE` 는 영구 차단이 아니다(재시도로 풀린다).
	 * 잠기지 않은 상태는 이 함수가 판정하지 않는다 ― `None` 은 `GetBlockState()` 가 낸다.
	 */
	static ECloudBlockState ClassifyBlock(FName LastFailureCode, bool bQueryInProgress);

	/**
	 * 3상태 안내 문구. `CodeText` = `DT_CloudErrorText` 가 준 코드 문구(영구 차단에서만 앞세운다).
	 * 나머지 두 상태는 코드 문구를 쓰지 않는다 ― `INTERNAL` 행이 "잠시 후 다시 시도해 주세요" 라서
	 * 재시도 문구와 겹쳐 같은 말이 두 번 나온다.
	 */
	static FText BlockStateMessage(ECloudBlockState State, const FText& CodeText);

	/**
	 * 접점 고유 "부족합니다" 문구를 유지할 코드인가. 잔액·재고 부족만 true ―
	 * 나머지(네트워크·게이트·서버 내부)에 부족 문구를 쓰면 거짓 원인을 안내한다.
	 */
	static bool ShouldKeepShortfallText(FName Code);

	/** 지금 잠겼는가 + 왜. 준비됐으면 `None` */
	UFUNCTION(BlueprintPure, Category = "CloudEconomy")
	ECloudBlockState GetBlockState() const;

	/** 마지막 잠금 원인 코드(성공으로 풀리면 비워진다) */
	FName GetLastBlockCode() const { return LastBlockCode; }

	/** 인증 교환 또는 `/player/state` 가 진행 중인가 = "기다리면 풀린다" */
	bool IsCloudQueryInProgress() const;

	// ===== 중복 클릭 차단 (§6.3 "pending 플래그") =====

	/** 이미 진행 중이면 false. true 를 받았으면 반드시 `EndPendingAction` 으로 짝을 맞출 것 */
	bool BeginPendingAction(FName ActionKey);
	void EndPendingAction(FName ActionKey);
	bool IsActionPending(FName ActionKey) const { return PendingActions.Contains(ActionKey); }

private:
	void RequestLocalSpend(FName Currency, int64 Amount, FName Reason, const FCloudMeta& Meta, FOnCloudOp Done);
	void DispatchFacilityInstantFinish(const FFacilityInstantFinishTransaction& Transaction, FOnCloudOp Done);
	void FinishFacilityInstantFinish(const FString& OpId, const FCloudOpResult& Result, FOnCloudOp Done);
	void ResumePendingFacilityInstantFinishes();
	void ScheduleFacilityInstantFinishRetry(const FString& OpId);
	void RequestLocalCredit(FName Source, const FString& ClaimKey, int64 Amount, FOnCloudOp Done);
	void RequestLocalConsume(FName Reason, FName Item, int32 Amount, FOnCloudOp Done);
	void CompleteLocalOp(FOnCloudOp& Done, bool bOk, FName Code, int32 HttpStatus,
		const TSharedPtr<FJsonObject>& Result = TSharedPtr<FJsonObject>()) const;
	void RefreshLocalState();
	bool EnsureLocalSaveCache(USaveLoadManager*& OutSaveManager) const;

	void HandleAuthReady(bool bReady);
	void HandleGameDataLoaded();
	void SetEconomyReady(bool bReady);
	void ApplyStateFromResponse(const FCloudResponse& Response);

	FCloudEconomyApplyTargets ResolveTargets() const;
	FString AcquireIdToken() const;

	/** 게이트 거절 ― 요청을 보내지 않는다. 문구는 `AUTH_REQUIRED`("서버와 다시 연결하고 있습니다") */
	bool RejectWhenNotReady(const TCHAR* Action, FOnCloudOp& Done);

	void SendEconomyOp(
		const FString& Path,
		const FString& Action,
		const FString& OpId,
		const FString& Body,
		FOnCloudOp Done,
		bool bAllowAuthRetry);

	void HandleOpResponse(
		const FString& Path,
		const FString& Action,
		const FString& OpId,
		const FString& Body,
		FOnCloudOp Done,
		bool bAllowAuthRetry,
		double StartSeconds,
		const FCloudResponse& Response);

	bool bEconomyReady = false;
	bool bAuthReady = false;
	bool bSaveLoaded = false;
	bool bFetchInFlight = false;

	// `/player/state` 를 막은 마지막 코드. 안내 문구가 원인별로 갈리려면 코드가 남아 있어야 한다
	FName LastBlockCode;

	FServerEconomyState LastState;

	// 같은 액션의 중복 클릭 차단 키
	TSet<FName> PendingActions;
	TSet<FString> FacilityOpsInFlight;
	TSet<FString> FacilityAccountMismatchLogged;
	TMap<FString, int32> FacilityRetryCounts;
	FTimerHandle FacilityRetryTimer;
};
