// Fill out your copyright notice in the Description page of Project Settings.

#include "Manager/ChatManagerSubsystem.h"
#include "Manager/PlayFabManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Data/GameSaveData.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Http.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"

void UChatManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LoadFirebaseConfig();

	// PlayFab 매니저 참조 캐싱
	if (UGameInstance* GI = GetGameInstance())
	{
		// PlayFabManager는 사용 시점에 GetSubsystem으로 가져옴 (초기화 순서 독립)
	}

	UE_LOG(LogTemp, Log, TEXT("[ChatManager] 초기화 완료 (Firebase URL: %s)"), *FirebaseBaseUrl);
}

void UChatManagerSubsystem::Deinitialize()
{
	StopPolling();
	Super::Deinitialize();
}

void UChatManagerSubsystem::LoadFirebaseConfig()
{
	// DefaultGame.ini [/Script/CompanyGrowthRenewal.ChatManagerSubsystem] 섹션에서 읽음
	const FString IniSection = TEXT("/Script/CompanyGrowthRenewal.ChatManagerSubsystem");

	// Firebase Base URL — ini가 단일 진실 원천
	FString LoadedUrl;
	const bool bRead = GConfig && GConfig->GetString(*IniSection, TEXT("FirebaseBaseUrl"), LoadedUrl, GGameIni);

	// 빈 값이 아니라 스킴까지 본다 — ini 값에 따옴표가 없으면 파서가 '//' 이후를 주석으로 잘라
	// "https:" 만 남는데, 그건 IsEmpty() 를 통과해 죽은 URL 로 폴링만 반복하게 된다 (PLAYBOOK §3.10)
	const bool bWellFormed = LoadedUrl.StartsWith(TEXT("https://")) || LoadedUrl.StartsWith(TEXT("http://"));

	if (bRead && bWellFormed)
	{
		FirebaseBaseUrl = LoadedUrl;
	}
	else
	{
		// 공개 포트폴리오에서는 실제 배포 주소 대신 문서용 placeholder를 사용한다.
		FirebaseBaseUrl = TEXT("https://YOUR_SERVICE_URL.example.com");
		UE_LOG(LogTemp, Error,
			TEXT("[ChatManager] ini FirebaseBaseUrl 이 유효하지 않아 폴백 사용 (읽은 값: '%s') — ini 값을 큰따옴표로 감쌌는지 확인할 것"),
			*LoadedUrl);
	}

	// 폴링 간격 — ini 없으면 3.0초 폴백
	float LoadedInterval = 0.0f;
	if (GConfig && GConfig->GetFloat(*IniSection, TEXT("PollIntervalSeconds"), LoadedInterval, GGameIni) && LoadedInterval > 0.0f)
	{
		ConfigPollIntervalSeconds = LoadedInterval;
	}
	else
	{
		ConfigPollIntervalSeconds = 3.0f;
	}

	UE_LOG(LogTemp, Log, TEXT("[ChatManager] FirebaseBaseUrl = %s, PollIntervalSeconds = %.1f"), *FirebaseBaseUrl, ConfigPollIntervalSeconds);
}

void UChatManagerSubsystem::SendMessage(const FString& Content, const FString& Channel)
{
	if (FirebaseBaseUrl.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[ChatManager] Firebase URL이 설정되지 않음"));
		return;
	}

	if (Content.IsEmpty())
	{
		return;
	}

	// PlayFab 유저 정보로 발신자 정보 구성
	FString SenderId = TEXT("anonymous");
	FString SenderName = TEXT("Anonymous");

	UGameInstance* GI = GetGameInstance();
	UPlayFabManagerSubsystem* PFMgr = GI ? GI->GetSubsystem<UPlayFabManagerSubsystem>() : nullptr;

	if (PFMgr && PFMgr->IsLoggedIn())
	{
		const FPlayFabUserInfo& UserInfo = PFMgr->GetUserInfo();
		SenderId = UserInfo.PlayFabId;
		SenderName = UserInfo.DisplayName.IsEmpty() ? UserInfo.PlayFabId : UserInfo.DisplayName;
	}

	// 프로필 이미지는 로컬 세이브가 권위 — 랭킹 업로드(RankingManagerSubsystem "pi")와 같은 출처
	int32 ProfileImageID = 0;
	if (USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr)
	{
		if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
		{
			ProfileImageID = SaveData->GameData.ProfileImageID;
		}
	}

	// JSON 바디 구성
	TSharedRef<FJsonObject> JsonBody = MakeShareable(new FJsonObject());
	JsonBody->SetStringField(TEXT("senderId"), SenderId);
	JsonBody->SetStringField(TEXT("senderName"), SenderName);
	JsonBody->SetStringField(TEXT("content"), Content);
	JsonBody->SetStringField(TEXT("channel"), Channel);
	JsonBody->SetNumberField(TEXT("profileImageId"), ProfileImageID);

	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	FJsonSerializer::Serialize(JsonBody, Writer);

	// HTTP POST 요청
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	const FString SendUrl = FString::Printf(TEXT("%s/chat/send"), *FirebaseBaseUrl);
	HttpRequest->SetURL(SendUrl);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestBody);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UChatManagerSubsystem::OnSendMessageResponse);
	HttpRequest->ProcessRequest();

	// 내 메시지는 로컬에서 즉시 표시 (서버 응답 안 기다림)
	FChatMessage LocalMsg;
	LocalMsg.SenderId = SenderId;
	LocalMsg.SenderName = SenderName;
	LocalMsg.Content = Content;
	LocalMsg.Channel = Channel;
	LocalMsg.ProfileImageID = ProfileImageID;
	LocalMsg.CreatedAt = FDateTime::UtcNow().ToIso8601();

	CachedMessages.Add(LocalMsg);
	while (CachedMessages.Num() > MaxCachedMessages)
	{
		CachedMessages.RemoveAt(0);
	}

	// LastMessageTimestamp 갱신 (폴링에서 내 메시지 중복 수신 방지)
	LastMessageTimestamp = LocalMsg.CreatedAt;

	OnMessagesReceived.Broadcast({LocalMsg});

	UE_LOG(LogTemp, Log, TEXT("[ChatManager] 메시지 전송 요청 (URL: %s, Channel: %s)"), *SendUrl, *Channel);
}

void UChatManagerSubsystem::FetchRecentMessages(const FString& Channel, int32 Limit)
{
	if (FirebaseBaseUrl.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[ChatManager] Firebase URL이 설정되지 않음"));
		return;
	}

	// URL 쿼리 파라미터 구성
	FString Url = FString::Printf(TEXT("%s/chat/messages?channel=%s&limit=%d"),
		*FirebaseBaseUrl, *Channel, Limit);

	// 폴링 최적화: 마지막 타임스탬프 이후 메시지만 요청
	if (!LastMessageTimestamp.IsEmpty())
	{
		// ISO 타임스탬프의 특수문자 URL 인코딩 (+, :)
		FString EncodedTimestamp = LastMessageTimestamp;
		EncodedTimestamp = EncodedTimestamp.Replace(TEXT("+"), TEXT("%2B"));
		EncodedTimestamp = EncodedTimestamp.Replace(TEXT(":"), TEXT("%3A"));
		Url += FString::Printf(TEXT("&after=%s"), *EncodedTimestamp);
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Url);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UChatManagerSubsystem::OnFetchMessagesResponse);
	HttpRequest->ProcessRequest();
}

void UChatManagerSubsystem::StartPolling(float IntervalSeconds)
{
	if (bIsPolling)
	{
		return;
	}

	// 인자 미지정(음수 기본값) 시 ini 설정값(ConfigPollIntervalSeconds) 사용, 양수면 호출자 지정값 우선
	const float ActualInterval = (IntervalSeconds > 0.0f) ? IntervalSeconds : ConfigPollIntervalSeconds;

	bIsPolling = true;

	// 즉시 한 번 조회
	FetchRecentMessages(CurrentPollingChannel);

	// 타이머 설정
	if (UWorld* World = GetGameInstance()->GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PollingTimerHandle,
			this,
			&UChatManagerSubsystem::PollMessages,
			ActualInterval,
			true // 반복
		);
	}

	UE_LOG(LogTemp, Log, TEXT("[ChatManager] 폴링 시작 (간격: %.1f초, 채널: %s)"), ActualInterval, *CurrentPollingChannel);
}

void UChatManagerSubsystem::StopPolling()
{
	if (!bIsPolling)
	{
		return;
	}

	bIsPolling = false;

	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		World->GetTimerManager().ClearTimer(PollingTimerHandle);
	}

	UE_LOG(LogTemp, Log, TEXT("[ChatManager] 폴링 중지"));
}

void UChatManagerSubsystem::PollMessages()
{
	FetchRecentMessages(CurrentPollingChannel);
}

// ===== HTTP 응답 핸들러 =====

void UChatManagerSubsystem::OnSendMessageResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[ChatManager] 메시지 전송 실패: HTTP 요청 에러"));
		OnSendResult.Broadcast(false);
		OnChatError.Broadcast(TEXT("메시지 전송 실패: 서버 연결 에러"));
		return;
	}

	const int32 ResponseCode = Response->GetResponseCode();
	if (ResponseCode != 200)
	{
		UE_LOG(LogTemp, Error, TEXT("[ChatManager] 메시지 전송 실패 (HTTP %d): %s"), ResponseCode, *Response->GetContentAsString());
		OnSendResult.Broadcast(false);
		OnChatError.Broadcast(FString::Printf(TEXT("메시지 전송 실패 (HTTP %d)"), ResponseCode));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[ChatManager] 메시지 전송 성공"));
	OnSendResult.Broadcast(true);
}

void UChatManagerSubsystem::OnFetchMessagesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ChatManager] 메시지 조회 실패: HTTP 요청 에러"));
		HandleFetchFailure();
		return;
	}

	if (Response->GetResponseCode() != 200)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ChatManager] 메시지 조회 실패 (HTTP %d)"), Response->GetResponseCode());
		HandleFetchFailure();
		return;
	}

	HandleFetchSuccess();

	TArray<FChatMessage> NewMessages = ParseMessagesFromJson(Response->GetContentAsString());

	// Optimistic UI 중복 방지: 이미 캐시에 있는 메시지 ID는 스킵
	NewMessages.RemoveAll([this](const FChatMessage& Msg)
	{
		for (const FChatMessage& Cached : CachedMessages)
		{
			// 로컬에서 보낸 메시지는 MessageId가 비어있으므로 SenderId+Content+시간 근접으로 비교
			if (!Cached.MessageId.IsEmpty() && Cached.MessageId == Msg.MessageId)
			{
				return true;
			}
			// 로컬 즉시 표시된 메시지 (MessageId 없음)와 서버에서 온 같은 메시지 비교
			if (Cached.MessageId.IsEmpty()
				&& Cached.SenderId == Msg.SenderId
				&& Cached.Content == Msg.Content)
			{
				return true;
			}
		}
		return false;
	});

	if (NewMessages.Num() > 0)
	{
		// 캐시에 추가
		CachedMessages.Append(NewMessages);

		// 캐시 크기 제한
		while (CachedMessages.Num() > MaxCachedMessages)
		{
			CachedMessages.RemoveAt(0);
		}

		// 마지막 타임스탬프 갱신 (다음 폴링에서 이후 메시지만 조회)
		LastMessageTimestamp = NewMessages.Last().CreatedAt;

		OnMessagesReceived.Broadcast(NewMessages);

		UE_LOG(LogTemp, Log, TEXT("[ChatManager] 새 메시지 %d개 수신"), NewMessages.Num());
	}
}

void UChatManagerSubsystem::HandleFetchFailure()
{
	++ConsecutiveFetchFailures;

	// 한 번 끊긴 걸로 경고하지 않고, 임계값을 넘는 순간 딱 한 번만 알린다
	if (ConsecutiveFetchFailures >= FetchFailureThreshold && bConnectionHealthy)
	{
		bConnectionHealthy = false;
		UE_LOG(LogTemp, Error, TEXT("[ChatManager] 폴링 %d회 연속 실패 — 채팅 서버에 연결할 수 없음 (URL: %s)"),
			ConsecutiveFetchFailures, *FirebaseBaseUrl);
		OnChatConnectionChanged.Broadcast(false);
	}
}

void UChatManagerSubsystem::HandleFetchSuccess()
{
	ConsecutiveFetchFailures = 0;

	if (!bConnectionHealthy)
	{
		bConnectionHealthy = true;
		UE_LOG(LogTemp, Log, TEXT("[ChatManager] 채팅 서버 연결 복구"));
		OnChatConnectionChanged.Broadcast(true);
	}
}

TArray<FChatMessage> UChatManagerSubsystem::ParseMessagesFromJson(const FString& JsonString)
{
	TArray<FChatMessage> Result;

	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ChatManager] JSON 파싱 실패"));
		return Result;
	}

	const TArray<TSharedPtr<FJsonValue>>* MessagesArray;
	if (!RootObject->TryGetArrayField(TEXT("messages"), MessagesArray))
	{
		return Result;
	}

	for (const TSharedPtr<FJsonValue>& MsgValue : *MessagesArray)
	{
		const TSharedPtr<FJsonObject>& MsgObj = MsgValue->AsObject();
		if (!MsgObj.IsValid())
		{
			continue;
		}

		FChatMessage Msg;
		Msg.MessageId = MsgObj->GetStringField(TEXT("messageId"));
		Msg.SenderId = MsgObj->GetStringField(TEXT("senderId"));
		Msg.SenderName = MsgObj->GetStringField(TEXT("senderName"));
		Msg.Content = MsgObj->GetStringField(TEXT("content"));
		Msg.Channel = MsgObj->GetStringField(TEXT("channel"));
		Msg.CreatedAt = MsgObj->GetStringField(TEXT("createdAt"));

		// 서버 배포 이전에 쌓인 문서엔 필드가 없다 — 없으면 0(기본 아이콘)으로 둔다
		double ParsedProfileImageID = 0.0;
		if (MsgObj->TryGetNumberField(TEXT("profileImageId"), ParsedProfileImageID))
		{
			Msg.ProfileImageID = static_cast<int32>(ParsedProfileImageID);
		}

		Result.Add(Msg);
	}

	return Result;
}
