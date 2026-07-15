// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NetworkGameInstance.h"
#include "Engine/GameInstance.h"
#include "Pong_GameInstance.generated.h"

class UPong_GameUserSettings;

USTRUCT()
struct FServerInfo
{
	GENERATED_BODY()
	FString IP;
	int Port;
	int CurrentPlayers;
	int MaxPlayers;
	FString Name;
};

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnServerListReady, const TArray<FServerInfo>&, ServerList);


class IHttpResponse;
class IHttpRequest;
/**
 * 
 */
UCLASS()
class PINGPONG_API UPong_GameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	void OnServerListGet(TSharedPtr<IHttpRequest> HttpRequest, TSharedPtr<IHttpResponse> HttpResponse, bool bArg);
	void GetServersList();
	void OnCreateHostCompleted(TSharedPtr<IHttpRequest> HttpRequest, TSharedPtr<IHttpResponse> HttpResponse, bool bArg);
	void CreateHost(FString map,FString serverName,uint32 id);
	void PlayersUpdate();
	void HostShutdown();
public:
	FOnServerListReady OnServerListReady;

private:
	UPROPERTY()
	UPong_GameUserSettings* Pong_Settings;
	UPROPERTY()
	APlayerState* PlayerState;
	UPROPERTY()
	UGameUserSettings* Settings;
	virtual void Init() override;
	virtual void Shutdown() override;
	FString PublicServerAddress = TEXT("193.124.254.197");
	FString CrowServerAddress = TEXT("localhost");
	FTimerHandle ShutDownServerHandle;
	void OnTimer();
	float WaitTime=120;
};