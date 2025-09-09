// Fill out your copyright notice in the Description page of Project Settings.


#include "PongPlayerController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "GameFramework/GameMode.h"
#include "Kismet/GameplayStatics.h"
#include "PingPong/Actors/PongPlatform.h"
#include "PingPong/GameStates/PongGameState.h"
#include "PingPong/Pawns/PongPlayerPawn.h"
#include "PingPong/Pawns/PongSpectatorPawn.h"
#include "PingPong/PlayerStates/PongPlayerState.h"
#include "PingPong/UI/OverlayWidget.h"
#include "PingPong/UI/HUDs/BaseHUD.h"

void APongPlayerController::BeginPlay()
{	
	FString PlatformName = UGameplayStatics::GetPlatformName();
	if (!PlatformName.Contains(TEXT("Android")))
	{
		SetVirtualJoystickVisibility(false);
	}
	if(IsLocalPlayerController())
	{
		AHUD* HUD = GetHUD();
		if(HUD)
		{
			PingPongHUD = Cast<AGameHUD>(HUD);
		}
	}
	if(!PingPongGameState)
	{
		PingPongGameState = Cast<APongGameState>(UGameplayStatics::GetGameState(GetWorld()));	
	}
	PingPongGameState = Cast<APongGameState>(UGameplayStatics::GetGameState(GetWorld()));
	check(PingPongGameState);
	PingPongGameState->OnMatchStateChanged.AddDynamic(this,&ThisClass::HandleMatchStateChange);
	UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(this);	
	ModificationsCount = PingPongGameState->GeBallModificatorsCount();	
	Super::BeginPlay();
	if (!PlatformName.Contains(TEXT("Android")))
	{
		SetShowMouseCursor(true);
	}
	else
	{
		SetShowMouseCursor(false);
	}
}

void APongPlayerController::Tick(float DeltaSeconds)
{	
	Super::Tick(DeltaSeconds);
	if(Platform)
	{		
		Platform->Server_Rotate(0);
		if(!bIsMovingForward && !bIsMovingSides) Platform->Floating();
	}	
}


APongPlayerController::APongPlayerController()
{
	bReplicates=true;
}

void APongPlayerController::SetStartTransform(FTransform NewStartTransform)
{
	StartTransform=NewStartTransform;
}

void APongPlayerController::PreInitializeComponents()
{

	Super::PreInitializeComponents();
	bReplicates=true;
}

void APongPlayerController::SpawnPlatform_Implementation()
{
	Platform = (APongPlatform*)GetWorld()->SpawnActor<APongPlatform>(PlatformClass);
    if(Platform)
    {
		Platform->SetActorLocation(StartTransform.GetLocation());
		Platform->SetActorRotation(StartTransform.GetRotation());
    	Platform->SetOwner(GetPawn());
    }
	else
	{
		UE_LOG(LogTemp, Error, TEXT("APongPlayerController::SpawnPlatform_Implementation: HAS NO Platform SUBClass!"));
	}
}

bool APongPlayerController::SpawnPlatform_Validate()
{
	return PlatformClass != NULL;
}
#if PLATFORM_ANDROID
void APongPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindAxis("LeftRight", this,&APongPlayerController::MoveRight);
	InputComponent->BindAxis("ForwardBackward", this,&APongPlayerController::MoveForward);
	InputComponent->BindAction("Menu",EInputEvent::IE_Pressed,this, &APongPlayerController::OpenMenu);
	InputComponent->BindAction("Fire",EInputEvent::IE_Pressed,this, &APongPlayerController::Fire);
	//TODO need to fix it - due to compilation error  G:/Git/UE5_PingPong/Source/PingPong/PlayerControllers/PingPongPlayerController.cpp(101,18): error: no matching member function for call to 'BindAction'
	//InputComponent->BindAction("ScrollColor",EInputEvent::IE_Pressed,this, &APongPlayerController::ScrollColor);
	InputComponent->BindAxis("AndroidRotate", this,&APongPlayerController::RotatePlatform);	
	InputComponent->bBlockInput = true;	
}

#else
	
void APongPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindAxis("LeftRight", this,&APongPlayerController::MoveRight);
	InputComponent->BindAxis("ForwardBackward", this,&APongPlayerController::MoveForward);
	InputComponent->BindAxis("ScrollColor",this, &APongPlayerController::ScrollColor);
	InputComponent->BindAxis("MouseX", this,&APongPlayerController::RotatePlatform);
	InputComponent->BindAction("Menu",EInputEvent::IE_Pressed,this, &APongPlayerController::OpenMenu);
	InputComponent->BindAction("Fire",EInputEvent::IE_Pressed,this, &APongPlayerController::Fire);
	
}
#endif


void APongPlayerController::MoveRight(float AxisValue)
{
	if (RightValue==AxisValue)
		return;
	RightValue=AxisValue;
	if (APongSpectatorPawn* SpecPawn = Cast<APongSpectatorPawn>(GetPawn()))
	{
		FRotator ControlRot = GetControlRotation();
		const FVector Direction = FRotationMatrix(ControlRot).GetUnitAxis(EAxis::Y);
		SpecPawn->AddMovementInput(Direction, AxisValue);
	}
	else if (APongPlayerPawn* GamePawn = Cast<APongPlayerPawn>(GetPawn()))
	{
		// Game pawn logic
		if (bBlockMovement)
			return;
		Server_PlatformMoveRight(AxisValue);
	}	
	
}

void APongPlayerController::MoveForward(float AxisValue)
{
	if (ForwardValue==AxisValue)
		return;
	ForwardValue=AxisValue;
	if (APongSpectatorPawn* SpecPawn = Cast<APongSpectatorPawn>(GetPawn()))
	{
		FRotator ControlRot = GetControlRotation();
		const FVector Direction = FRotationMatrix(ControlRot).GetUnitAxis(EAxis::X);
		SpecPawn->AddMovementInput(Direction, AxisValue);
	}
	else if (APongPlayerPawn* GamePawn = Cast<APongPlayerPawn>(GetPawn()))
	{
		// Game pawn logic
		if (bBlockMovement)
			return;
		Server_PlatformMoveForward(AxisValue);
	}	
}

void APongPlayerController::RotatePlatform(float AxisValue)
{
	if (APongSpectatorPawn* SpecPawn = Cast<APongSpectatorPawn>(GetPawn()))
	{
		// Spectator movement logic
		SpecPawn->AddControllerYawInput(AxisValue);
	}
	else if (APongPlayerPawn* GamePawn = Cast<APongPlayerPawn>(GetPawn()))
	{
		Server_PlatformRotate(AxisValue);
	}	
}

void APongPlayerController::RequstPause_Implementation(bool state)
{
	PingPongGameState->ServerPause(state);
}

bool APongPlayerController::RequstPause_Validate(bool state)
{
	return true;
}

void APongPlayerController::OpenMenu_Implementation()
{
	AHUD* HUD = GetHUD();
		AGameHUD* LocalHUD = HUD ? Cast<AGameHUD>(HUD) : nullptr;
	if (!LocalHUD) return;
	if (!PingPongHUD->IsWidgetVisible(Widgets::MainMenu))
	{
		SetShowMouseCursor(true);
		PingPongHUD->SwitchUI(Widgets::MainMenu);		
		//RequstPause(true); TODO Fix it!
	}
	else
	{
		if (PingPongHUD->IsWidgetVisible(Widgets::Settings))
			return;
		SetShowMouseCursor(false);
		PingPongHUD->SwitchUI(Widgets::MainMenu);
		//RequstPause(false);
	}
}

bool APongPlayerController::OpenMenu_Validate()
{	
	return true;
}

void APongPlayerController::Fire_Implementation()
{
	if(Platform)
	{
		PingPongPlayerState = GetPlayerState<APongPlayerState>();
		check(PingPongPlayerState);
		Platform->Server_Fire(PingPongPlayerState->GetModificator());
	}
}

bool APongPlayerController::Fire_Validate()
{
	return true;
}

void APongPlayerController::Server_PlatformMoveForward_Implementation(float AxisValue)
{	
	if(Platform)
	{		
		Platform->Server_GetForwardValue(AxisValue);
		if(AxisValue==0) bIsMovingForward=false;
		else bIsMovingForward=true;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("APongPlayerController::Server_PlatformMoveRight_Implementation: HAS NO PLATFORM!"));
	}
}

bool APongPlayerController::Server_PlatformMoveForward_Validate(float AxisValue)
{
	return true;
}

void APongPlayerController::Server_PlatformMoveRight_Implementation(float AxisValue)
{	
	if(Platform)
	{		
		Platform->Server_GetRightValue(AxisValue);
		if(AxisValue==0) bIsMovingSides=false;
		else bIsMovingSides=true;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("APongPlayerController::Server_PlatformMoveRight_Implementation: HAS NO PLATFORM!"));
	}
}

bool APongPlayerController::Server_PlatformMoveRight_Validate(float AxisValue)
{
	return true;
}



bool APongPlayerController::Initialize_Validate()
{
	return true;
}

void APongPlayerController::Initialize_Implementation()
{
	if(Platform)
		Platform->Destroy();
	SpawnPlatform();
}

void APongPlayerController::Server_PlatformRotate_Implementation(float AxisValue)
{
	if(Platform)
	{		
		Platform->Server_Rotate(AxisValue);
	}
}

bool APongPlayerController::Server_PlatformRotate_Validate(float AxisValue)
{
	return true;
}

void APongPlayerController::ScrollColorOnServer_Implementation(float Axis)
{
	PingPongPlayerState = GetPlayerState<APongPlayerState>();
	if (Axis > 0)
	{				
		PingPongPlayerState->NextModificator();
	}
	else
	{
		PingPongPlayerState->PrevModificator();
	}
	// Update UI
	SetColorAndPriceUI(
		PingPongGameState->GetModificatorColor(PingPongPlayerState->GetModificator()),
		PingPongGameState->GetShotCost(PingPongPlayerState->GetModificator()),
		PingPongGameState->GetTextColor(PingPongPlayerState->GetModificator())
	);
}

bool APongPlayerController::ScrollColorOnServer_Validate(float Axis)
{
	return true;
}


void APongPlayerController::SetUIStatus(EUIStatus status)
{
	if(HasAuthority() && UKismetSystemLibrary::IsDedicatedServer(GetWorld())) return; 
	PingPongGameState = Cast<APongGameState>(UGameplayStatics::GetGameState(GetWorld()));
	UIStatus=status;
	switch (UIStatus)
	{
	case EUIStatus::UILoaded: //UILoaded
		{			
			ServerIncreaseUILoaded();			
			break;
		}
	case EUIStatus::ReadyButtonPressed: //ReadyButtonPressed
		{
			ServerIncreaseReadyPlayers();
			break;
		}
	case EUIStatus::UIPaused: //UIPaused
		{
				
			break;
		}
	case EUIStatus::Started:
		{
			ServerIncreaseStartedPlayers();
			break;
		}
	default:
		{
				
		}
	}
}

TMap<uint32, FString> APongPlayerController::GetPlayersInGame()
{	
	return PlayerList;
}

void APongPlayerController::ScrollColor(float Axis)
{
	
	if (FMath::IsNearlyZero(Axis)) // Handle floating-point precision
		return;
	ScrollColorOnServer(Axis);
	
}

void APongPlayerController::HandleMatchStateChange(FName NewState)
{
	
	if(NewState==MatchState::InProgress)
	{
		bBlockMovement=false;
	}
	if(NewState==MatchState::WaitingPostMatch)
	{
		bBlockMovement=true;
	}
}

void APongPlayerController::ServerIncreaseStartedPlayers_Implementation()
{
	APongGameState* MyGS = GetWorld()->GetGameState<APongGameState>();
	if (MyGS)
	{
		MyGS->IncreaseStartedPlayers();
	}
}

bool APongPlayerController::ServerIncreaseStartedPlayers_Validate()
{
	return true;
}

void APongPlayerController::ServerIncreaseReadyPlayers_Implementation()
{
	APongGameState* MyGS = GetWorld()->GetGameState<APongGameState>();
	if (MyGS)
	{
		MyGS->IncreaseReadyPlayer();
	}
}

bool APongPlayerController::ServerIncreaseReadyPlayers_Validate()
{
	return true;
}

void APongPlayerController::ServerIncreaseUILoaded_Implementation()
{	
	APongGameState* MyGS = GetWorld()->GetGameState<APongGameState>();
	if (MyGS)
	{
		MyGS->IncreaseLoadedPlayer();
	}
}

bool APongPlayerController::ServerIncreaseUILoaded_Validate()
{
	return true;
}

void APongPlayerController::SetColorAndPriceUI_Implementation(FLinearColor Color,int32 Price,FLinearColor TextColor)
{
	PingPongHUD->GetOverlayWidget()->SetBallColor(Color);
	PingPongHUD->GetOverlayWidget()->SetBallShotCostText(Price);
	PingPongHUD->GetOverlayWidget()->SetBallShotCostTextColor(TextColor);
}

void APongPlayerController::SetNewScore_Implementation(int32 PlayerId, float Score)
{
	PingPongHUD->GetOverlayWidget()->UpdateScore(PlayerId,Score);
}

