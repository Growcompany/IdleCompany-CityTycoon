#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CGQABridge.generated.h"

namespace CGQABridgePolicy
{
	COMPANYGROWTHRENEWAL_API bool IsReflectionBuildMapAllowed(const FString& MapPackageName);
	COMPANYGROWTHRENEWAL_API bool IsMainMapVisualProductionFeatureLevelSupported(
		ERHIFeatureLevel::Type FeatureLevel);
	COMPANYGROWTHRENEWAL_API bool IsMainMapVisualProductionWorldReadyForPhase(
		bool bAsyncLoading,
		bool bExpectedStreamingLevelLoaded,
		bool bStreamingStatePending,
		bool bHasEndOfFrameUpdates,
		bool bRequireEndOfFrameDrain);
}

/**
 * QA Control Center 전송로 — 외부 도구(Tools/QAControl)가 원격 파이썬으로 부르는 유일한 진입점.
 *
 * 치트는 void 라서 콘솔로 쏘면 "실행했다" 와 "무슨 일이 났나" 가 분리된다. 그 틈을 메우려면
 * 로그 파일을 tail 하며 마커로 구간을 잘라내야 하는데(인코딩·로테이션·타임아웃), 엔진 안에서는
 * 출력을 그 자리에서 가로채 그대로 돌려주면 된다. 반환값이 있는 것이 이 클래스의 존재 이유다.
 *
 * 반환은 전부 JSON 한 줄 — 원격 파이썬이 문자열 하나만 실어 나르면 되게.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UCGQABridge : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 콘솔 명령을 실행하고 그동안 로그로 나간 줄을 함께 돌려준다.
	 * 반환 JSON: { ok, world, map, pcClass, console, truncated, lines:[{cat,verbosity,text}] }
	 */
	UFUNCTION(BlueprintCallable, Category = "CG|QA", meta = (WorldContext = "WorldContextObject"))
	static FString ExecAndCapture(UObject* WorldContextObject, const FString& Command);

	/**
	 * 컨텍스트 게이트가 쓰는 상태를 한 번에 조회한다 (맵/선택 직원/관리 빌딩/액터 유무).
	 * 항목마다 따로 왕복하면 명령 하나 쏘는 데 원격 호출이 여러 번 붙는다.
	 * 반환 JSON: { ok, world, map, pcClass, selectedEmployeeID, managedBuildingIndex,
	 *              hasTimeCycleManager, brickFactoryCount }
	 */
	UFUNCTION(BlueprintCallable, Category = "CG|QA", meta = (WorldContext = "WorldContextObject"))
	static FString DescribeContext(UObject* WorldContextObject);

	/**
	 * 고정 E6 manager mode와 startup map을 검증하고 bounded editor readiness 뒤에만 실행한다.
	 * clone/cleanup은 source MainMap, prepare/build/verify는 transient E6 map에서만 허용하며,
	 * reflection으로 확인한 bValidateOnSave=false 없이는 어떤 delegate도 등록하지 않는다.
	 * prepare는 PostEditorTick readiness 2회 -> 다음 BeginFrame spawn -> 정상 editor tick ->
	 * 같은 frame PostEditorTick에서 검증과 save를 연속 수행하고, 다른 mode는 readiness 뒤 한 번만 dispatch한다.
	 * 반환 JSON: { ok, scheduled, operation, mode, mapPackage, phase,
	 *              readyFrame, spawnFrame, stableProbeSuccesses, mapBuildDataId,
	 *              validationSuppressed }
	 * 최초 prepare는 phase=readiness, 동기 spawn 재진입은 phase=prepareSave를 반환한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "CG|QA")
	static FString ScheduleEditorEndFrameE6Manager();

	/**
	 * MainMap 비주얼 프로덕션 prepare/build/verify를 고정 스크립트와 고정 맵에서만 예약한다.
	 * prepare는 안정화된 PostEditorTick 뒤 다음 BeginFrame에서 mutation/spawn을 실행하고,
	 * 정상 editor tick 뒤 같은 frame PostEditorTick에서 검증과 단 한 번의 맵 저장을 실행한다.
	 * build/verify는 별도 프로세스에서 readiness 뒤 각각 한 번만 dispatch한다.
	 * 반환 JSON: { ok, scheduled, operation, mode, mapPackage, phase,
	 *              readyFrame, spawnFrame, stableProbeSuccesses, dispatchCount,
	 *              mapBuildDataId, validationSuppressed }
	 */
	UFUNCTION(BlueprintCallable, Category = "CG|QA")
	static FString ScheduleEditorMainMapVisualProduction();

	/**
	 * 허용된 MainMap의 정적 Sphere Reflection Capture를 빌드하고 직렬화 payload 전체와 저장 결과를 검증한다.
	 * 반환 JSON: { ok, mapPackage, actorCount, actorLabel, mapBuildDataId,
	 *              componentRegistered, builtDataPresent, serializedPayloadPresent,
	 *              uploadedFinal, encodedResourcePresent, payloadState, cubemapSize,
	 *              fullHDRBytes, encodedHDRBytes, allocatedBytes, mapDataSaved }
	 */
	UFUNCTION(BlueprintCallable, Category = "CG|QA")
	static FString BuildEditorReflectionCaptures(const FString& ExpectedMapPackage);

	/**
	 * 저장·재로드된 허용 MainMap Reflection Capture BuiltData를 변경 없이 조회한다.
	 * 반환 JSON: { ok, mapPackage, actorCount, actorLabel, mapBuildDataId,
	 *              componentRegistered, builtDataPresent, serializedPayloadPresent,
	 *              uploadedFinal, encodedResourcePresent, payloadState, cubemapSize,
	 *              fullHDRBytes, encodedHDRBytes, allocatedBytes }
	 * payloadState: Serialized | EncodedResource | UploadedFinal
	 */
	UFUNCTION(BlueprintCallable, Category = "CG|QA", meta = (WorldContext = "WorldContextObject"))
	static FString DescribeEditorReflectionCaptureBuildData(
		UObject* WorldContextObject,
		const FString& ExpectedMapPackage);

	// ===== 장기 실행 명령 (Balance_GateRun 처럼 타이머로 수십 초에 걸쳐 결과가 나오는 것) =====
	//
	// ExecAndCapture 는 ConsoleCommand 가 반환하는 순간 싱크를 뗀다. 타이머로 도는 명령은
	// 그 시점에 아직 아무것도 안 나왔으므로 결과가 통째로 유실된다.
	//
	// 여기서 기다릴 수는 없다 — 우리는 게임스레드 위에 있고, 블로킹하면 타이머가 영영 안 돈다.
	// 그래서 "싱크를 붙인 채로 반환하고, 밖에서 주기적으로 긁어간다".

	/** 캡처를 시작하고 명령을 쏜 뒤 즉시 반환. 반환 JSON: { ok, sessionId, world, map, console, lines } */
	UFUNCTION(BlueprintCallable, Category = "CG|QA", meta = (WorldContext = "WorldContextObject"))
	static FString BeginCapture(UObject* WorldContextObject, const FString& Command);

	/** 그동안 쌓인 줄을 비워서 돌려준다. DoneMarker 를 본 줄이 있으면 done=true.
	 *  반환 JSON: { ok, done, elapsed, truncated, lines } */
	UFUNCTION(BlueprintCallable, Category = "CG|QA")
	static FString PollCapture(const FString& SessionId, const FString& DoneMarker);

	/** 캡처를 끝내고 싱크를 뗀다. 호출하지 않아도 오래된 세션은 다음 BeginCapture 가 정리한다. */
	UFUNCTION(BlueprintCallable, Category = "CG|QA")
	static FString EndCapture(const FString& SessionId);
};
