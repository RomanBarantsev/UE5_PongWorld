// Copyright Epic Games, Inc. All Rights Reserved.


#include "PongGameMode.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpectatorPawn.h"
#include "Kismet/GameplayStatics.h"
#include "PingPong/Actors/PongGoal.h"
#include "PingPong/Actors/PongPlatform.h"
#include "PingPong/GameInstance/Pong_GameInstance.h"
#include "PingPong/GameStates/PongGameState.h"
#include "PingPong/Pawns/PongPlayerPawn.h"
#include "PingPong/Pawns/PongSpectatorPawn.h"
#include "PingPong/PlayerControllers/PongPlayerController.h"
#include "PingPong/PlayerStates/PongPlayerState.h"
#include "PingPong/UI/HUDs/BaseHUD.h"

APongGameMode::APongGameMode()
{
	DefaultPawnClass = APongPlayerPawn::StaticClass();
	PlayerControllerClass = APongPlayerController::StaticClass();
	HUDClass = ABaseHUD::StaticClass();
	GameStateClass = APongGameState::StaticClass();
	PlayerStateClass = APongPlayerState::StaticClass();
}

void APongGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	if (MapName.Contains(TEXT("4Players")))
	{
		PlayersCount = 4;
	}
	else
	{
		PlayersCount = 2;
	}
}

void APongGameMode::PostLogin(APlayerController* NewPlayer)
{	
	UWorld* world = GetWorld();
	check(world);
	PingPongGameState = Cast<APongGameState>( GetGameState<APongGameState>());
	check(PingPongGameState);
	if (PingPongGameState->PlayerStates.Num()==PlayersCount)
	{
		auto NewPawn = GetWorld()->SpawnActor<ASpectatorPawn>(APongSpectatorPawn::StaticClass());
		NewPawn->SetActorLocation(FVector::Zero());
		NewPawn->SetActorRotation(FRotator(0, 0, 0));
		NewPlayer->Possess(NewPawn);
	}
	else
	{
		APongPlayerController* PingPongPlayerController = Cast<APongPlayerController>(NewPlayer);
		APongPlayerPawn* Pawn = CreatePawnForController( PingPongPlayerController,world);
		SetPawnRotationAndLocation(Pawn,PingPongPlayerController);
		SetClosestGoalOwner(Pawn);
		Super::PostLogin(NewPlayer);
		if (HasAuthority() && GetNetMode() == NM_DedicatedServer && !UE_EDITOR)
		{
			auto GI = UGameplayStatics::GetGameInstance(GetWorld());
			if (GI)
			{
				auto Pong_GI = Cast<UPong_GameInstance>(GI);
				if (Pong_GI)
				{
					Pong_GI->PlayersUpdate();
				}
			}
		}	
		PingPongGameState->HandlePlayerStatesUpdated();
	}	
}

void APongGameMode::Logout(AController* Exiting)
{
	//TODO client which reconnect after close window is desync, and platform stays in level. Some bug need to fix. 
	APongPlayerController* PingPongPlayerController = Cast<APongPlayerController>(Exiting);
	PingPongGameState = Cast<APongGameState>( GetGameState<APongGameState>());
	check(PingPongGameState);
	PingPongGameState->DecreaseLoadedPlayer(Exiting);
	if (IsNetMode(NM_ListenServer) || IsNetMode(NM_DedicatedServer) || Exiting)
	{
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), APongPlatform::StaticClass(), FoundActors);
		for(AActor* Actor : FoundActors)
		{
			if (Actor->GetOwner() && Actor->GetOwner()->GetInstigatorController()==nullptr)
			{
				Actor->Destroy();
			}
		}
	}	
	Super::Logout(Exiting);
	if (GetNetMode() == NM_DedicatedServer && !UE_EDITOR)
	{
		auto GI = UGameplayStatics::GetGameInstance(GetWorld());
		if (GI)
		{
			auto Pong_GI = Cast<UPong_GameInstance>(GI);
			if (Pong_GI)
			{
				if (GetNumPlayers()==0)
				{
					Pong_GI->PlayersUpdate();
					Pong_GI->HostShutdown();
					FGenericPlatformMisc::RequestExit(false);
				}
				else
				{
					Pong_GI->PlayersUpdate();
				}
			}		
		}
	}	
	PingPongGameState->HandlePlayerStatesUpdated();
}

void APongGameMode::FinishGameServer()
{
	if (!HasAuthority())
		return;
	UAgonesSubsystem* Agones = UAgonesSubsystem::Get(this);
	if (!IsValid(Agones))
	{
		UE_LOG(LogTemp, Warning, TEXT("Agones subsystem is unavailable"));
		return;
	}
	Agones->Shutdown(FShutdownDelegate(),FAgonesErrorDelegate());
	UE_LOG(LogTemp, Log, TEXT("Agones shutdown requested"));
}

APlayerController* APongGameMode::SpawnPlayerController(ENetRole InRemoteRole, const FString& Options)
{
	if (!PingPongGameState)
		return Super::SpawnPlayerController(InRemoteRole, Options);;
	if (PingPongGameState->PlayerStates.Num()==PlayersCount)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto SpectatorController = GetWorld()->SpawnActor<APongPlayerController>(
		APongPlayerController::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
		return SpectatorController;
	}	
	else
	{
		return Super::SpawnPlayerController(InRemoteRole, Options);
	}
	
}

APongPlayerPawn* APongGameMode::CreatePawnForController(APongPlayerController* PingPongPlayerController,
                                                        UWorld* World)
{
	APongPlayerPawn* newPawn = Cast<APongPlayerPawn>(PingPongPlayerController->GetPawn());
	if(!newPawn)
	{
		newPawn = World->SpawnActor<APongPlayerPawn>(DefaultPawnClass);
		return newPawn;
	}
	return nullptr;	
}

void APongGameMode::SetClosestGoalOwner(APongPlayerPawn* PingPongPlayerPawn)
{
	if(!HasAuthority()) return;
	TArray<AActor*> foundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(),APongGoal::StaticClass(),foundActors);
	float lastDistance=20000.0f;
	APongGoal* ClosestGoal=nullptr;
	for (auto FoundActor : foundActors)
	{
		APongGoal* PongGoal = Cast<APongGoal>(FoundActor);
		if(!PongGoal) continue;
		float distance = FVector::Dist2D(PingPongPlayerPawn->GetActorLocation(),PongGoal->GetActorLocation());
		if(distance<lastDistance)
		{
			ClosestGoal=PongGoal;
			lastDistance=distance;
		}		
	}
	ClosestGoal->SetOwner(PingPongPlayerPawn);
}

int APongGameMode::GetPlayersCount() const
{
	return PlayersCount;
}

void APongGameMode::SetPawnRotationAndLocation_Implementation(APongPlayerPawn* PingPongPlayerPawn,
                                                                  APongPlayerController* PingPongPlayerController)
{
	TArray<AActor*> foundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(),APlayerStart::StaticClass(), foundActors);
	APlayerStart* startPos=Cast<APlayerStart>(foundActors[UGameplayStatics::GetNumPlayerControllers(GetWorld())-1]);	
	if(startPos && PingPongPlayerPawn)
	{
		PingPongPlayerPawn->SetActorLocation(startPos->GetActorLocation());
		PingPongPlayerPawn->SetActorRotation(startPos->GetActorRotation());
		PingPongPlayerController->Possess(PingPongPlayerPawn);
		PingPongPlayerController->SetStartTransform(startPos->GetActorTransform());
		PingPongPlayerController->Initialize();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Start position not setted in PingPongGameMode!"));
	}
}

