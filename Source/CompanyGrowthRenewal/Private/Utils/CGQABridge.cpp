#include "Utils/CGQABridge.h"

#include "Core/CGGameInstance.h"
#include "Manager/EmployeeManager.h"
#include "TimeCycle/TimeCycleManager.h"
#include "Entity/Factory/BrickFactory.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Logging/LogVerbosity.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_EDITOR
#include "Components/ReflectionCaptureComponent.h"
#include "Components/SphereReflectionCaptureComponent.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Engine/LevelStreaming.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Engine/SphereReflectionCapture.h"
#include "Engine/TextureCube.h"
#include "FileHelpers.h"
#include "LevelInstance/LevelInstanceLevelStreaming.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#endif

namespace
{
	// 한 명령이 로그를 폭주시켜도 도구가 메모리를 통째로 먹지 않게 상한을 둔다.
	// 잘렸다는 사실은 반환 JSON 의 truncated 로 드러난다 — 조용히 버리지 않는다.
	constexpr int32 MaxCapturedLines = 4000;

	/** 명령이 도는 동안 GLog 로 나가는 줄을 모으는 임시 싱크. */
	class FCGQALogSink : public FOutputDevice
	{
	public:
		struct FLine
		{
			FName Category;
			FString Verbosity;
			FString Text;
		};

		TArray<FLine> Lines;
		bool bTruncated = false;

		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			// GLog 는 아무 스레드에서나 들어온다 — 게임스레드 명령이 도는 동안 로딩 스레드가
			// 같이 찍으면 TArray 가 깨진다.
			FScopeLock Lock(&Mutex);
			if (Lines.Num() >= MaxCapturedLines)
			{
				bTruncated = true;
				return;
			}
			Lines.Add(FLine{ Category, FString(::ToString(Verbosity)), FString(Message) });
		}

		virtual bool CanBeUsedOnAnyThread() const override { return true; }
		virtual bool CanBeUsedOnMultipleThreads() const override { return true; }

		/** 쌓인 줄을 가져가고 비운다 — 폴링 한 번에 같은 줄이 두 번 오지 않게. */
		TArray<FLine> Drain()
		{
			FScopeLock Lock(&Mutex);
			TArray<FLine> Out = MoveTemp(Lines);
			Lines.Reset();
			return Out;
		}

	private:
		FCriticalSection Mutex;
	};

	/** 장기 실행 캡처 세션. 싱크가 GLog 에 붙은 채로 살아 있어야 해서 전역에 둔다. */
	struct FCaptureSession
	{
		TSharedPtr<FCGQALogSink> Sink;
		FString Command;
		double StartedAt = 0.0;
	};

	TMap<FString, FCaptureSession>& Sessions()
	{
		static TMap<FString, FCaptureSession> Map;
		return Map;
	}

	// 도구가 죽어 EndCapture 를 못 부르면 싱크가 GLog 에 영원히 붙어 메모리를 먹는다.
	// 새 세션을 열 때마다 오래된 것을 걷어낸다.
	constexpr double SessionMaxSeconds = 900.0;

	void CloseSession(const FString& SessionId)
	{
		if (FCaptureSession* Session = Sessions().Find(SessionId))
		{
			if (Session->Sink.IsValid())
			{
				GLog->Flush();
				GLog->RemoveOutputDevice(Session->Sink.Get());
			}
			Sessions().Remove(SessionId);
		}
	}

	void SweepStaleSessions()
	{
		const double Now = FPlatformTime::Seconds();
		TArray<FString> Expired;
		for (const TPair<FString, FCaptureSession>& Pair : Sessions())
		{
			if (Now - Pair.Value.StartedAt > SessionMaxSeconds)
			{
				Expired.Add(Pair.Key);
			}
		}
		for (const FString& Key : Expired)
		{
			UE_LOG(LogTemp, Warning, TEXT("[CGQABridge] 오래된 캡처 세션을 정리합니다: %s"), *Key);
			CloseSession(Key);
		}
	}

	void AppendLines(const TSharedRef<FJsonObject>& Root, const TArray<FCGQALogSink::FLine>& Lines)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Reserve(Lines.Num());
		for (const FCGQALogSink::FLine& Line : Lines)
		{
			const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("cat"), Line.Category.ToString());
			Entry->SetStringField(TEXT("verbosity"), Line.Verbosity);
			Entry->SetStringField(TEXT("text"), Line.Text);
			Values.Add(MakeShared<FJsonValueObject>(Entry));
		}
		Root->SetArrayField(TEXT("lines"), Values);
	}

	FString ToJsonLine(const TSharedRef<FJsonObject>& Root)
	{
		FString Out;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Root, Writer);
		return Out;
	}

	FString ErrorJson(const FString& Code, const FString& Message)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetBoolField(TEXT("ok"), false);
		Root->SetStringField(TEXT("code"), Code);
		Root->SetStringField(TEXT("message"), Message);
		return ToJsonLine(Root);
	}

	UWorld* ResolveWorld(UObject* WorldContextObject)
	{
		if (!GEngine)
		{
			return nullptr;
		}
		return GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	}

	const FString& GetE6ReflectionMapPackage()
	{
		static const FString Package(
			TEXT("/Game/__CGRTransient/MobileVisualAB/E6/MainMap_TheRiverwalkCity_E6"));
		return Package;
	}

	const FString& GetE6ManagerSourceMapPackage()
	{
		static const FString Package(TEXT("/Game/CompanyGrowth/Level/MainMap_TheRiverwalkCity"));
		return Package;
	}

#if WITH_EDITOR
	const FString& GetE6ReflectionActorLabel()
	{
		static const FString Label(TEXT("CGR_E6_StaticReflectionCapture"));
		return Label;
	}

	const FString& GetE6ManagerExpectedStreamingLevelPackage()
	{
		static const FString Package(TEXT("/Game/CompanyGrowth/Level/TheRiverwalkInstance"));
		return Package;
	}

	bool IsExpectedE6ManagerStreamingLevelInstance(const ULevelStreaming* StreamingLevel)
	{
		return StreamingLevel
			&& StreamingLevel->IsA<ULevelStreamingLevelInstance>()
			&& StreamingLevel->PackageNameToLoad
				== FName(*GetE6ManagerExpectedStreamingLevelPackage());
	}

	enum class ECGE6ManagerEndFramePhase : uint8
	{
		Idle,
		ReadinessPending,
		BeginFramePending,
		PrimaryDispatching,
		PrepareSpawnDispatching,
		PrepareVerificationPending,
		PrepareSaveDispatching,
		Terminal
	};

	struct FCGE6ManagerEndFrameState
	{
		FDelegateHandle BeginFrameHandle;
		FDelegateHandle PostEditorTickHandle;
		ECGE6ManagerEndFramePhase Phase = ECGE6ManagerEndFramePhase::Idle;
		bool bDispatchConsumed = false;
		int32 DispatchCount = 0;
		int32 StableProbeSuccesses = 0;
		int32 StableProbeDispatches = 0;
		double StableProbeStartedAt = 0.0;
		uint64 ReadyFrame = 0;
		uint64 SpawnFrame = 0;
		uint64 VerificationFrame = 0;
		FGuid PrepareMapBuildDataId;
		FString ScheduledMode;
		FString ScheduledMapPackage;
		FString ScheduledScriptPath;
	};

	FCGE6ManagerEndFrameState& GetE6ManagerEndFrameState()
	{
		static FCGE6ManagerEndFrameState State;
		return State;
	}

	const FString& GetMainMapVisualProductionMapPackage()
	{
		static const FString Package(TEXT("/Game/CompanyGrowth/Level/MainMap_TheRiverwalkCity"));
		return Package;
	}

	const FString& GetMainMapVisualProductionScriptRelativePath()
	{
		static const FString RelativePath(
			TEXT("Tools/MainMapPreview/apply_mainmap_visual_production.py"));
		return RelativePath;
	}

	enum class ECGMainMapVisualProductionPhase : uint8
	{
		Idle,
		ReadinessPending,
		BeginFramePending,
		PrepareMutationDispatching,
		PrepareVerificationPending,
		PrepareSaveDispatching,
		PrimaryDispatching,
		Terminal
	};

	struct FCGMainMapVisualProductionState
	{
		FDelegateHandle BeginFrameHandle;
		FDelegateHandle PostEditorTickHandle;
		ECGMainMapVisualProductionPhase Phase = ECGMainMapVisualProductionPhase::Idle;
		bool bDispatchConsumed = false;
		int32 DispatchCount = 0;
		int32 StableProbeSuccesses = 0;
		int32 StableProbeDispatches = 0;
		double StableProbeStartedAt = 0.0;
		uint64 ReadyFrame = 0;
		uint64 SpawnFrame = 0;
		uint64 VerificationFrame = 0;
		FGuid PrepareMapBuildDataId;
		FString ScheduledMode;
		FString ScheduledMapPackage;
		FString ScheduledScriptPath;
	};

	FCGMainMapVisualProductionState& GetMainMapVisualProductionState()
	{
		static FCGMainMapVisualProductionState State;
		return State;
	}

	struct FCGReflectionCaptureInventory
	{
		ASphereReflectionCapture* Actor = nullptr;
		USphereReflectionCaptureComponent* Component = nullptr;
		int32 ComponentCount = 0;
		FString ActorLabel;
	};

	enum class ECGReflectionPayloadRequirement : uint8
	{
		RequireSerialized,
		AllowSerializedOrRuntimeConsumed
	};

	struct FCGReflectionCaptureBuildDataSnapshot
	{
		FGuid MapBuildDataId;
		bool bComponentRegistered = false;
		bool bSerializedPayloadPresent = false;
		bool bUploadedFinal = false;
		bool bEncodedResourcePresent = false;
		int32 CubemapSize = 0;
		int32 FullHDRBytes = 0;
		int32 EncodedHDRBytes = 0;
		SIZE_T AllocatedBytes = 0;
		FString PayloadState;
	};

	bool TryInventoryE6ReflectionCapture(
		UWorld* World,
		FCGReflectionCaptureInventory& OutInventory,
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		OutInventory = FCGReflectionCaptureInventory();
		TArray<UReflectionCaptureComponent*> WorldComponents;

		for (TObjectIterator<UReflectionCaptureComponent> It; It; ++It)
		{
			UReflectionCaptureComponent* Candidate = *It;
			AActor* CandidateOwner = IsValid(Candidate) ? Candidate->GetOwner() : nullptr;
			if (CandidateOwner && World->ContainsActor(CandidateOwner))
			{
				WorldComponents.Add(Candidate);
			}
		}

		OutInventory.ComponentCount = WorldComponents.Num();
		if (WorldComponents.Num() != 1)
		{
			OutErrorCode = TEXT("CAPTURE_COUNT_MISMATCH");
			OutErrorMessage = TEXT("E6 임시 맵에는 Reflection Capture 컴포넌트가 정확히 하나 있어야 합니다.");
			return false;
		}

		UReflectionCaptureComponent* CaptureComponent = WorldComponents[0];
		AActor* CaptureOwner = CaptureComponent->GetOwner();
		if (!CaptureOwner || CaptureOwner->GetClass() != ASphereReflectionCapture::StaticClass())
		{
			OutErrorCode = TEXT("CAPTURE_ACTOR_CLASS_MISMATCH");
			OutErrorMessage = TEXT("E6 Reflection Capture owner는 정확히 SphereReflectionCapture여야 합니다.");
			return false;
		}
		if (CaptureComponent->GetClass() != USphereReflectionCaptureComponent::StaticClass())
		{
			OutErrorCode = TEXT("CAPTURE_COMPONENT_CLASS_MISMATCH");
			OutErrorMessage = TEXT("E6 Reflection Capture 컴포넌트 타입이 계약과 다릅니다.");
			return false;
		}

		ASphereReflectionCapture* SphereActor = CastChecked<ASphereReflectionCapture>(CaptureOwner);
		USphereReflectionCaptureComponent* SphereComponent =
			CastChecked<USphereReflectionCaptureComponent>(CaptureComponent);
		if (SphereActor->GetCaptureComponent() != SphereComponent)
		{
			OutErrorCode = TEXT("CAPTURE_COMPONENT_OWNERSHIP_MISMATCH");
			OutErrorMessage = TEXT("E6 Sphere actor의 기본 Reflection Capture 컴포넌트가 아닙니다.");
			return false;
		}

		const FString ActorLabel = SphereActor->GetActorLabel(false);
		if (ActorLabel != GetE6ReflectionActorLabel())
		{
			OutErrorCode = TEXT("CAPTURE_LABEL_MISMATCH");
			OutErrorMessage = TEXT("E6 Sphere Reflection Capture 라벨이 계약과 다릅니다.");
			return false;
		}

		OutInventory.Actor = SphereActor;
		OutInventory.Component = SphereComponent;
		OutInventory.ActorLabel = ActorLabel;
		return true;
	}

	bool TryReadE6ReflectionBuildData(
		USphereReflectionCaptureComponent* Component,
		ECGReflectionPayloadRequirement Requirement,
		FCGReflectionCaptureBuildDataSnapshot& OutSnapshot,
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		OutSnapshot = FCGReflectionCaptureBuildDataSnapshot();
		OutSnapshot.bComponentRegistered = Component->IsRegistered();
		if (!OutSnapshot.bComponentRegistered)
		{
			OutErrorCode = TEXT("CAPTURE_NOT_REGISTERED");
			OutErrorMessage = TEXT("E6 Reflection Capture 컴포넌트가 현재 World에 등록되어 있지 않습니다.");
			return false;
		}
		if (!Component->MapBuildDataId.IsValid())
		{
			OutErrorCode = TEXT("INVALID_BUILD_DATA_ID");
			OutErrorMessage = TEXT("E6 Reflection Capture의 MapBuildDataId가 비어 있습니다.");
			return false;
		}

		FReflectionCaptureMapBuildData* MapBuildData = Component->GetMapBuildData();
		if (!MapBuildData)
		{
			OutErrorCode = TEXT("NO_BUILD_DATA");
			OutErrorMessage = TEXT("E6 Reflection Capture MapBuildData를 찾을 수 없습니다.");
			return false;
		}

		OutSnapshot.MapBuildDataId = Component->MapBuildDataId;
		OutSnapshot.CubemapSize = MapBuildData->CubemapSize;
		OutSnapshot.FullHDRBytes = MapBuildData->FullHDRCapturedData.Num();
		OutSnapshot.EncodedHDRBytes = MapBuildData->EncodedHDRCapturedData.Num();
		OutSnapshot.AllocatedBytes = MapBuildData->GetAllocatedSize();
		OutSnapshot.bSerializedPayloadPresent = OutSnapshot.CubemapSize > 0
			&& OutSnapshot.FullHDRBytes > 0
			&& OutSnapshot.EncodedHDRBytes > 0;
		OutSnapshot.bUploadedFinal = MapBuildData->HasBeenUploadedFinal();
		OutSnapshot.bEncodedResourcePresent = Component->EncodedHDRCubemapTexture != nullptr;

		if (OutSnapshot.AllocatedBytes == 0)
		{
			OutErrorCode = TEXT("EMPTY_BUILD_DATA_ALLOCATION");
			OutErrorMessage = TEXT("Reflection Capture BuiltData의 직렬화 할당 크기가 비어 있습니다.");
			return false;
		}

		if (Requirement == ECGReflectionPayloadRequirement::RequireSerialized)
		{
			if (!OutSnapshot.bSerializedPayloadPresent)
			{
				OutErrorCode = TEXT("INCOMPLETE_SERIALIZED_BUILD_DATA");
				OutErrorMessage = TEXT("빌드 직후 Reflection Capture의 Cubemap·Full HDR·Encoded HDR payload가 모두 필요합니다.");
				return false;
			}
			OutSnapshot.PayloadState = TEXT("Serialized");
			return true;
		}

		if (OutSnapshot.bSerializedPayloadPresent)
		{
			OutSnapshot.PayloadState = TEXT("Serialized");
			return true;
		}
		if (OutSnapshot.bEncodedResourcePresent)
		{
			OutSnapshot.PayloadState = TEXT("EncodedResource");
			return true;
		}
		if (OutSnapshot.bUploadedFinal)
		{
			OutSnapshot.PayloadState = TEXT("UploadedFinal");
			return true;
		}

		OutErrorCode = TEXT("INCOMPLETE_RUNTIME_BUILD_DATA");
		OutErrorMessage = TEXT("직렬화 payload, Encoded GPU resource, 최종 upload 증거가 모두 없습니다.");
		return false;
	}

	bool TryGetE6ReflectionCommandLineMode(
		FString& OutMode,
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		OutMode.Reset();
		TArray<FString> Tokens;
		TArray<FString> Switches;
		FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);

		auto TryReadUniqueSwitchValue = [&Switches](const FString& Prefix, FString& OutValue) -> bool
		{
			int32 MatchCount = 0;
			for (const FString& Switch : Switches)
			{
				if (Switch.StartsWith(Prefix, ESearchCase::IgnoreCase))
				{
					++MatchCount;
					OutValue = Switch.Mid(Prefix.Len());
				}
			}
			return MatchCount == 1 && !OutValue.IsEmpty();
		};

		FString Mode;
		if (!TryReadUniqueSwitchValue(TEXT("CGRE6ReflectionMode="), Mode))
		{
			OutErrorCode = TEXT("INVALID_E6_MODE_SWITCH");
			OutErrorMessage = TEXT("E6 manager 예약에는 CGRE6ReflectionMode switch가 정확히 하나 필요합니다.");
			return false;
		}

		FString MarkerToken;
		if (!TryReadUniqueSwitchValue(TEXT("CGRE6ReflectionMarkerBase64="), MarkerToken))
		{
			OutErrorCode = TEXT("INVALID_E6_MARKER_SWITCH");
			OutErrorMessage = TEXT("E6 manager 예약에는 marker switch가 정확히 하나 필요합니다.");
			return false;
		}

		Mode.ToLowerInline();
		if (Mode != TEXT("clone")
			&& Mode != TEXT("prepare")
			&& Mode != TEXT("build")
			&& Mode != TEXT("verify")
			&& Mode != TEXT("cleanup"))
		{
			OutErrorCode = TEXT("E6_MODE_NOT_ALLOWED");
			OutErrorMessage = TEXT("허용되지 않은 E6 Reflection Capture manager mode입니다.");
			return false;
		}

		OutMode = MoveTemp(Mode);
		return true;
	}

	bool TryValidateE6DataValidationSuppressed(
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		UClass* DataValidationSettingsClass = FindObject<UClass>(
			nullptr,
			TEXT("/Script/DataValidation.DataValidationSettings"));
		if (!DataValidationSettingsClass)
		{
			OutErrorCode = TEXT("DATA_VALIDATION_SETTINGS_CLASS_MISSING");
			OutErrorMessage = TEXT("로드된 DataValidationSettings 클래스를 reflection으로 찾을 수 없습니다.");
			return false;
		}

		UObject* DataValidationSettingsCDO = DataValidationSettingsClass->GetDefaultObject();
		if (!DataValidationSettingsCDO)
		{
			OutErrorCode = TEXT("DATA_VALIDATION_SETTINGS_CDO_MISSING");
			OutErrorMessage = TEXT("DataValidationSettings CDO가 없습니다.");
			return false;
		}

		const FBoolProperty* ValidateOnSaveProperty = FindFProperty<FBoolProperty>(
			DataValidationSettingsClass,
			TEXT("bValidateOnSave"));
		if (!ValidateOnSaveProperty)
		{
			OutErrorCode = TEXT("VALIDATE_ON_SAVE_PROPERTY_MISSING");
			OutErrorMessage = TEXT("DataValidationSettings.bValidateOnSave reflection property가 없습니다.");
			return false;
		}
		if (ValidateOnSaveProperty->GetPropertyValue_InContainer(DataValidationSettingsCDO))
		{
			OutErrorCode = TEXT("VALIDATION_NOT_SUPPRESSED");
			OutErrorMessage = TEXT("E6 manager 프로세스의 bValidateOnSave가 false가 아닙니다.");
			return false;
		}

		return true;
	}

	bool TryGetE6ManagerScriptPath(
		FString& OutScriptPath,
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		OutScriptPath.Reset();
		FString ProjectDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		FPaths::NormalizeDirectoryName(ProjectDirectory);

		FString ScriptPath = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(
				ProjectDirectory,
				TEXT("Tools/MainMapPreview/manage_mobile_static_reflection_capture.py")));
		FPaths::NormalizeFilename(ScriptPath);

		if (!FPaths::IsUnderDirectory(ScriptPath, ProjectDirectory)
			|| FPaths::GetCleanFilename(ScriptPath) != TEXT("manage_mobile_static_reflection_capture.py"))
		{
			OutErrorCode = TEXT("E6_MANAGER_PATH_MISMATCH");
			OutErrorMessage = TEXT("E6 manager script 경로가 프로젝트의 고정 경로와 다릅니다.");
			return false;
		}
		if (!FPaths::FileExists(ScriptPath))
		{
			OutErrorCode = TEXT("E6_MANAGER_SCRIPT_MISSING");
			OutErrorMessage = TEXT("고정 E6 manager script 파일을 찾을 수 없습니다.");
			return false;
		}

		OutScriptPath = MoveTemp(ScriptPath);
		return true;
	}

	bool TryGetE6ManagerEditorWorld(
		const FString& ExpectedMapPackage,
		UWorld*& OutEditorWorld,
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		OutEditorWorld = nullptr;
		if (!IsInGameThread())
		{
			OutErrorCode = TEXT("GAME_THREAD_REQUIRED");
			OutErrorMessage = TEXT("E6 manager end-frame 예약과 실행은 game thread에서만 허용됩니다.");
			return false;
		}
		if (!GIsEditor)
		{
			OutErrorCode = TEXT("EDITOR_RUNTIME_REQUIRED");
			OutErrorMessage = TEXT("E6 manager end-frame 실행은 일반 UnrealEditor에서만 허용됩니다.");
			return false;
		}
		if (IsRunningCommandlet())
		{
			OutErrorCode = TEXT("COMMANDLET_DISABLED");
			OutErrorMessage = TEXT("Commandlet에서는 E6 manager end-frame 실행을 예약할 수 없습니다.");
			return false;
		}
		if (!GEditor)
		{
			OutErrorCode = TEXT("NO_EDITOR");
			OutErrorMessage = TEXT("E6 manager를 실행할 에디터 인스턴스를 찾을 수 없습니다.");
			return false;
		}
		if (GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
		{
			OutErrorCode = TEXT("PLAY_SESSION_ACTIVE");
			OutErrorMessage = TEXT("PIE 또는 Simulate 중에는 E6 manager를 실행하지 않습니다.");
			return false;
		}

		UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
		if (!EditorWorld)
		{
			OutErrorCode = TEXT("NO_EDITOR_WORLD");
			OutErrorMessage = TEXT("E6 manager를 실행할 현재 에디터 World가 없습니다.");
			return false;
		}
		if (EditorWorld->WorldType != EWorldType::Editor || EditorWorld->IsGameWorld())
		{
			OutErrorCode = TEXT("INVALID_WORLD_TYPE");
			OutErrorMessage = TEXT("E6 manager는 일반 에디터 World에서만 실행할 수 있습니다.");
			return false;
		}
		if (EditorWorld->GetOutermost()->GetName() != ExpectedMapPackage)
		{
			OutErrorCode = TEXT("MAP_MISMATCH");
			OutErrorMessage = TEXT("E6 manager의 현재 에디터 맵이 해당 단계의 정확한 맵과 다릅니다.");
			return false;
		}

		OutEditorWorld = EditorWorld;
		return true;
	}

	bool TryValidateE6PrepareTransientCapture(
		UWorld* EditorWorld,
		FString& OutErrorCode,
		FString& OutErrorMessage,
		USphereReflectionCaptureComponent** OutValidatedComponent = nullptr)
	{
		if (OutValidatedComponent)
		{
			*OutValidatedComponent = nullptr;
		}

		FCGReflectionCaptureInventory Inventory;
		if (!TryInventoryE6ReflectionCapture(
			EditorWorld,
			Inventory,
			OutErrorCode,
			OutErrorMessage))
		{
			return false;
		}

		if (Inventory.Actor->GetLevel() != EditorWorld->PersistentLevel)
		{
			OutErrorCode = TEXT("CAPTURE_LEVEL_MISMATCH");
			OutErrorMessage = TEXT("prepare 단계의 E6 Reflection Capture는 Persistent Level에 있어야 합니다.");
			return false;
		}

		USphereReflectionCaptureComponent* Component = Inventory.Component;
		if (!Component->IsRegistered())
		{
			OutErrorCode = TEXT("CAPTURE_NOT_REGISTERED");
			OutErrorMessage = TEXT("prepare 단계의 E6 Reflection Capture 컴포넌트가 등록되지 않았습니다.");
			return false;
		}
		if (Component->Mobility != EComponentMobility::Static)
		{
			OutErrorCode = TEXT("CAPTURE_NOT_STATIC");
			OutErrorMessage = TEXT("prepare 단계의 E6 Reflection Capture는 Static이어야 합니다.");
			return false;
		}
		if (Component->ReflectionSourceType != EReflectionSourceType::SpecifiedCubemap || !Component->Cubemap)
		{
			OutErrorCode = TEXT("INVALID_CAPTURE_SOURCE");
			OutErrorMessage = TEXT("prepare 단계의 E6 Reflection Capture에는 Specified Cubemap이 필요합니다.");
			return false;
		}
		if (!Component->MapBuildDataId.IsValid())
		{
			OutErrorCode = TEXT("INVALID_BUILD_DATA_ID");
			OutErrorMessage = TEXT("prepare 단계의 E6 Reflection Capture MapBuildDataId가 비어 있습니다.");
			return false;
		}

		if (OutValidatedComponent)
		{
			*OutValidatedComponent = Component;
		}
		return true;
	}

	void RemoveScheduledE6ManagerHandles(FCGE6ManagerEndFrameState& State)
	{
		const FDelegateHandle BeginFrameHandle = State.BeginFrameHandle;
		const FDelegateHandle PostEditorTickHandle = State.PostEditorTickHandle;
		State.BeginFrameHandle.Reset();
		State.PostEditorTickHandle.Reset();

		if (BeginFrameHandle.IsValid())
		{
			FCoreDelegates::OnBeginFrame.Remove(BeginFrameHandle);
		}
		if (PostEditorTickHandle.IsValid() && GEngine)
		{
			GEngine->OnPostEditorTick().Remove(PostEditorTickHandle);
		}
	}

	void FailScheduledE6ManagerAtEndFrame(const FString& ErrorCode, const FString& ErrorMessage)
	{
		FCGE6ManagerEndFrameState& State = GetE6ManagerEndFrameState();
		State.Phase = ECGE6ManagerEndFramePhase::Terminal;
		if (IsInGameThread())
		{
			RemoveScheduledE6ManagerHandles(State);
		}

		const FString ExitReason = FString::Printf(
			TEXT("CGQABridge E6 end-frame failure [%s]: %s"),
			*ErrorCode,
			*ErrorMessage);
		UE_LOG(LogTemp, Error, TEXT("%s"), *ExitReason);
		RequestEngineExit(ExitReason);
	}

	bool TryValidateScheduledE6ManagerIdentity(
		const FCGE6ManagerEndFrameState& State,
		FString& OutScriptPath,
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		FString CurrentMode;
		if (!TryGetE6ReflectionCommandLineMode(CurrentMode, OutErrorCode, OutErrorMessage))
		{
			return false;
		}
		if (CurrentMode != State.ScheduledMode)
		{
			OutErrorCode = TEXT("SCHEDULED_MODE_MISMATCH");
			OutErrorMessage = TEXT("E6 manager mode가 예약 후 변경되었습니다.");
			return false;
		}

		if (!TryGetE6ManagerScriptPath(OutScriptPath, OutErrorCode, OutErrorMessage))
		{
			return false;
		}
		if (!FPaths::IsSamePath(OutScriptPath, State.ScheduledScriptPath))
		{
			OutErrorCode = TEXT("SCHEDULED_SCRIPT_MISMATCH");
			OutErrorMessage = TEXT("E6 manager script identity가 예약 후 변경되었습니다.");
			return false;
		}
		if (!TryValidateE6DataValidationSuppressed(OutErrorCode, OutErrorMessage))
		{
			return false;
		}

		return true;
	}

	void RunScheduledE6ManagerVerifyAfterEditorTick(float DeltaSeconds)
	{
		FCGE6ManagerEndFrameState& State = GetE6ManagerEndFrameState();
		const FDelegateHandle PostEditorTickHandle = State.PostEditorTickHandle;
		State.PostEditorTickHandle.Reset();
		if (PostEditorTickHandle.IsValid() && GEngine)
		{
			GEngine->OnPostEditorTick().Remove(PostEditorTickHandle);
		}

		(void)DeltaSeconds;
		if (!IsInGameThread())
		{
			FailScheduledE6ManagerAtEndFrame(
				TEXT("GAME_THREAD_REQUIRED"),
				TEXT("E6 prepare verification callback이 game thread 밖에서 호출되었습니다."));
			return;
		}

		if (!PostEditorTickHandle.IsValid()
			|| State.Phase != ECGE6ManagerEndFramePhase::PrepareVerificationPending
			|| State.ScheduledMode != TEXT("prepare")
			|| State.DispatchCount != 1)
		{
			FailScheduledE6ManagerAtEndFrame(
				TEXT("INVALID_PREPARE_VERIFICATION_STATE"),
				TEXT("E6 prepare PostEditorTick verification state가 완전하지 않습니다."));
			return;
		}

		UWorld* EditorWorld = nullptr;
		FString ValidationErrorCode;
		FString ValidationErrorMessage;
		if (!TryGetE6ManagerEditorWorld(
			GetE6ReflectionMapPackage(),
			EditorWorld,
			ValidationErrorCode,
			ValidationErrorMessage))
		{
			FailScheduledE6ManagerAtEndFrame(ValidationErrorCode, ValidationErrorMessage);
			return;
		}
		FString CurrentScriptPath;
		if (!TryValidateScheduledE6ManagerIdentity(
			State,
			CurrentScriptPath,
			ValidationErrorCode,
			ValidationErrorMessage))
		{
			FailScheduledE6ManagerAtEndFrame(ValidationErrorCode, ValidationErrorMessage);
			return;
		}
		USphereReflectionCaptureComponent* ValidatedCaptureComponent = nullptr;
		if (!TryValidateE6PrepareTransientCapture(
			EditorWorld,
			ValidationErrorCode,
			ValidationErrorMessage,
			&ValidatedCaptureComponent))
		{
			FailScheduledE6ManagerAtEndFrame(ValidationErrorCode, ValidationErrorMessage);
			return;
		}
		if (!State.PrepareMapBuildDataId.IsValid()
			|| State.PrepareMapBuildDataId != ValidatedCaptureComponent->MapBuildDataId)
		{
			FailScheduledE6ManagerAtEndFrame(
				TEXT("PREPARE_BUILD_DATA_ID_DRIFT"),
				TEXT("E6 prepare-save 직전 Reflection Capture MapBuildDataId가 prepare 재진입 기준값과 다릅니다."));
			return;
		}
		if (GFrameCounter != State.SpawnFrame)
		{
			FailScheduledE6ManagerAtEndFrame(
				TEXT("PREPARE_VERIFICATION_FRAME_MISMATCH"),
				TEXT("E6 prepare verification이 spawn과 같은 editor frame에서 실행되지 않았습니다."));
			return;
		}
		if (EditorWorld->HasEndOfFrameUpdates())
		{
			FailScheduledE6ManagerAtEndFrame(
				TEXT("PREPARE_END_OF_FRAME_UPDATES_PENDING"),
				TEXT("editor tick 뒤에도 E6 transient World에 EndOfFrame update가 남아 있습니다."));
			return;
		}

		State.VerificationFrame = GFrameCounter;
		if (State.ReadyFrame >= State.SpawnFrame
			|| State.VerificationFrame != State.SpawnFrame)
		{
			FailScheduledE6ManagerAtEndFrame(
				TEXT("PREPARE_FRAME_BARRIER_MISMATCH"),
				TEXT("E6 prepare-save frame이 ready < spawn == verification 계약과 다릅니다."));
			return;
		}

		++State.DispatchCount;
		if (State.DispatchCount != 2)
		{
			FailScheduledE6ManagerAtEndFrame(
				TEXT("PREPARE_DISPATCH_COUNT_MISMATCH"),
				TEXT("E6 prepare-save는 정확히 두 번째 manager dispatch여야 합니다."));
			return;
		}
		State.Phase = ECGE6ManagerEndFramePhase::PrepareSaveDispatching;

		const FString PythonCommand = FString::Printf(TEXT("PY \"%s\""), *CurrentScriptPath);
		const bool bCommandRouted = GEditor->Exec(EditorWorld, *PythonCommand);
		if (!bCommandRouted)
		{
			FailScheduledE6ManagerAtEndFrame(
				TEXT("PYTHON_COMMAND_NOT_ROUTED"),
				TEXT("E6 prepare-save Python command가 editor exec 경로에서 처리되지 않았습니다."));
			return;
		}

		State.Phase = ECGE6ManagerEndFramePhase::Terminal;
	}

	void RunScheduledE6ManagerPrepareSpawnAtBeginFrame()
	{
		FCGE6ManagerEndFrameState& State = GetE6ManagerEndFrameState();
		const FDelegateHandle BeginFrameHandle = State.BeginFrameHandle;
		State.BeginFrameHandle.Reset();
		if (BeginFrameHandle.IsValid())
		{
			FCoreDelegates::OnBeginFrame.Remove(BeginFrameHandle);
		}

		if (!IsInGameThread())
		{
			FailScheduledE6ManagerAtEndFrame(
				TEXT("GAME_THREAD_REQUIRED"),
				TEXT("E6 prepare BeginFrame callback이 game thread 밖에서 호출되었습니다."));
			return;
		}

		if (!BeginFrameHandle.IsValid()
			|| !State.bDispatchConsumed
			|| State.Phase != ECGE6ManagerEndFramePhase::BeginFramePending
			|| State.ScheduledMode != TEXT("prepare")
			|| State.StableProbeSuccesses != 2
			|| State.DispatchCount != 0)
		{
			FailScheduledE6ManagerAtEndFrame(
				TEXT("INVALID_PREPARE_BEGIN_FRAME_STATE"),
				TEXT("E6 prepare BeginFrame state가 완전하지 않습니다."));
			return;
		}

		UWorld* EditorWorld = nullptr;
		FString ValidationErrorCode;
		FString ValidationErrorMessage;
		if (!TryGetE6ManagerEditorWorld(
			GetE6ReflectionMapPackage(),
			EditorWorld,
			ValidationErrorCode,
			ValidationErrorMessage))
		{
			FailScheduledE6ManagerAtEndFrame(ValidationErrorCode, ValidationErrorMessage);
			return;
		}
		FString CurrentScriptPath;
		if (!TryValidateScheduledE6ManagerIdentity(
			State,
			CurrentScriptPath,
			ValidationErrorCode,
			ValidationErrorMessage))
		{
			FailScheduledE6ManagerAtEndFrame(ValidationErrorCode, ValidationErrorMessage);
			return;
		}
		if (GFrameCounter <= State.ReadyFrame)
		{
			FailScheduledE6ManagerAtEndFrame(
				TEXT("PREPARE_BEGIN_FRAME_DID_NOT_ADVANCE"),
				TEXT("E6 prepare BeginFrame이 최종 readiness frame보다 전진하지 않았습니다."));
			return;
		}

		bool bExpectedStreamingLevelLoaded = false;
		bool bStreamingStatePending = false;
		int32 NullStreamingLevelCount = 0;
		int32 LoadedStreamingLevelCount = 0;
		int32 PendingStreamingLevelCount = 0;
		int32 ExpectedStreamingLevelCount = 0;
		TArray<FString> ExpectedStreamingLevelStates;
		TArray<FString> PendingStreamingLevelStates;
		const TArray<ULevelStreaming*>& StreamingLevels = EditorWorld->GetStreamingLevels();
		for (ULevelStreaming* StreamingLevel : StreamingLevels)
		{
			if (!StreamingLevel)
			{
				bStreamingStatePending = true;
				++NullStreamingLevelCount;
				continue;
			}
			const bool bLevelLoaded = StreamingLevel->GetLoadedLevel() != nullptr;
			const bool bLevelPending = StreamingLevel->IsStreamingStatePending();
			LoadedStreamingLevelCount += bLevelLoaded ? 1 : 0;
			PendingStreamingLevelCount += bLevelPending ? 1 : 0;
			bStreamingStatePending |= bLevelPending;

			const FString StreamingLevelState = FString::Printf(
				TEXT("{asset=%s, packageToLoad=%s, class=%s, state=%s, loaded=%s, visible=%s, shouldLoad=%s, shouldBeVisible=%s, pending=%s}"),
				*StreamingLevel->GetWorldAssetPackageFName().ToString(),
				*StreamingLevel->PackageNameToLoad.ToString(),
				*StreamingLevel->GetClass()->GetName(),
				EnumToString(StreamingLevel->GetLevelStreamingState()),
				bLevelLoaded ? TEXT("true") : TEXT("false"),
				StreamingLevel->IsLevelVisible() ? TEXT("true") : TEXT("false"),
				StreamingLevel->ShouldBeLoaded() ? TEXT("true") : TEXT("false"),
				StreamingLevel->ShouldBeVisible() ? TEXT("true") : TEXT("false"),
				bLevelPending ? TEXT("true") : TEXT("false"));
			if (bLevelPending)
			{
				PendingStreamingLevelStates.Add(StreamingLevelState);
			}
			if (IsExpectedE6ManagerStreamingLevelInstance(StreamingLevel))
			{
				++ExpectedStreamingLevelCount;
				ExpectedStreamingLevelStates.Add(StreamingLevelState);
				bExpectedStreamingLevelLoaded = bLevelLoaded && !bLevelPending;
			}
		}
		const bool bAsyncLoading = IsAsyncLoading();
		const bool bHasEndOfFrameUpdates = EditorWorld->HasEndOfFrameUpdates();
		if (bAsyncLoading
			|| !bExpectedStreamingLevelLoaded
			|| bStreamingStatePending)
		{
			const FString ReadinessDiagnostics = FString::Printf(
				TEXT("E6 prepare BeginFrame 직전 editor World readiness가 변경되었습니다. "
					"[isAsyncLoading=%s, expectedStreamingLevelLoaded=%s, streamingStatePending=%s, hasEndOfFrameUpdates=%s, "
					"streamingLevelCount=%d, nullStreamingLevelCount=%d, loadedStreamingLevelCount=%d, "
					"pendingStreamingLevelCount=%d, expectedStreamingLevelCount=%d, expectedStreamingLevels=%s, pendingStreamingLevels=%s]"),
				bAsyncLoading ? TEXT("true") : TEXT("false"),
				bExpectedStreamingLevelLoaded ? TEXT("true") : TEXT("false"),
				bStreamingStatePending ? TEXT("true") : TEXT("false"),
				bHasEndOfFrameUpdates ? TEXT("true") : TEXT("false"),
				StreamingLevels.Num(),
				NullStreamingLevelCount,
				LoadedStreamingLevelCount,
				PendingStreamingLevelCount,
				ExpectedStreamingLevelCount,
				ExpectedStreamingLevelStates.IsEmpty()
					? TEXT("None")
					: *FString::Join(ExpectedStreamingLevelStates, TEXT("; ")),
				PendingStreamingLevelStates.IsEmpty()
					? TEXT("None")
					: *FString::Join(PendingStreamingLevelStates, TEXT("; ")));
			FailScheduledE6ManagerAtEndFrame(
				TEXT("PREPARE_BEGIN_FRAME_WORLD_NOT_READY"),
				ReadinessDiagnostics);
			return;
		}

		State.Phase = ECGE6ManagerEndFramePhase::PrepareSpawnDispatching;
		++State.DispatchCount;
		if (State.DispatchCount != 1)
		{
			FailScheduledE6ManagerAtEndFrame(
				TEXT("PREPARE_SPAWN_DISPATCH_COUNT_MISMATCH"),
				TEXT("E6 prepare spawn은 정확히 첫 번째 manager dispatch여야 합니다."));
			return;
		}

		const FString PythonCommand = FString::Printf(TEXT("PY \"%s\""), *CurrentScriptPath);
		const bool bCommandRouted = GEditor->Exec(EditorWorld, *PythonCommand);
		if (!bCommandRouted)
		{
			FailScheduledE6ManagerAtEndFrame(
				TEXT("PYTHON_COMMAND_NOT_ROUTED"),
				TEXT("E6 prepare spawn Python command가 editor exec 경로에서 처리되지 않았습니다."));
			return;
		}

		if (State.Phase != ECGE6ManagerEndFramePhase::PrepareVerificationPending
			|| !State.PostEditorTickHandle.IsValid()
			|| State.DispatchCount != 1
			|| State.SpawnFrame != GFrameCounter)
		{
			FailScheduledE6ManagerAtEndFrame(
				TEXT("PREPARE_VERIFICATION_NOT_SCHEDULED"),
				TEXT("E6 prepare spawn의 동기 재진입이 verification을 정확히 한 번 예약하지 않았습니다."));
			return;
		}
		USphereReflectionCaptureComponent* ValidatedCaptureComponent = nullptr;
		if (!TryValidateE6PrepareTransientCapture(
			EditorWorld,
			ValidationErrorCode,
			ValidationErrorMessage,
			&ValidatedCaptureComponent))
		{
			FailScheduledE6ManagerAtEndFrame(ValidationErrorCode, ValidationErrorMessage);
			return;
		}
		ValidatedCaptureComponent->UpdatePreviewShape();
	}

	void DispatchScheduledE6ManagerAfterReadiness(UWorld* EditorWorld, const FString& ScriptPath)
	{
		FCGE6ManagerEndFrameState& State = GetE6ManagerEndFrameState();
		if (State.Phase != ECGE6ManagerEndFramePhase::ReadinessPending
			|| State.ScheduledMode == TEXT("prepare")
			|| State.DispatchCount != 0)
		{
			FailScheduledE6ManagerAtEndFrame(
				TEXT("INVALID_PRIMARY_DISPATCH_STATE"),
				TEXT("E6 non-prepare manager dispatch state가 완전하지 않습니다."));
			return;
		}

		State.Phase = ECGE6ManagerEndFramePhase::PrimaryDispatching;
		++State.DispatchCount;
		if (State.DispatchCount != 1)
		{
			FailScheduledE6ManagerAtEndFrame(
				TEXT("PRIMARY_DISPATCH_COUNT_MISMATCH"),
				TEXT("E6 manager primary dispatch count가 계약과 다릅니다."));
			return;
		}

		const FString PythonCommand = FString::Printf(TEXT("PY \"%s\""), *ScriptPath);
		const bool bCommandRouted = GEditor->Exec(EditorWorld, *PythonCommand);
		if (!bCommandRouted)
		{
			FailScheduledE6ManagerAtEndFrame(
				TEXT("PYTHON_COMMAND_NOT_ROUTED"),
				TEXT("E6 manager Python command가 editor exec 경로에서 처리되지 않았습니다."));
			return;
		}

		State.Phase = ECGE6ManagerEndFramePhase::Terminal;
	}

	void RunScheduledE6ManagerReadinessProbe(float DeltaSeconds)
	{
		FCGE6ManagerEndFrameState& State = GetE6ManagerEndFrameState();
		const FDelegateHandle PostEditorTickHandle = State.PostEditorTickHandle;
		State.PostEditorTickHandle.Reset();
		if (PostEditorTickHandle.IsValid() && GEngine)
		{
			GEngine->OnPostEditorTick().Remove(PostEditorTickHandle);
		}

		(void)DeltaSeconds;
		if (!IsInGameThread())
		{
			FailScheduledE6ManagerAtEndFrame(
				TEXT("GAME_THREAD_REQUIRED"),
				TEXT("E6 readiness callback이 game thread 밖에서 호출되었습니다."));
			return;
		}

		if (!PostEditorTickHandle.IsValid()
			|| State.Phase != ECGE6ManagerEndFramePhase::ReadinessPending
			|| State.DispatchCount != 0)
		{
			FailScheduledE6ManagerAtEndFrame(
				TEXT("INVALID_READINESS_STATE"),
				TEXT("E6 manager readiness state가 완전하지 않습니다."));
			return;
		}

		UWorld* EditorWorld = nullptr;
		FString ValidationErrorCode;
		FString ValidationErrorMessage;
		if (!TryGetE6ManagerEditorWorld(
			State.ScheduledMapPackage,
			EditorWorld,
			ValidationErrorCode,
			ValidationErrorMessage))
		{
			FailScheduledE6ManagerAtEndFrame(ValidationErrorCode, ValidationErrorMessage);
			return;
		}

		FString CurrentScriptPath;
		if (!TryValidateScheduledE6ManagerIdentity(
			State,
			CurrentScriptPath,
			ValidationErrorCode,
			ValidationErrorMessage))
		{
			FailScheduledE6ManagerAtEndFrame(ValidationErrorCode, ValidationErrorMessage);
			return;
		}

		++State.StableProbeDispatches;
		if (FPlatformTime::Seconds() - State.StableProbeStartedAt > 30.0)
		{
			FailScheduledE6ManagerAtEndFrame(
				TEXT("READINESS_TIMEOUT"),
				TEXT("E6 manager readiness가 30초 안에 안정화되지 않았습니다."));
			return;
		}

		bool bExpectedStreamingLevelLoaded = false;
		bool bStreamingStatePending = false;
		for (ULevelStreaming* StreamingLevel : EditorWorld->GetStreamingLevels())
		{
			if (!StreamingLevel)
			{
				bStreamingStatePending = true;
				continue;
			}
			const bool bLevelPending = StreamingLevel->IsStreamingStatePending();
			bStreamingStatePending |= bLevelPending;
			if (IsExpectedE6ManagerStreamingLevelInstance(StreamingLevel))
			{
				bExpectedStreamingLevelLoaded = StreamingLevel->GetLoadedLevel() != nullptr
					&& !bLevelPending;
			}
		}

		const bool bWorldReady = !IsAsyncLoading()
			&& bExpectedStreamingLevelLoaded
			&& !bStreamingStatePending
			&& !EditorWorld->HasEndOfFrameUpdates();
		if (!bWorldReady)
		{
			State.StableProbeSuccesses = 0;
			if (State.StableProbeDispatches >= 120)
			{
				FailScheduledE6ManagerAtEndFrame(
					TEXT("READINESS_DISPATCH_LIMIT"),
					TEXT("E6 manager readiness가 120번의 probe 안에 안정화되지 않았습니다."));
				return;
			}
			const FDelegateHandle RearmedPostEditorTickHandle =
				GEngine->OnPostEditorTick().AddStatic(&RunScheduledE6ManagerReadinessProbe);
			if (!RearmedPostEditorTickHandle.IsValid())
			{
				FailScheduledE6ManagerAtEndFrame(
					TEXT("READINESS_REARM_FAILED"),
					TEXT("E6 manager readiness delegate 재등록에 실패했습니다."));
				return;
			}
			State.PostEditorTickHandle = RearmedPostEditorTickHandle;
			return;
		}

		++State.StableProbeSuccesses;
		if (State.StableProbeSuccesses < 2)
		{
			if (State.StableProbeDispatches >= 120)
			{
				FailScheduledE6ManagerAtEndFrame(
					TEXT("READINESS_DISPATCH_LIMIT"),
					TEXT("E6 manager readiness의 두 번째 연속 성공 전에 probe 한도에 도달했습니다."));
				return;
			}
			const FDelegateHandle RearmedPostEditorTickHandle =
				GEngine->OnPostEditorTick().AddStatic(&RunScheduledE6ManagerReadinessProbe);
			if (!RearmedPostEditorTickHandle.IsValid())
			{
				FailScheduledE6ManagerAtEndFrame(
					TEXT("READINESS_REARM_FAILED"),
					TEXT("E6 manager readiness delegate 재등록에 실패했습니다."));
				return;
			}
			State.PostEditorTickHandle = RearmedPostEditorTickHandle;
			return;
		}

		State.ReadyFrame = GFrameCounter;
		if (State.ScheduledMode == TEXT("prepare"))
		{
			const FDelegateHandle BeginFrameHandle =
				FCoreDelegates::OnBeginFrame.AddStatic(&RunScheduledE6ManagerPrepareSpawnAtBeginFrame);
			if (!BeginFrameHandle.IsValid())
			{
				FailScheduledE6ManagerAtEndFrame(
					TEXT("PREPARE_BEGIN_FRAME_BIND_FAILED"),
					TEXT("E6 prepare BeginFrame delegate 등록에 실패했습니다."));
				return;
			}
			State.BeginFrameHandle = BeginFrameHandle;
			State.Phase = ECGE6ManagerEndFramePhase::BeginFramePending;
			return;
		}

		DispatchScheduledE6ManagerAfterReadiness(EditorWorld, CurrentScriptPath);
	}

	void RunScheduledMainMapVisualProductionReadinessProbe(float DeltaSeconds);
	void RunScheduledMainMapVisualProductionPrepareAtBeginFrame();
	void RunScheduledMainMapVisualProductionVerifyAndSaveAfterEditorTick(float DeltaSeconds);

	bool TryGetMainMapVisualProductionCommandLineMode(
		FString& OutMode,
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		OutMode.Reset();
		TArray<FString> Tokens;
		TArray<FString> Switches;
		FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);

		int32 MatchCount = 0;
		FString Mode;
		for (const FString& Switch : Switches)
		{
			static const FString Prefix(TEXT("CGRProductionMode="));
			if (Switch.StartsWith(Prefix, ESearchCase::IgnoreCase))
			{
				++MatchCount;
				Mode = Switch.Mid(Prefix.Len());
			}
		}
		Mode.ToLowerInline();
		if (MatchCount != 1 || Mode.IsEmpty())
		{
			OutErrorCode = TEXT("INVALID_PRODUCTION_MODE_SWITCH");
			OutErrorMessage = TEXT("MainMap production 예약에는 CGRProductionMode switch가 정확히 하나 필요합니다.");
			return false;
		}
		if (Mode != TEXT("prepare")
			&& Mode != TEXT("build")
			&& Mode != TEXT("verify"))
		{
			OutErrorCode = TEXT("PRODUCTION_MODE_NOT_ALLOWED");
			OutErrorMessage = TEXT("MainMap production mode는 prepare/build/verify만 허용됩니다.");
			return false;
		}

		OutMode = MoveTemp(Mode);
		return true;
	}

	bool TryGetMainMapVisualProductionScriptPath(
		FString& OutScriptPath,
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		OutScriptPath.Reset();
		FString ProjectDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		FPaths::NormalizeDirectoryName(ProjectDirectory);
		FString ScriptPath = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(ProjectDirectory, GetMainMapVisualProductionScriptRelativePath()));
		FPaths::NormalizeFilename(ScriptPath);

		if (!FPaths::IsUnderDirectory(ScriptPath, ProjectDirectory)
			|| FPaths::GetCleanFilename(ScriptPath) != TEXT("apply_mainmap_visual_production.py"))
		{
			OutErrorCode = TEXT("PRODUCTION_SCRIPT_PATH_MISMATCH");
			OutErrorMessage = TEXT("MainMap production script가 프로젝트의 고정 경로와 다릅니다.");
			return false;
		}
		if (!FPaths::FileExists(ScriptPath))
		{
			OutErrorCode = TEXT("PRODUCTION_SCRIPT_MISSING");
			OutErrorMessage = TEXT("고정 MainMap production script 파일을 찾을 수 없습니다.");
			return false;
		}

		OutScriptPath = MoveTemp(ScriptPath);
		return true;
	}

	bool TryGetMainMapVisualProductionEditorWorld(
		UWorld*& OutEditorWorld,
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		OutEditorWorld = nullptr;
		if (!IsInGameThread())
		{
			OutErrorCode = TEXT("GAME_THREAD_REQUIRED");
			OutErrorMessage = TEXT("MainMap production 예약과 실행은 game thread에서만 허용됩니다.");
			return false;
		}
		if (!GIsEditor || IsRunningCommandlet())
		{
			OutErrorCode = TEXT("NORMAL_EDITOR_REQUIRED");
			OutErrorMessage = TEXT("MainMap production은 일반 UnrealEditor에서만 실행할 수 있습니다.");
			return false;
		}
		if (!GEditor)
		{
			OutErrorCode = TEXT("NO_EDITOR");
			OutErrorMessage = TEXT("MainMap production을 실행할 에디터 인스턴스가 없습니다.");
			return false;
		}
		if (GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
		{
			OutErrorCode = TEXT("PLAY_SESSION_ACTIVE");
			OutErrorMessage = TEXT("PIE 또는 Simulate 중에는 MainMap production을 실행하지 않습니다.");
			return false;
		}

		UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
		if (!EditorWorld)
		{
			OutErrorCode = TEXT("NO_EDITOR_WORLD");
			OutErrorMessage = TEXT("MainMap production editor World가 없습니다.");
			return false;
		}
		if (EditorWorld->WorldType != EWorldType::Editor || EditorWorld->IsGameWorld())
		{
			OutErrorCode = TEXT("INVALID_WORLD_TYPE");
			OutErrorMessage = TEXT("MainMap production은 일반 editor World에서만 실행할 수 있습니다.");
			return false;
		}
		if (EditorWorld->GetOutermost()->GetName() != GetMainMapVisualProductionMapPackage())
		{
			OutErrorCode = TEXT("PRODUCTION_MAP_MISMATCH");
			OutErrorMessage = TEXT("현재 에디터 맵이 고정 MainMap production 맵과 다릅니다.");
			return false;
		}
		if (!CGQABridgePolicy::IsMainMapVisualProductionFeatureLevelSupported(
			EditorWorld->GetFeatureLevel()))
		{
			OutErrorCode = TEXT("FEATURE_LEVEL_TOO_LOW");
			OutErrorMessage = TEXT("MainMap production은 SM5 이상 에디터 월드가 필요합니다.");
			return false;
		}

		OutEditorWorld = EditorWorld;
		return true;
	}

	bool IsMainMapVisualProductionWorldReady(
		UWorld* EditorWorld,
		const bool bRequireEndOfFrameDrain)
	{
		if (!EditorWorld)
		{
			return false;
		}

		bool bAnyStreamingStatePending = false;
		bool bExpectedStreamingLevelLoaded = false;
		int32 ExpectedStreamingLevelCount = 0;
		for (ULevelStreaming* StreamingLevel : EditorWorld->GetStreamingLevels())
		{
			if (!StreamingLevel)
			{
				bAnyStreamingStatePending = true;
				continue;
			}
			const bool bLevelPending = StreamingLevel->IsStreamingStatePending();
			bAnyStreamingStatePending |= bLevelPending;
			if (IsExpectedE6ManagerStreamingLevelInstance(StreamingLevel))
			{
				++ExpectedStreamingLevelCount;
				bExpectedStreamingLevelLoaded = StreamingLevel->GetLoadedLevel() != nullptr
					&& !bLevelPending;
			}
		}

		return CGQABridgePolicy::IsMainMapVisualProductionWorldReadyForPhase(
			IsAsyncLoading(),
			ExpectedStreamingLevelCount == 1 && bExpectedStreamingLevelLoaded,
			bAnyStreamingStatePending,
			EditorWorld->HasEndOfFrameUpdates(),
			bRequireEndOfFrameDrain);
	}

	bool TryValidateMainMapVisualProductionCapture(
		UWorld* EditorWorld,
		FString& OutErrorCode,
		FString& OutErrorMessage,
		USphereReflectionCaptureComponent** OutValidatedComponent = nullptr)
	{
		if (OutValidatedComponent)
		{
			*OutValidatedComponent = nullptr;
		}

		FCGReflectionCaptureInventory Inventory;
		if (!TryInventoryE6ReflectionCapture(
			EditorWorld,
			Inventory,
			OutErrorCode,
			OutErrorMessage))
		{
			return false;
		}
		USphereReflectionCaptureComponent* Component = Inventory.Component;
		if (Inventory.Actor->GetLevel() != EditorWorld->PersistentLevel)
		{
			OutErrorCode = TEXT("PRODUCTION_CAPTURE_LEVEL_MISMATCH");
			OutErrorMessage = TEXT("MainMap production capture는 Persistent Level에 있어야 합니다.");
			return false;
		}
		if (!Component->IsRegistered()
			|| Component->Mobility != EComponentMobility::Static
			|| Component->ReflectionSourceType != EReflectionSourceType::SpecifiedCubemap
			|| !Component->Cubemap
			|| !Component->MapBuildDataId.IsValid())
		{
			OutErrorCode = TEXT("PRODUCTION_CAPTURE_STATE_MISMATCH");
			OutErrorMessage = TEXT("MainMap production capture의 등록·Static·cubemap·GUID 계약이 다릅니다.");
			return false;
		}

		static const FVector ExpectedLocation(20086.6483, 281.4998, 31659.5026);
		static constexpr float ExpectedBrightness = 0.85f;
		static constexpr float ExpectedInfluenceRadius = 89271.5547f;
		static const FString ExpectedCubemapPath(
			TEXT("/Game/CompanyGrowth/Resources/Materials/MainMap/TC_StudioReflect.TC_StudioReflect"));
		if (!Inventory.Actor->GetActorLocation().Equals(ExpectedLocation, 0.01)
			|| !FMath::IsNearlyEqual(Component->Brightness, ExpectedBrightness, 1.0e-5f)
			|| !FMath::IsNearlyEqual(Component->InfluenceRadius, ExpectedInfluenceRadius, 0.01f)
			|| !FMath::IsNearlyZero(Component->SourceCubemapAngle, 1.0e-6f)
			|| Component->Cubemap->GetPathName() != ExpectedCubemapPath
			|| !Component->IsVisible())
		{
			OutErrorCode = TEXT("PRODUCTION_CAPTURE_VALUE_MISMATCH");
			OutErrorMessage = TEXT("MainMap production capture의 위치·밝기·반경·cubemap 값이 다릅니다.");
			return false;
		}

		if (OutValidatedComponent)
		{
			*OutValidatedComponent = Component;
		}
		return true;
	}

	void RemoveScheduledMainMapVisualProductionHandles(FCGMainMapVisualProductionState& State)
	{
		const FDelegateHandle BeginFrameHandle = State.BeginFrameHandle;
		const FDelegateHandle PostEditorTickHandle = State.PostEditorTickHandle;
		State.BeginFrameHandle.Reset();
		State.PostEditorTickHandle.Reset();
		if (BeginFrameHandle.IsValid())
		{
			FCoreDelegates::OnBeginFrame.Remove(BeginFrameHandle);
		}
		if (PostEditorTickHandle.IsValid() && GEngine)
		{
			GEngine->OnPostEditorTick().Remove(PostEditorTickHandle);
		}
	}

	void FailScheduledMainMapVisualProduction(
		const FString& ErrorCode,
		const FString& ErrorMessage)
	{
		FCGMainMapVisualProductionState& State = GetMainMapVisualProductionState();
		State.Phase = ECGMainMapVisualProductionPhase::Terminal;
		if (IsInGameThread())
		{
			RemoveScheduledMainMapVisualProductionHandles(State);
		}

		const FString ExitReason = FString::Printf(
			TEXT("CGQABridge MainMap production failure [%s]: %s"),
			*ErrorCode,
			*ErrorMessage);
		UE_LOG(LogTemp, Error, TEXT("%s"), *ExitReason);
		RequestEngineExit(ExitReason);
	}

	bool TryValidateScheduledMainMapVisualProductionIdentity(
		const FCGMainMapVisualProductionState& State,
		FString& OutScriptPath,
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		FString CurrentMode;
		if (!TryGetMainMapVisualProductionCommandLineMode(
			CurrentMode,
			OutErrorCode,
			OutErrorMessage))
		{
			return false;
		}
		if (CurrentMode != State.ScheduledMode)
		{
			OutErrorCode = TEXT("SCHEDULED_PRODUCTION_MODE_MISMATCH");
			OutErrorMessage = TEXT("MainMap production mode가 예약 후 변경되었습니다.");
			return false;
		}
		if (!TryGetMainMapVisualProductionScriptPath(
			OutScriptPath,
			OutErrorCode,
			OutErrorMessage))
		{
			return false;
		}
		if (!FPaths::IsSamePath(OutScriptPath, State.ScheduledScriptPath))
		{
			OutErrorCode = TEXT("SCHEDULED_PRODUCTION_SCRIPT_MISMATCH");
			OutErrorMessage = TEXT("MainMap production script identity가 예약 후 변경되었습니다.");
			return false;
		}
		return TryValidateE6DataValidationSuppressed(OutErrorCode, OutErrorMessage);
	}

	void DispatchScheduledMainMapVisualProductionAfterReadiness(
		UWorld* EditorWorld,
		const FString& ScriptPath)
	{
		FCGMainMapVisualProductionState& State = GetMainMapVisualProductionState();
		if (State.Phase != ECGMainMapVisualProductionPhase::ReadinessPending
			|| State.ScheduledMode == TEXT("prepare")
			|| State.DispatchCount != 0)
		{
			FailScheduledMainMapVisualProduction(
				TEXT("INVALID_PRODUCTION_PRIMARY_STATE"),
				TEXT("MainMap production primary dispatch state가 완전하지 않습니다."));
			return;
		}

		State.Phase = ECGMainMapVisualProductionPhase::PrimaryDispatching;
		++State.DispatchCount;
		if (State.DispatchCount != 1)
		{
			FailScheduledMainMapVisualProduction(
				TEXT("PRODUCTION_PRIMARY_DISPATCH_COUNT_MISMATCH"),
				TEXT("MainMap production build/verify dispatch는 정확히 한 번이어야 합니다."));
			return;
		}
		const FString PythonCommand = FString::Printf(TEXT("PY \"%s\""), *ScriptPath);
		if (!GEditor->Exec(EditorWorld, *PythonCommand))
		{
			FailScheduledMainMapVisualProduction(
				TEXT("PRODUCTION_PYTHON_COMMAND_NOT_ROUTED"),
				TEXT("MainMap production Python command가 editor exec 경로에서 처리되지 않았습니다."));
			return;
		}
		State.Phase = ECGMainMapVisualProductionPhase::Terminal;
	}

	void RunScheduledMainMapVisualProductionVerifyAndSaveAfterEditorTick(float DeltaSeconds)
	{
		FCGMainMapVisualProductionState& State = GetMainMapVisualProductionState();
		const FDelegateHandle PostEditorTickHandle = State.PostEditorTickHandle;
		State.PostEditorTickHandle.Reset();
		if (PostEditorTickHandle.IsValid() && GEngine)
		{
			GEngine->OnPostEditorTick().Remove(PostEditorTickHandle);
		}
		(void)DeltaSeconds;

		if (!IsInGameThread()
			|| !PostEditorTickHandle.IsValid()
			|| State.Phase != ECGMainMapVisualProductionPhase::PrepareVerificationPending
			|| State.ScheduledMode != TEXT("prepare")
			|| State.DispatchCount != 1)
		{
			FailScheduledMainMapVisualProduction(
				TEXT("INVALID_PRODUCTION_PREPARE_VERIFICATION_STATE"),
				TEXT("MainMap production prepare PostEditorTick state가 완전하지 않습니다."));
			return;
		}

		UWorld* EditorWorld = nullptr;
		FString ErrorCode;
		FString ErrorMessage;
		FString ScriptPath;
		if (!TryGetMainMapVisualProductionEditorWorld(EditorWorld, ErrorCode, ErrorMessage)
			|| !TryValidateScheduledMainMapVisualProductionIdentity(
				State,
				ScriptPath,
				ErrorCode,
				ErrorMessage))
		{
			FailScheduledMainMapVisualProduction(ErrorCode, ErrorMessage);
			return;
		}
		USphereReflectionCaptureComponent* Component = nullptr;
		if (!TryValidateMainMapVisualProductionCapture(
			EditorWorld,
			ErrorCode,
			ErrorMessage,
			&Component))
		{
			FailScheduledMainMapVisualProduction(ErrorCode, ErrorMessage);
			return;
		}
		if (!State.PrepareMapBuildDataId.IsValid()
			|| State.PrepareMapBuildDataId != Component->MapBuildDataId)
		{
			FailScheduledMainMapVisualProduction(
				TEXT("PRODUCTION_PREPARE_GUID_DRIFT"),
				TEXT("MainMap production capture GUID가 prepare verification 전에 변경되었습니다."));
			return;
		}
		if (EditorWorld->HasEndOfFrameUpdates())
		{
			FailScheduledMainMapVisualProduction(
				TEXT("PRODUCTION_END_OF_FRAME_UPDATES_PENDING"),
				TEXT("정상 editor tick 뒤에도 MainMap EndOfFrame update가 남아 있습니다."));
			return;
		}

		State.VerificationFrame = GFrameCounter;
		if (State.ReadyFrame >= State.SpawnFrame
			|| State.VerificationFrame != State.SpawnFrame)
		{
			FailScheduledMainMapVisualProduction(
				TEXT("PRODUCTION_PREPARE_FRAME_CONTRACT_MISMATCH"),
				TEXT("MainMap prepare frame이 ready < spawn == verification 계약과 다릅니다."));
			return;
		}
		++State.DispatchCount;
		if (State.DispatchCount != 2)
		{
			FailScheduledMainMapVisualProduction(
				TEXT("PRODUCTION_PREPARE_DISPATCH_COUNT_MISMATCH"),
				TEXT("MainMap prepare save는 정확히 두 번째 Python dispatch여야 합니다."));
			return;
		}

		State.Phase = ECGMainMapVisualProductionPhase::PrepareSaveDispatching;
		const FString PythonCommand = FString::Printf(TEXT("PY \"%s\""), *ScriptPath);
		if (!GEditor->Exec(EditorWorld, *PythonCommand))
		{
			FailScheduledMainMapVisualProduction(
				TEXT("PRODUCTION_SAVE_COMMAND_NOT_ROUTED"),
				TEXT("MainMap production save stage가 editor exec 경로에서 처리되지 않았습니다."));
			return;
		}
		State.Phase = ECGMainMapVisualProductionPhase::Terminal;
	}

	void RunScheduledMainMapVisualProductionPrepareAtBeginFrame()
	{
		FCGMainMapVisualProductionState& State = GetMainMapVisualProductionState();
		const FDelegateHandle BeginFrameHandle = State.BeginFrameHandle;
		State.BeginFrameHandle.Reset();
		if (BeginFrameHandle.IsValid())
		{
			FCoreDelegates::OnBeginFrame.Remove(BeginFrameHandle);
		}

		if (!IsInGameThread()
			|| !BeginFrameHandle.IsValid()
			|| State.Phase != ECGMainMapVisualProductionPhase::BeginFramePending
			|| State.ScheduledMode != TEXT("prepare")
			|| State.StableProbeSuccesses != 2
			|| State.DispatchCount != 0)
		{
			FailScheduledMainMapVisualProduction(
				TEXT("INVALID_PRODUCTION_PREPARE_BEGIN_FRAME_STATE"),
				TEXT("MainMap production prepare BeginFrame state가 완전하지 않습니다."));
			return;
		}

		UWorld* EditorWorld = nullptr;
		FString ErrorCode;
		FString ErrorMessage;
		FString ScriptPath;
		if (!TryGetMainMapVisualProductionEditorWorld(EditorWorld, ErrorCode, ErrorMessage)
			|| !TryValidateScheduledMainMapVisualProductionIdentity(
				State,
				ScriptPath,
				ErrorCode,
				ErrorMessage))
		{
			FailScheduledMainMapVisualProduction(ErrorCode, ErrorMessage);
			return;
		}
		if (GFrameCounter <= State.ReadyFrame
			|| !IsMainMapVisualProductionWorldReady(
				EditorWorld,
				/*bRequireEndOfFrameDrain*/ false))
		{
			FailScheduledMainMapVisualProduction(
				TEXT("PRODUCTION_PREPARE_BEGIN_FRAME_NOT_READY"),
				TEXT("MainMap production BeginFrame이 readiness 계약을 유지하지 못했습니다."));
			return;
		}

		State.Phase = ECGMainMapVisualProductionPhase::PrepareMutationDispatching;
		++State.DispatchCount;
		if (State.DispatchCount != 1)
		{
			FailScheduledMainMapVisualProduction(
				TEXT("PRODUCTION_MUTATION_DISPATCH_COUNT_MISMATCH"),
				TEXT("MainMap production mutation은 정확히 첫 번째 Python dispatch여야 합니다."));
			return;
		}
		const FString PythonCommand = FString::Printf(TEXT("PY \"%s\""), *ScriptPath);
		if (!GEditor->Exec(EditorWorld, *PythonCommand))
		{
			FailScheduledMainMapVisualProduction(
				TEXT("PRODUCTION_MUTATION_COMMAND_NOT_ROUTED"),
				TEXT("MainMap production mutation stage가 editor exec 경로에서 처리되지 않았습니다."));
			return;
		}

		if (State.Phase != ECGMainMapVisualProductionPhase::PrepareVerificationPending
			|| !State.PostEditorTickHandle.IsValid()
			|| State.DispatchCount != 1
			|| State.SpawnFrame != GFrameCounter)
		{
			FailScheduledMainMapVisualProduction(
				TEXT("PRODUCTION_VERIFICATION_NOT_SCHEDULED"),
				TEXT("MainMap production mutation 재진입이 verification을 예약하지 않았습니다."));
			return;
		}
		USphereReflectionCaptureComponent* Component = nullptr;
		if (!TryValidateMainMapVisualProductionCapture(
			EditorWorld,
			ErrorCode,
			ErrorMessage,
			&Component)
			|| Component->MapBuildDataId != State.PrepareMapBuildDataId)
		{
			if (ErrorCode.IsEmpty())
			{
				ErrorCode = TEXT("PRODUCTION_PREPARE_GUID_MISMATCH");
				ErrorMessage = TEXT("MainMap production re-entry GUID가 C++ 검증값과 다릅니다.");
			}
			FailScheduledMainMapVisualProduction(ErrorCode, ErrorMessage);
			return;
		}
		Component->UpdatePreviewShape();
	}

	void RunScheduledMainMapVisualProductionReadinessProbe(float DeltaSeconds)
	{
		FCGMainMapVisualProductionState& State = GetMainMapVisualProductionState();
		const FDelegateHandle PostEditorTickHandle = State.PostEditorTickHandle;
		State.PostEditorTickHandle.Reset();
		if (PostEditorTickHandle.IsValid() && GEngine)
		{
			GEngine->OnPostEditorTick().Remove(PostEditorTickHandle);
		}
		(void)DeltaSeconds;

		if (!IsInGameThread()
			|| !PostEditorTickHandle.IsValid()
			|| State.Phase != ECGMainMapVisualProductionPhase::ReadinessPending
			|| State.DispatchCount != 0)
		{
			FailScheduledMainMapVisualProduction(
				TEXT("INVALID_PRODUCTION_READINESS_STATE"),
				TEXT("MainMap production readiness state가 완전하지 않습니다."));
			return;
		}

		UWorld* EditorWorld = nullptr;
		FString ErrorCode;
		FString ErrorMessage;
		FString ScriptPath;
		if (!TryGetMainMapVisualProductionEditorWorld(EditorWorld, ErrorCode, ErrorMessage)
			|| !TryValidateScheduledMainMapVisualProductionIdentity(
				State,
				ScriptPath,
				ErrorCode,
				ErrorMessage))
		{
			FailScheduledMainMapVisualProduction(ErrorCode, ErrorMessage);
			return;
		}

		++State.StableProbeDispatches;
		if (FPlatformTime::Seconds() - State.StableProbeStartedAt > 30.0)
		{
			FailScheduledMainMapVisualProduction(
				TEXT("PRODUCTION_READINESS_TIMEOUT"),
				TEXT("MainMap production readiness가 30초 안에 안정화되지 않았습니다."));
			return;
		}
		if (IsMainMapVisualProductionWorldReady(
			EditorWorld,
			/*bRequireEndOfFrameDrain*/ true))
		{
			++State.StableProbeSuccesses;
		}
		else
		{
			State.StableProbeSuccesses = 0;
		}

		if (State.StableProbeSuccesses < 2)
		{
			if (State.StableProbeDispatches >= 120)
			{
				FailScheduledMainMapVisualProduction(
					TEXT("PRODUCTION_READINESS_DISPATCH_LIMIT"),
					TEXT("MainMap production readiness가 120번 안에 안정화되지 않았습니다."));
				return;
			}
			const FDelegateHandle RearmedHandle =
				GEngine->OnPostEditorTick().AddStatic(
					&RunScheduledMainMapVisualProductionReadinessProbe);
			if (!RearmedHandle.IsValid())
			{
				FailScheduledMainMapVisualProduction(
					TEXT("PRODUCTION_READINESS_REARM_FAILED"),
					TEXT("MainMap production readiness delegate 재등록에 실패했습니다."));
				return;
			}
			State.PostEditorTickHandle = RearmedHandle;
			return;
		}

		State.ReadyFrame = GFrameCounter;
		if (State.ScheduledMode == TEXT("prepare"))
		{
			const FDelegateHandle BeginFrameHandle =
				FCoreDelegates::OnBeginFrame.AddStatic(
					&RunScheduledMainMapVisualProductionPrepareAtBeginFrame);
			if (!BeginFrameHandle.IsValid())
			{
				FailScheduledMainMapVisualProduction(
					TEXT("PRODUCTION_BEGIN_FRAME_BIND_FAILED"),
					TEXT("MainMap production prepare BeginFrame 등록에 실패했습니다."));
				return;
			}
			State.BeginFrameHandle = BeginFrameHandle;
			State.Phase = ECGMainMapVisualProductionPhase::BeginFramePending;
			return;
		}

		DispatchScheduledMainMapVisualProductionAfterReadiness(EditorWorld, ScriptPath);
	}

	void WriteE6ReflectionBuildData(
		const TSharedRef<FJsonObject>& Root,
		const FString& MapPackage,
		const FCGReflectionCaptureInventory& Inventory,
		const FCGReflectionCaptureBuildDataSnapshot& Snapshot)
	{
		Root->SetBoolField(TEXT("ok"), true);
		Root->SetStringField(TEXT("mapPackage"), MapPackage);
		Root->SetNumberField(TEXT("actorCount"), Inventory.ComponentCount);
		Root->SetStringField(TEXT("actorLabel"), Inventory.ActorLabel);
		Root->SetStringField(TEXT("mapBuildDataId"), Snapshot.MapBuildDataId.ToString(EGuidFormats::DigitsWithHyphens));
		Root->SetBoolField(TEXT("componentRegistered"), Snapshot.bComponentRegistered);
		Root->SetBoolField(TEXT("builtDataPresent"), true);
		Root->SetBoolField(TEXT("serializedPayloadPresent"), Snapshot.bSerializedPayloadPresent);
		Root->SetBoolField(TEXT("uploadedFinal"), Snapshot.bUploadedFinal);
		Root->SetBoolField(TEXT("encodedResourcePresent"), Snapshot.bEncodedResourcePresent);
		Root->SetStringField(TEXT("payloadState"), Snapshot.PayloadState);
		Root->SetNumberField(TEXT("cubemapSize"), Snapshot.CubemapSize);
		Root->SetNumberField(TEXT("fullHDRBytes"), Snapshot.FullHDRBytes);
		Root->SetNumberField(TEXT("encodedHDRBytes"), Snapshot.EncodedHDRBytes);
		Root->SetNumberField(TEXT("allocatedBytes"), static_cast<double>(Snapshot.AllocatedBytes));
	}
#endif

	/** 월드/PC 신원을 반환 JSON 에 싣는다 — 어느 세션에서 실행됐는지가 곧 증거다. */
	void WriteWorldIdentity(const TSharedRef<FJsonObject>& Root, UWorld* World, APlayerController* PC)
	{
		Root->SetStringField(TEXT("world"), World ? World->GetName() : FString());
		Root->SetStringField(TEXT("map"), World ? UWorld::RemovePIEPrefix(World->GetMapName()) : FString());
		Root->SetStringField(TEXT("pcClass"), PC ? PC->GetClass()->GetName() : FString());
	}
}

bool CGQABridgePolicy::IsReflectionBuildMapAllowed(const FString& MapPackageName)
{
	static const FString TransientE6MapPackage(
		TEXT("/Game/__CGRTransient/MobileVisualAB/E6/MainMap_TheRiverwalkCity_E6"));
	static const FString ProductionMainMapPackage(
		TEXT("/Game/CompanyGrowth/Level/MainMap_TheRiverwalkCity"));

	return MapPackageName == TransientE6MapPackage
		|| MapPackageName == ProductionMainMapPackage;
}

bool CGQABridgePolicy::IsMainMapVisualProductionFeatureLevelSupported(
	const ERHIFeatureLevel::Type FeatureLevel)
{
	return FeatureLevel >= ERHIFeatureLevel::SM5;
}

bool CGQABridgePolicy::IsMainMapVisualProductionWorldReadyForPhase(
	const bool bAsyncLoading,
	const bool bExpectedStreamingLevelLoaded,
	const bool bStreamingStatePending,
	const bool bHasEndOfFrameUpdates,
	const bool bRequireEndOfFrameDrain)
{
	return !bAsyncLoading
		&& bExpectedStreamingLevelLoaded
		&& !bStreamingStatePending
		&& (!bRequireEndOfFrameDrain || !bHasEndOfFrameUpdates);
}

FString UCGQABridge::ExecAndCapture(UObject* WorldContextObject, const FString& Command)
{
#if UE_BUILD_SHIPPING
	(void)WorldContextObject;
	(void)Command;
	return ErrorJson(TEXT("SHIPPING_DISABLED"), TEXT("Shipping 빌드에서는 QA 브리지가 비활성화됩니다."));
#else
	if (Command.TrimStartAndEnd().IsEmpty())
	{
		return ErrorJson(TEXT("EMPTY_COMMAND"), TEXT("실행할 명령이 비어 있습니다."));
	}

	UWorld* World = ResolveWorld(WorldContextObject);
	if (!World)
	{
		return ErrorJson(TEXT("NO_WORLD"), TEXT("게임 월드가 없습니다 — 에디터에서 Play(Alt+P) 후 다시 시도하세요."));
	}

	APlayerController* PC = World->GetFirstPlayerController();

	FCGQALogSink Sink;
	FString ConsoleOutput;

	GLog->AddOutputDevice(&Sink);
	if (PC)
	{
		// PlayerController 경유가 치트의 정규 경로다 — CheatManager 는 여기에 붙어 있다.
		ConsoleOutput = PC->ConsoleCommand(Command, /*bWriteToLog*/ true);
	}
	else
	{
		FStringOutputDevice Ar;
		GEngine->Exec(World, *Command, Ar);
		ConsoleOutput = Ar;
	}
	// Flush 를 Remove 앞에 둬야 버퍼에 남은 줄까지 싱크로 들어온다.
	GLog->Flush();
	GLog->RemoveOutputDevice(&Sink);

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("ok"), true);
	Root->SetStringField(TEXT("command"), Command);
	WriteWorldIdentity(Root, World, PC);
	Root->SetBoolField(TEXT("hadPlayerController"), PC != nullptr);
	Root->SetStringField(TEXT("console"), ConsoleOutput);
	Root->SetBoolField(TEXT("truncated"), Sink.bTruncated);

	AppendLines(Root, Sink.Lines);

	return ToJsonLine(Root);
#endif
}

FString UCGQABridge::DescribeContext(UObject* WorldContextObject)
{
#if UE_BUILD_SHIPPING
	(void)WorldContextObject;
	return ErrorJson(TEXT("SHIPPING_DISABLED"), TEXT("Shipping 빌드에서는 QA 브리지가 비활성화됩니다."));
#else
	UWorld* World = ResolveWorld(WorldContextObject);
	if (!World)
	{
		return ErrorJson(TEXT("NO_WORLD"), TEXT("게임 월드가 없습니다 — 에디터에서 Play(Alt+P) 후 다시 시도하세요."));
	}

	APlayerController* PC = World->GetFirstPlayerController();

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("ok"), true);
	WriteWorldIdentity(Root, World, PC);

	// 게이트가 "모름" 과 "불일치" 를 구분할 수 있어야 한다 — 매니저가 없으면 -1 이 아니라
	// 필드를 아예 비워 두는 편이 정직하지만, 이 둘은 애초에 게임인스턴스가 없으면 같이 없다.
	int32 SelectedEmployeeID = INDEX_NONE;
	int32 ManagedBuildingIndex = INDEX_NONE;
	if (UCGGameInstance* GI = UCGGameInstance::GetInstance())
	{
		ManagedBuildingIndex = GI->GetCurrentManagedBuildingIndex();
		if (UEmployeeManager* EmployeeMgr = GI->GetSubsystem<UEmployeeManager>())
		{
			SelectedEmployeeID = EmployeeMgr->GetSelectedEmployeeID();
		}
	}
	Root->SetNumberField(TEXT("selectedEmployeeID"), SelectedEmployeeID);
	Root->SetNumberField(TEXT("managedBuildingIndex"), ManagedBuildingIndex);

	Root->SetBoolField(
		TEXT("hasTimeCycleManager"),
		UGameplayStatics::GetActorOfClass(World, ATimeCycleManager::StaticClass()) != nullptr);

	TArray<AActor*> Factories;
	UGameplayStatics::GetAllActorsOfClass(World, ABrickFactory::StaticClass(), Factories);
	Root->SetNumberField(TEXT("brickFactoryCount"), Factories.Num());

	return ToJsonLine(Root);
#endif
}

FString UCGQABridge::ScheduleEditorEndFrameE6Manager()
{
#if !WITH_EDITOR
	return ErrorJson(TEXT("EDITOR_ONLY"), TEXT("E6 manager end-frame 예약은 에디터 빌드에서만 사용할 수 있습니다."));
#else
	if (!IsInGameThread())
	{
		const FString ErrorCode(TEXT("GAME_THREAD_REQUIRED"));
		const FString ErrorMessage(TEXT("E6 manager 예약은 game thread에서만 허용됩니다."));
		FailScheduledE6ManagerAtEndFrame(ErrorCode, ErrorMessage);
		return ErrorJson(ErrorCode, ErrorMessage);
	}

	FString ValidationErrorCode;
	FString ValidationErrorMessage;
	const bool bValidationSuppressed = TryValidateE6DataValidationSuppressed(
		ValidationErrorCode,
		ValidationErrorMessage);
	if (!bValidationSuppressed)
	{
		FailScheduledE6ManagerAtEndFrame(ValidationErrorCode, ValidationErrorMessage);
		return ErrorJson(ValidationErrorCode, ValidationErrorMessage);
	}

	FString Mode;
	if (!TryGetE6ReflectionCommandLineMode(Mode, ValidationErrorCode, ValidationErrorMessage))
	{
		FailScheduledE6ManagerAtEndFrame(ValidationErrorCode, ValidationErrorMessage);
		return ErrorJson(ValidationErrorCode, ValidationErrorMessage);
	}

	FString ScriptPath;
	if (!TryGetE6ManagerScriptPath(ScriptPath, ValidationErrorCode, ValidationErrorMessage))
	{
		FailScheduledE6ManagerAtEndFrame(ValidationErrorCode, ValidationErrorMessage);
		return ErrorJson(ValidationErrorCode, ValidationErrorMessage);
	}

	FCGE6ManagerEndFrameState& State = GetE6ManagerEndFrameState();
	const bool bPrepareSaveRequest = State.bDispatchConsumed
		&& State.Phase == ECGE6ManagerEndFramePhase::PrepareSpawnDispatching
		&& State.ScheduledMode == TEXT("prepare")
		&& State.DispatchCount == 1;
	if (State.bDispatchConsumed && !bPrepareSaveRequest)
	{
		const FString ErrorCode(TEXT("E6_MANAGER_ALREADY_SCHEDULED"));
		const FString ErrorMessage(TEXT("이 UnrealEditor 프로세스에서는 허용된 prepare-save 재진입 외 추가 예약을 거부합니다."));
		FailScheduledE6ManagerAtEndFrame(ErrorCode, ErrorMessage);
		return ErrorJson(ErrorCode, ErrorMessage);
	}

	const bool bSourceStartupMode = Mode == TEXT("clone") || Mode == TEXT("cleanup");
	const FString& ExpectedMapPackage = bSourceStartupMode
		? GetE6ManagerSourceMapPackage()
		: GetE6ReflectionMapPackage();
	UWorld* EditorWorld = nullptr;
	if (!TryGetE6ManagerEditorWorld(
		ExpectedMapPackage,
		EditorWorld,
		ValidationErrorCode,
		ValidationErrorMessage))
	{
		FailScheduledE6ManagerAtEndFrame(ValidationErrorCode, ValidationErrorMessage);
		return ErrorJson(ValidationErrorCode, ValidationErrorMessage);
	}

	if (bPrepareSaveRequest)
	{
		if (Mode != State.ScheduledMode || !FPaths::IsSamePath(ScriptPath, State.ScheduledScriptPath))
		{
			const FString ErrorCode(TEXT("PREPARE_REENTRY_IDENTITY_MISMATCH"));
			const FString ErrorMessage(TEXT("E6 prepare-save 재진입의 mode 또는 script identity가 다릅니다."));
			FailScheduledE6ManagerAtEndFrame(ErrorCode, ErrorMessage);
			return ErrorJson(ErrorCode, ErrorMessage);
		}
		if (State.BeginFrameHandle.IsValid()
			|| State.PostEditorTickHandle.IsValid())
		{
			const FString ErrorCode(TEXT("PREPARE_REENTRY_HANDLE_CONFLICT"));
			const FString ErrorMessage(TEXT("E6 prepare-save 재진입 전에 이전 delegate가 완전히 제거되지 않았습니다."));
			FailScheduledE6ManagerAtEndFrame(ErrorCode, ErrorMessage);
			return ErrorJson(ErrorCode, ErrorMessage);
		}
		USphereReflectionCaptureComponent* ValidatedCaptureComponent = nullptr;
		if (!TryValidateE6PrepareTransientCapture(
			EditorWorld,
			ValidationErrorCode,
			ValidationErrorMessage,
			&ValidatedCaptureComponent))
		{
			FailScheduledE6ManagerAtEndFrame(ValidationErrorCode, ValidationErrorMessage);
			return ErrorJson(ValidationErrorCode, ValidationErrorMessage);
		}
		State.PrepareMapBuildDataId = ValidatedCaptureComponent->MapBuildDataId;
		if (!GEngine)
		{
			const FString ErrorCode(TEXT("NO_ENGINE"));
			const FString ErrorMessage(TEXT("E6 prepare full-frame barrier를 등록할 Engine이 없습니다."));
			FailScheduledE6ManagerAtEndFrame(ErrorCode, ErrorMessage);
			return ErrorJson(ErrorCode, ErrorMessage);
		}

		if (GFrameCounter <= State.ReadyFrame || State.StableProbeSuccesses != 2)
		{
			const FString ErrorCode(TEXT("PREPARE_REENTRY_FRAME_MISMATCH"));
			const FString ErrorMessage(TEXT("E6 prepare 재진입 frame 또는 readiness 증거가 계약과 다릅니다."));
			FailScheduledE6ManagerAtEndFrame(ErrorCode, ErrorMessage);
			return ErrorJson(ErrorCode, ErrorMessage);
		}

		State.SpawnFrame = GFrameCounter;
		const FDelegateHandle PostEditorTickHandle =
			GEngine->OnPostEditorTick().AddStatic(&RunScheduledE6ManagerVerifyAfterEditorTick);
		if (!PostEditorTickHandle.IsValid())
		{
			const FString ErrorCode(TEXT("POST_EDITOR_TICK_BIND_FAILED"));
			const FString ErrorMessage(TEXT("E6 prepare verification delegate 등록에 실패했습니다."));
			FailScheduledE6ManagerAtEndFrame(ErrorCode, ErrorMessage);
			return ErrorJson(ErrorCode, ErrorMessage);
		}

		State.PostEditorTickHandle = PostEditorTickHandle;
		State.Phase = ECGE6ManagerEndFramePhase::PrepareVerificationPending;

		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetBoolField(TEXT("ok"), true);
		Root->SetBoolField(TEXT("scheduled"), true);
		Root->SetStringField(TEXT("operation"), TEXT("RunE6Manager"));
		Root->SetStringField(TEXT("mode"), Mode);
		Root->SetStringField(TEXT("mapPackage"), State.ScheduledMapPackage);
		Root->SetStringField(TEXT("phase"), TEXT("prepareSave"));
		Root->SetNumberField(TEXT("readyFrame"), static_cast<double>(State.ReadyFrame));
		Root->SetNumberField(TEXT("spawnFrame"), static_cast<double>(State.SpawnFrame));
		Root->SetNumberField(TEXT("stableProbeSuccesses"), State.StableProbeSuccesses);
		Root->SetStringField(
			TEXT("mapBuildDataId"),
			State.PrepareMapBuildDataId.ToString(EGuidFormats::DigitsWithHyphens));
		Root->SetBoolField(TEXT("validationSuppressed"), true);
		return ToJsonLine(Root);
	}

	if (State.Phase != ECGE6ManagerEndFramePhase::Idle
		|| State.BeginFrameHandle.IsValid()
		|| State.PostEditorTickHandle.IsValid()
		|| State.DispatchCount != 0)
	{
		const FString ErrorCode(TEXT("INVALID_INITIAL_STATE"));
		const FString ErrorMessage(TEXT("E6 manager primary 예약 state가 초기 상태가 아닙니다."));
		FailScheduledE6ManagerAtEndFrame(ErrorCode, ErrorMessage);
		return ErrorJson(ErrorCode, ErrorMessage);
	}
	if (!GEngine)
	{
		const FString ErrorCode(TEXT("NO_ENGINE"));
		const FString ErrorMessage(TEXT("E6 manager readiness delegate를 등록할 Engine이 없습니다."));
		FailScheduledE6ManagerAtEndFrame(ErrorCode, ErrorMessage);
		return ErrorJson(ErrorCode, ErrorMessage);
	}

	State.ScheduledMode = Mode;
	State.ScheduledMapPackage = EditorWorld->GetOutermost()->GetName();
	State.ScheduledScriptPath = ScriptPath;
	State.StableProbeStartedAt = FPlatformTime::Seconds();
	const FDelegateHandle PostEditorTickHandle =
		GEngine->OnPostEditorTick().AddStatic(&RunScheduledE6ManagerReadinessProbe);
	if (!PostEditorTickHandle.IsValid())
	{
		const FString ErrorCode(TEXT("READINESS_BIND_FAILED"));
		const FString ErrorMessage(TEXT("E6 manager readiness delegate 등록에 실패했습니다."));
		FailScheduledE6ManagerAtEndFrame(ErrorCode, ErrorMessage);
		return ErrorJson(ErrorCode, ErrorMessage);
	}

	State.PostEditorTickHandle = PostEditorTickHandle;
	State.bDispatchConsumed = true;
	State.Phase = ECGE6ManagerEndFramePhase::ReadinessPending;

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("ok"), true);
	Root->SetBoolField(TEXT("scheduled"), true);
	Root->SetStringField(TEXT("operation"), TEXT("RunE6Manager"));
	Root->SetStringField(TEXT("mode"), Mode);
	Root->SetStringField(TEXT("mapPackage"), State.ScheduledMapPackage);
	Root->SetStringField(TEXT("phase"), Mode == TEXT("prepare") ? TEXT("readiness") : TEXT("primary"));
	Root->SetNumberField(TEXT("readyFrame"), static_cast<double>(State.ReadyFrame));
	Root->SetNumberField(TEXT("spawnFrame"), static_cast<double>(State.SpawnFrame));
	Root->SetNumberField(TEXT("stableProbeSuccesses"), State.StableProbeSuccesses);
	Root->SetBoolField(TEXT("validationSuppressed"), true);
	return ToJsonLine(Root);
#endif
}

FString UCGQABridge::ScheduleEditorMainMapVisualProduction()
{
#if !WITH_EDITOR
	return ErrorJson(
		TEXT("EDITOR_ONLY"),
		TEXT("MainMap visual production 예약은 에디터 빌드에서만 사용할 수 있습니다."));
#else
	if (!IsInGameThread())
	{
		const FString ErrorCode(TEXT("GAME_THREAD_REQUIRED"));
		const FString ErrorMessage(TEXT("MainMap visual production 예약은 game thread에서만 허용됩니다."));
		FailScheduledMainMapVisualProduction(ErrorCode, ErrorMessage);
		return ErrorJson(ErrorCode, ErrorMessage);
	}

	FString ErrorCode;
	FString ErrorMessage;
	if (!TryValidateE6DataValidationSuppressed(ErrorCode, ErrorMessage))
	{
		FailScheduledMainMapVisualProduction(ErrorCode, ErrorMessage);
		return ErrorJson(ErrorCode, ErrorMessage);
	}

	FString Mode;
	if (!TryGetMainMapVisualProductionCommandLineMode(Mode, ErrorCode, ErrorMessage))
	{
		FailScheduledMainMapVisualProduction(ErrorCode, ErrorMessage);
		return ErrorJson(ErrorCode, ErrorMessage);
	}
	FString ScriptPath;
	if (!TryGetMainMapVisualProductionScriptPath(ScriptPath, ErrorCode, ErrorMessage))
	{
		FailScheduledMainMapVisualProduction(ErrorCode, ErrorMessage);
		return ErrorJson(ErrorCode, ErrorMessage);
	}
	UWorld* EditorWorld = nullptr;
	if (!TryGetMainMapVisualProductionEditorWorld(EditorWorld, ErrorCode, ErrorMessage))
	{
		FailScheduledMainMapVisualProduction(ErrorCode, ErrorMessage);
		return ErrorJson(ErrorCode, ErrorMessage);
	}

	FCGMainMapVisualProductionState& State = GetMainMapVisualProductionState();
	const bool bPrepareSaveRequest = State.bDispatchConsumed
		&& State.Phase == ECGMainMapVisualProductionPhase::PrepareMutationDispatching
		&& State.ScheduledMode == TEXT("prepare")
		&& State.DispatchCount == 1;
	if (State.bDispatchConsumed && !bPrepareSaveRequest)
	{
		const FString ScheduleErrorCode(TEXT("PRODUCTION_ALREADY_SCHEDULED"));
		const FString ScheduleErrorMessage(
			TEXT("이 UnrealEditor 프로세스에서는 허용된 prepare-save 재진입 외 추가 production 예약을 거부합니다."));
		FailScheduledMainMapVisualProduction(ScheduleErrorCode, ScheduleErrorMessage);
		return ErrorJson(ScheduleErrorCode, ScheduleErrorMessage);
	}

	if (bPrepareSaveRequest)
	{
		if (Mode != State.ScheduledMode
			|| State.ScheduledMapPackage != GetMainMapVisualProductionMapPackage()
			|| !FPaths::IsSamePath(ScriptPath, State.ScheduledScriptPath))
		{
			const FString ReentryErrorCode(TEXT("PRODUCTION_PREPARE_REENTRY_IDENTITY_MISMATCH"));
			const FString ReentryErrorMessage(TEXT("MainMap production prepare re-entry identity가 다릅니다."));
			FailScheduledMainMapVisualProduction(ReentryErrorCode, ReentryErrorMessage);
			return ErrorJson(ReentryErrorCode, ReentryErrorMessage);
		}
		if (State.BeginFrameHandle.IsValid() || State.PostEditorTickHandle.IsValid())
		{
			const FString HandleErrorCode(TEXT("PRODUCTION_PREPARE_REENTRY_HANDLE_CONFLICT"));
			const FString HandleErrorMessage(TEXT("MainMap production re-entry 전에 이전 delegate가 제거되지 않았습니다."));
			FailScheduledMainMapVisualProduction(HandleErrorCode, HandleErrorMessage);
			return ErrorJson(HandleErrorCode, HandleErrorMessage);
		}

		USphereReflectionCaptureComponent* Component = nullptr;
		if (!TryValidateMainMapVisualProductionCapture(
			EditorWorld,
			ErrorCode,
			ErrorMessage,
			&Component))
		{
			FailScheduledMainMapVisualProduction(ErrorCode, ErrorMessage);
			return ErrorJson(ErrorCode, ErrorMessage);
		}
		if (GFrameCounter <= State.ReadyFrame || State.StableProbeSuccesses != 2)
		{
			const FString FrameErrorCode(TEXT("PRODUCTION_PREPARE_REENTRY_FRAME_MISMATCH"));
			const FString FrameErrorMessage(TEXT("MainMap production spawn frame 또는 readiness 증거가 다릅니다."));
			FailScheduledMainMapVisualProduction(FrameErrorCode, FrameErrorMessage);
			return ErrorJson(FrameErrorCode, FrameErrorMessage);
		}
		if (!GEngine)
		{
			const FString EngineErrorCode(TEXT("NO_ENGINE"));
			const FString EngineErrorMessage(TEXT("MainMap production PostEditorTick을 등록할 Engine이 없습니다."));
			FailScheduledMainMapVisualProduction(EngineErrorCode, EngineErrorMessage);
			return ErrorJson(EngineErrorCode, EngineErrorMessage);
		}

		State.PrepareMapBuildDataId = Component->MapBuildDataId;
		State.SpawnFrame = GFrameCounter;
		const FDelegateHandle PostEditorTickHandle =
			GEngine->OnPostEditorTick().AddStatic(
				&RunScheduledMainMapVisualProductionVerifyAndSaveAfterEditorTick);
		if (!PostEditorTickHandle.IsValid())
		{
			const FString BindErrorCode(TEXT("PRODUCTION_POST_EDITOR_TICK_BIND_FAILED"));
			const FString BindErrorMessage(TEXT("MainMap production verification callback 등록에 실패했습니다."));
			FailScheduledMainMapVisualProduction(BindErrorCode, BindErrorMessage);
			return ErrorJson(BindErrorCode, BindErrorMessage);
		}

		State.PostEditorTickHandle = PostEditorTickHandle;
		State.Phase = ECGMainMapVisualProductionPhase::PrepareVerificationPending;

		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetBoolField(TEXT("ok"), true);
		Root->SetBoolField(TEXT("scheduled"), true);
		Root->SetStringField(TEXT("operation"), TEXT("RunMainMapVisualProduction"));
		Root->SetStringField(TEXT("mode"), Mode);
		Root->SetStringField(TEXT("mapPackage"), State.ScheduledMapPackage);
		Root->SetStringField(TEXT("phase"), TEXT("prepareSave"));
		Root->SetNumberField(TEXT("readyFrame"), static_cast<double>(State.ReadyFrame));
		Root->SetNumberField(TEXT("spawnFrame"), static_cast<double>(State.SpawnFrame));
		Root->SetNumberField(TEXT("stableProbeSuccesses"), State.StableProbeSuccesses);
		Root->SetNumberField(TEXT("dispatchCount"), State.DispatchCount);
		Root->SetStringField(
			TEXT("mapBuildDataId"),
			State.PrepareMapBuildDataId.ToString(EGuidFormats::DigitsWithHyphens));
		Root->SetBoolField(TEXT("validationSuppressed"), true);
		return ToJsonLine(Root);
	}

	if (State.Phase != ECGMainMapVisualProductionPhase::Idle
		|| State.BeginFrameHandle.IsValid()
		|| State.PostEditorTickHandle.IsValid()
		|| State.DispatchCount != 0)
	{
		const FString InitialErrorCode(TEXT("INVALID_PRODUCTION_INITIAL_STATE"));
		const FString InitialErrorMessage(TEXT("MainMap production 예약 state가 초기 상태가 아닙니다."));
		FailScheduledMainMapVisualProduction(InitialErrorCode, InitialErrorMessage);
		return ErrorJson(InitialErrorCode, InitialErrorMessage);
	}
	if (!GEngine)
	{
		const FString EngineErrorCode(TEXT("NO_ENGINE"));
		const FString EngineErrorMessage(TEXT("MainMap production readiness를 등록할 Engine이 없습니다."));
		FailScheduledMainMapVisualProduction(EngineErrorCode, EngineErrorMessage);
		return ErrorJson(EngineErrorCode, EngineErrorMessage);
	}

	State.ScheduledMode = Mode;
	State.ScheduledMapPackage = EditorWorld->GetOutermost()->GetName();
	State.ScheduledScriptPath = ScriptPath;
	State.StableProbeStartedAt = FPlatformTime::Seconds();
	const FDelegateHandle PostEditorTickHandle =
		GEngine->OnPostEditorTick().AddStatic(
			&RunScheduledMainMapVisualProductionReadinessProbe);
	if (!PostEditorTickHandle.IsValid())
	{
		const FString BindErrorCode(TEXT("PRODUCTION_READINESS_BIND_FAILED"));
		const FString BindErrorMessage(TEXT("MainMap production readiness callback 등록에 실패했습니다."));
		FailScheduledMainMapVisualProduction(BindErrorCode, BindErrorMessage);
		return ErrorJson(BindErrorCode, BindErrorMessage);
	}

	State.PostEditorTickHandle = PostEditorTickHandle;
	State.bDispatchConsumed = true;
	State.Phase = ECGMainMapVisualProductionPhase::ReadinessPending;

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("ok"), true);
	Root->SetBoolField(TEXT("scheduled"), true);
	Root->SetStringField(TEXT("operation"), TEXT("RunMainMapVisualProduction"));
	Root->SetStringField(TEXT("mode"), Mode);
	Root->SetStringField(TEXT("mapPackage"), State.ScheduledMapPackage);
	Root->SetStringField(TEXT("phase"), TEXT("readiness"));
	Root->SetNumberField(TEXT("readyFrame"), static_cast<double>(State.ReadyFrame));
	Root->SetNumberField(TEXT("spawnFrame"), static_cast<double>(State.SpawnFrame));
	Root->SetNumberField(TEXT("stableProbeSuccesses"), State.StableProbeSuccesses);
	Root->SetNumberField(TEXT("dispatchCount"), State.DispatchCount);
	Root->SetBoolField(TEXT("validationSuppressed"), true);
	return ToJsonLine(Root);
#endif
}

FString UCGQABridge::BuildEditorReflectionCaptures(const FString& ExpectedMapPackage)
{
#if !WITH_EDITOR
	(void)ExpectedMapPackage;
	return ErrorJson(TEXT("EDITOR_ONLY"), TEXT("Reflection Capture 빌드는 에디터에서만 사용할 수 있습니다."));
#else
	if (!CGQABridgePolicy::IsReflectionBuildMapAllowed(ExpectedMapPackage))
	{
		return ErrorJson(TEXT("MAP_NOT_ALLOWED"), TEXT("허용된 MainMap만 Reflection Capture를 빌드할 수 있습니다."));
	}
	if (IsRunningCommandlet())
	{
		return ErrorJson(TEXT("COMMANDLET_DISABLED"), TEXT("Reflection Capture 빌드는 일반 UnrealEditor에서 실행해야 합니다."));
	}
	if (!GEditor)
	{
		return ErrorJson(TEXT("NO_EDITOR"), TEXT("에디터 인스턴스를 찾을 수 없습니다."));
	}
	if (GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
	{
		return ErrorJson(TEXT("PLAY_SESSION_ACTIVE"), TEXT("PIE 또는 Simulate 실행 중에는 BuiltData를 수정하지 않습니다."));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return ErrorJson(TEXT("NO_EDITOR_WORLD"), TEXT("현재 에디터 월드를 찾을 수 없습니다."));
	}
	if (World->WorldType != EWorldType::Editor || World->IsGameWorld())
	{
		return ErrorJson(TEXT("INVALID_WORLD_TYPE"), TEXT("일반 에디터 월드에서만 Reflection Capture를 빌드할 수 있습니다."));
	}

	const FString CurrentMapPackage = World->GetOutermost()->GetName();
	if (CurrentMapPackage != ExpectedMapPackage)
	{
		return ErrorJson(TEXT("MAP_MISMATCH"), TEXT("현재 에디터 월드가 요청한 E6 임시 맵과 다릅니다."));
	}
	if (!World->Scene)
	{
		return ErrorJson(TEXT("NO_SCENE"), TEXT("Reflection Capture를 빌드할 렌더링 Scene이 없습니다."));
	}
	if (World->GetFeatureLevel() < ERHIFeatureLevel::SM5)
	{
		return ErrorJson(TEXT("FEATURE_LEVEL_TOO_LOW"), TEXT("Reflection Capture 빌드는 SM5 이상 에디터 월드가 필요합니다."));
	}

	FCGReflectionCaptureInventory Inventory;
	FString InventoryErrorCode;
	FString InventoryErrorMessage;
	if (!TryInventoryE6ReflectionCapture(World, Inventory, InventoryErrorCode, InventoryErrorMessage))
	{
		return ErrorJson(InventoryErrorCode, InventoryErrorMessage);
	}
	if (Inventory.Actor->GetLevel() != World->PersistentLevel)
	{
		return ErrorJson(TEXT("CAPTURE_LEVEL_MISMATCH"), TEXT("E6 Reflection Capture는 임시 맵의 Persistent Level에 있어야 합니다."));
	}

	USphereReflectionCaptureComponent* Component = Inventory.Component;
	if (Component->Mobility != EComponentMobility::Static)
	{
		return ErrorJson(TEXT("CAPTURE_NOT_STATIC"), TEXT("E6 Reflection Capture 컴포넌트는 Static이어야 합니다."));
	}
	if (Component->ReflectionSourceType != EReflectionSourceType::SpecifiedCubemap || !Component->Cubemap)
	{
		return ErrorJson(TEXT("INVALID_CAPTURE_SOURCE"), TEXT("E6 Reflection Capture에는 Specified Cubemap이 설정되어 있어야 합니다."));
	}
	if (!Component->MapBuildDataId.IsValid())
	{
		return ErrorJson(TEXT("INVALID_BUILD_DATA_ID"), TEXT("E6 Reflection Capture의 MapBuildDataId가 비어 있습니다."));
	}

	GEditor->BuildReflectionCaptures(World);

	FCGReflectionCaptureBuildDataSnapshot Snapshot;
	FString BuildDataErrorCode;
	FString BuildDataErrorMessage;
	if (!TryReadE6ReflectionBuildData(
		Component,
		ECGReflectionPayloadRequirement::RequireSerialized,
		Snapshot,
		BuildDataErrorCode,
		BuildDataErrorMessage))
	{
		return ErrorJson(BuildDataErrorCode, BuildDataErrorMessage);
	}

	const bool bMapDataSaved = FEditorFileUtils::SaveMapDataPackages(World, false);
	if (!bMapDataSaved)
	{
		return ErrorJson(TEXT("MAP_DATA_SAVE_FAILED"), TEXT("E6 Reflection Capture BuiltData 패키지 저장에 실패했습니다."));
	}

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	WriteE6ReflectionBuildData(Root, CurrentMapPackage, Inventory, Snapshot);
	Root->SetBoolField(TEXT("mapDataSaved"), bMapDataSaved);
	return ToJsonLine(Root);
#endif
}

FString UCGQABridge::DescribeEditorReflectionCaptureBuildData(
	UObject* WorldContextObject,
	const FString& ExpectedMapPackage)
{
#if !WITH_EDITOR
	(void)WorldContextObject;
	(void)ExpectedMapPackage;
	return ErrorJson(TEXT("EDITOR_ONLY"), TEXT("Reflection Capture BuiltData 조회는 에디터 빌드에서만 사용할 수 있습니다."));
#else
	if (!CGQABridgePolicy::IsReflectionBuildMapAllowed(ExpectedMapPackage))
	{
		return ErrorJson(TEXT("MAP_NOT_ALLOWED"), TEXT("허용된 MainMap Reflection Capture BuiltData만 조회할 수 있습니다."));
	}

	UWorld* World = ResolveWorld(WorldContextObject);
	if (!World)
	{
		return ErrorJson(TEXT("NO_WORLD"), TEXT("Reflection Capture BuiltData를 조회할 World가 없습니다."));
	}

	const FString CurrentMapPackage = World->GetOutermost()->GetName();
	if (CurrentMapPackage != ExpectedMapPackage)
	{
		return ErrorJson(TEXT("MAP_MISMATCH"), TEXT("현재 World가 요청한 E6 임시 맵과 다릅니다."));
	}

	FCGReflectionCaptureInventory Inventory;
	FString InventoryErrorCode;
	FString InventoryErrorMessage;
	if (!TryInventoryE6ReflectionCapture(World, Inventory, InventoryErrorCode, InventoryErrorMessage))
	{
		return ErrorJson(InventoryErrorCode, InventoryErrorMessage);
	}

	FCGReflectionCaptureBuildDataSnapshot Snapshot;
	FString BuildDataErrorCode;
	FString BuildDataErrorMessage;
	if (!TryReadE6ReflectionBuildData(
		Inventory.Component,
		ECGReflectionPayloadRequirement::AllowSerializedOrRuntimeConsumed,
		Snapshot,
		BuildDataErrorCode,
		BuildDataErrorMessage))
	{
		return ErrorJson(BuildDataErrorCode, BuildDataErrorMessage);
	}

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	WriteE6ReflectionBuildData(Root, CurrentMapPackage, Inventory, Snapshot);
	return ToJsonLine(Root);
#endif
}

FString UCGQABridge::BeginCapture(UObject* WorldContextObject, const FString& Command)
{
#if UE_BUILD_SHIPPING
	(void)WorldContextObject;
	(void)Command;
	return ErrorJson(TEXT("SHIPPING_DISABLED"), TEXT("Shipping 빌드에서는 QA 브리지가 비활성화됩니다."));
#else
	if (Command.TrimStartAndEnd().IsEmpty())
	{
		return ErrorJson(TEXT("EMPTY_COMMAND"), TEXT("실행할 명령이 비어 있습니다."));
	}
	UWorld* World = ResolveWorld(WorldContextObject);
	if (!World)
	{
		return ErrorJson(TEXT("NO_WORLD"), TEXT("게임 월드가 없습니다 — 에디터에서 Play(Alt+P) 후 다시 시도하세요."));
	}

	SweepStaleSessions();

	APlayerController* PC = World->GetFirstPlayerController();
	const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const TSharedPtr<FCGQALogSink> Sink = MakeShared<FCGQALogSink>();
	GLog->AddOutputDevice(Sink.Get());

	FString ConsoleOutput;
	if (PC)
	{
		ConsoleOutput = PC->ConsoleCommand(Command, /*bWriteToLog*/ true);
	}
	else
	{
		FStringOutputDevice Ar;
		GEngine->Exec(World, *Command, Ar);
		ConsoleOutput = Ar;
	}
	GLog->Flush();

	// 싱크를 떼지 않는 것이 이 함수의 요점이다 — 명령은 타이머로 계속 돌고,
	// 결과는 이후 PollCapture 가 긁어간다.
	Sessions().Add(SessionId, FCaptureSession{ Sink, Command, FPlatformTime::Seconds() });

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("ok"), true);
	Root->SetStringField(TEXT("sessionId"), SessionId);
	Root->SetStringField(TEXT("command"), Command);
	WriteWorldIdentity(Root, World, PC);
	Root->SetStringField(TEXT("console"), ConsoleOutput);
	Root->SetBoolField(TEXT("truncated"), Sink->bTruncated);
	AppendLines(Root, Sink->Drain());
	return ToJsonLine(Root);
#endif
}

FString UCGQABridge::PollCapture(const FString& SessionId, const FString& DoneMarker)
{
#if UE_BUILD_SHIPPING
	(void)SessionId;
	(void)DoneMarker;
	return ErrorJson(TEXT("SHIPPING_DISABLED"), TEXT("Shipping 빌드에서는 QA 브리지가 비활성화됩니다."));
#else
	FCaptureSession* Session = Sessions().Find(SessionId);
	if (!Session || !Session->Sink.IsValid())
	{
		return ErrorJson(TEXT("SESSION_NOT_FOUND"), TEXT("캡처 세션이 없습니다 — 이미 끝났거나 만료되었습니다."));
	}

	GLog->Flush();
	const TArray<FCGQALogSink::FLine> Lines = Session->Sink->Drain();

	// 종료 마커를 못 정한 명령은 스스로 끝났다고 말할 방법이 없다 — 호출자가 시간으로 끊는다.
	bool bDone = false;
	if (!DoneMarker.IsEmpty())
	{
		for (const FCGQALogSink::FLine& Line : Lines)
		{
			if (Line.Text.Contains(DoneMarker))
			{
				bDone = true;
				break;
			}
		}
	}

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("ok"), true);
	Root->SetStringField(TEXT("sessionId"), SessionId);
	Root->SetBoolField(TEXT("done"), bDone);
	Root->SetNumberField(TEXT("elapsed"), FPlatformTime::Seconds() - Session->StartedAt);
	Root->SetBoolField(TEXT("truncated"), Session->Sink->bTruncated);
	AppendLines(Root, Lines);
	return ToJsonLine(Root);
#endif
}

FString UCGQABridge::EndCapture(const FString& SessionId)
{
#if UE_BUILD_SHIPPING
	(void)SessionId;
	return ErrorJson(TEXT("SHIPPING_DISABLED"), TEXT("Shipping 빌드에서는 QA 브리지가 비활성화됩니다."));
#else
	const bool bExisted = Sessions().Contains(SessionId);
	CloseSession(SessionId);

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("ok"), true);
	Root->SetBoolField(TEXT("closed"), bExisted);
	Root->SetNumberField(TEXT("openSessions"), Sessions().Num());
	return ToJsonLine(Root);
#endif
}
