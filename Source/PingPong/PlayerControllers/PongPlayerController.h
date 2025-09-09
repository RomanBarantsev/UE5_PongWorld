// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PingPong/GameStates/PongGameState.h"
#include "PingPong/UI/HUDs/GameHUD.h"
#include "PongPlayerController.generated.h"

class APongPlayerState;
class ABaseHUD;
UENUM()
enum class EUIStatus : uint8
{
	NONE,
	UILoaded,
	ReadyButtonPressed,
	UIPaused,
	Started
};
enum class EPlayersStatus;
class APongGameState;
/**
 * 
 */
UCLASS()
class PINGPONG_API APongPlayerController : public APlayerController
{
	GENERATED_BODY()
protected:
	virtual void BeginPlay() override;
	TMap<uint32, FString> PlayerList;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY()
	FTransform StartTransform;
	UPROPERTY()
	APongGameState* PingPongGameState;
	UPROPERTY()
	APongPlayerState* PingPongPlayerState;
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite)
	TSubclassOf<class APongPlatform> PlatformClass;
	UPROPERTY()
	class APongPlatform* Platform;

public:
	APongPlayerController();
	UFUNCTION()
	void SetStartTransform(FTransform NewStartTransform);
	UFUNCTION(Server,Reliable,WithValidation)
	void Initialize();
	virtual void PreInitializeComponents() override;	
	UFUNCTION(Server,Reliable,WithValidation)
	void SpawnPlatform();
	virtual void SetupInputComponent() override;

public:
	UPROPERTY()
	bool bBlockMovement=true;
	UFUNCTION()
	virtual void MoveRight(float AxisValue);
	UFUNCTION()
	virtual void MoveForward(float AxisValue);
	UFUNCTION()
    virtual void RotatePlatform(float AxisValue);  
	UFUNCTION(Server,Reliable,WithValidation)
	virtual void Fire();
	UFUNCTION(Client,Reliable,WithValidation)
	void OpenMenu();
	float RightValue;
	UFUNCTION(Server,Reliable,WithValidation)
	void Server_PlatformMoveRight(float AxisValue) ;
	float ForwardValue;
	UFUNCTION(Server,Reliable,WithValidation)
	virtual void Server_PlatformMoveForward(float AxisValue);
	UFUNCTION(Server,Reliable,WithValidation)
	void Server_PlatformRotate(float AxisValue);
public:
	UFUNCTION(Server,Reliable,WithValidation)
	void RequstPause(bool state);
	
protected:
	UPROPERTY()
	AGameHUD* PingPongHUD;
	EUIStatus UIStatus;
	
public:
	
	UFUNCTION()
	void SetUIStatus(EUIStatus status);
	UFUNCTION(Client,Reliable)
	void SetNewScore(int32 PlayerId, float Score);
	UFUNCTION()
	TMap<uint32,FString> GetPlayersInGame();
private:
	int32 ModificationsCount;	
	UFUNCTION(Server,Reliable,WithValidation)
	void ScrollColorOnServer(float Axis);
	UFUNCTION()
	void ScrollColor(float Axis);
	UFUNCTION(Client,Reliable)
	void SetColorAndPriceUI(FLinearColor Color,int32 Price,FLinearColor TextColor);

private:
	bool bIsMovingSides=false;
	bool bIsMovingForward=false;
public:
	UFUNCTION()
	virtual void HandleMatchStateChange(FName NewState);
private:
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerIncreaseUILoaded();
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerIncreaseReadyPlayers();
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerIncreaseStartedPlayers();
};
