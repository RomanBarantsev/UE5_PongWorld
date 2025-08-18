// Fill out your copyright notice in the Description page of Project Settings.


#include "PongPlatform.h"
#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>
#include "Net/UnrealNetwork.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/ArrowComponent.h"
#include "Components/AudioComponent.h"
#include "PingPong/PlayerControllers/PongPlayerController.h"
#include "PingPong/PlayerStates/PongPlayerState.h"

// Sets default values
APongPlatform::APongPlatform()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;	
	MeshRoot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformBody BodyMeshRoot"));
	SetRootComponent(MeshRoot);
	MeshRoot->SetIsReplicated(true);
	MeshRoot->bHiddenInGame=true;
	MeshRoot->SetWorldScale3D(FVector(3,3,3));
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformBody Mesh"));
	BodyMesh->SetupAttachment(RootComponent);	
	BodyMesh->SetIsReplicated(true);
	BodyMesh->SetWorldScale3D(FVector(0.33,1,0.33));
	ShootDirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Shoot Direction Arrow"));
	ShootDirectionArrow->SetupAttachment(BodyMesh);
	ShootDirectionArrow->SetIsReplicated(true);
	AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("Audio Component"));
	AudioComponent->SetAutoActivate(true);
	PlatformModificator = CreateDefaultSubobject<UPlatformModificator>(TEXT("Platform Modificator"));
	BallsPoolComponent = CreateDefaultSubobject<UPongBallPool>(TEXT("BallsPoll"));
	BallsPoolComponent->SetIsReplicated(true);
	bReplicates=true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh>LoadedBodyMeshRoot(TEXT("/Script/Engine.StaticMesh'/ControlRig/Controls/ControlRig_Circle_1mm.ControlRig_Circle_1mm'"));
	
}

// Called when the game starts or when spawned
void APongPlatform::BeginPlay()
{
	Super::BeginPlay();
	PingPongGameState = Cast<APongGameState>(UGameplayStatics::GetGameState(GetWorld()));
	if (!PingPongGameState)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("APongPlatform::BeginPlay() - PingPongGameState is NULL"));
		FGenericPlatformMisc::RequestExit(false);
	}
	StartRotatePos = BodyMesh->GetComponentRotation().Yaw;
	SetReplicateMovement(true);
	InitialZ=GetActorLocation().Z;
	GameState = Cast<APongGameState>(UGameplayStatics::GetGameState(GetWorld()));
	check(GameState);
	FVector Origin,BoxExtended;
	GetActorBounds(false,Origin,BoxExtended,false);
	UKismetSystemLibrary::PrintText(GetWorld(),FText::AsNumber(BoxExtended.X));
	currentLocation = MeshRoot->GetComponentLocation();
	MoveSpeedMax=MoveSpeed*MoveSpeedMultiplier;
	MoveSpeedMin=MoveSpeed/MoveSpeedMultiplier;
}


// Called every frame
void APongPlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (HasAuthority())
	{
		TickMoveRight(DeltaTime);
		TickMoveForward(DeltaTime);
	}		
	//TODO FIX SOUND ON CLIENT SIDE
	SetSoundPitch(FMath::Max(FMath::Abs(targetForwardAxisValue),FMath::Abs(targetRightAxisValue))*5);
	//UE_LOG(LogTemp,Warning,TEXT("speed=%f"),FMath::Max(FMath::Abs(targetForwardAxisValue),FMath::Abs(targetRightAxisValue)))
}

bool APongPlatform::Server_Fire_Validate(EBallModificators Modificator)
{
	return true;
}

void APongPlatform::Server_GetRightValue_Implementation(float AxisValue)
{
	CurrentRightAxisValue=AxisValue;
}

bool APongPlatform::Server_GetRightValue_Validate(float AxisValue)
{
	return true;
}

void APongPlatform::Server_GetForwardValue_Implementation(float AxisValue)
{
	CurrentForwardAxisValue=AxisValue;
}

bool APongPlatform::Server_GetForwardValue_Validate(float AxisValue)
{
	return true;
}

void APongPlatform::TickMoveRight(float DeltaTime)
{
	if(bInvertedControl)
	{
		CurrentRightAxisValue=-CurrentRightAxisValue;
	}	
	targetRightAxisValue = FMath::Lerp(targetRightAxisValue,CurrentRightAxisValue,InterpolationKey);
	
	FVector nextLocation = GetActorLocation() + GetActorRightVector() * MoveSpeed * targetRightAxisValue*DeltaTime;		
	if(!SetActorLocation(nextLocation, true))
	{
		
	}
}

void APongPlatform::TickMoveForward(float DeltaTime)
{
	if(bInvertedControl)
	{
		CurrentForwardAxisValue=-CurrentForwardAxisValue;
	}	
	targetForwardAxisValue = FMath::Lerp(targetForwardAxisValue, CurrentForwardAxisValue, InterpolationKey);
    
	FVector nextLocation = GetActorLocation() + GetActorForwardVector() * MoveSpeed * targetForwardAxisValue * DeltaTime;
	SetActorLocation(nextLocation, true);
}

UPlatformModificator* APongPlatform::GetPlatformModificator() const
{
	return PlatformModificator;
}

void APongPlatform::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void APongPlatform::SetSpeedMultiplier(float Multiplier)
{	
	MoveSpeed*=Multiplier;
	MoveSpeed= FMath::Clamp(MoveSpeed,MoveSpeedMin,MoveSpeedMax);
}

bool APongPlatform::CheckScore(EBallModificators Modificator)
{
	APongPlayerState* PlayerState = nullptr;
	AController* Controller = GetOwner()->GetInstigatorController();
	if (Controller)
	{
		APongPlayerController* PlayerController = Cast<APongPlayerController>(Controller);
		if (PlayerController)
		{
			PlayerState = PlayerController->GetPlayerState<APongPlayerState>();
		}
	}	
	if (PlayerState)
	{
		if(PlayerState->GetScore()-GameState->GetShotCost(Modificator)>0)
		{
			PlayerState->SetScore(PlayerState->GetScore()-GameState->GetShotCost(Modificator));
		
			PingPongGameState->UpdatePlayersScore(PlayerState->GetPlayerId(),PlayerState->GetScore());
			PingPongGameState->AddMaxScore(PlayerState->GetScore());
			return true;
		}
	}	
	return false;
}


void APongPlatform::SetSoundPitch_Implementation(float pitch)
{
	AudioComponent->SetPitchMultiplier(pitch);
}

void APongPlatform::Floating_Implementation()
{
	if (!bFloating) return;
	FVector NewLocation = GetActorLocation();
	NewLocation.Z = InitialZ + Amplitude * FMath::Sin(Frequency * RunningTime);
	SetActorLocation(NewLocation);
	RunningTime += 0.01;
}

void APongPlatform::Server_Rotate_Implementation(float AxisValue)
{
	if(AxisValue!=0)
	{
		FRotator Rotator;
		Rotator.Yaw = BodyMesh->GetRelativeRotation().Yaw + AxisValue;
		Rotator.Pitch=0;
		Rotator.Roll=0;
		Rotator.Yaw =FMath::Clamp(Rotator.Yaw,-RotateAngle,RotateAngle);
		BodyMesh->SetRelativeRotation(Rotator);
	}
}

bool APongPlatform::Server_Rotate_Validate(float AxisValue)
{
	return true;
}

void APongPlatform::Server_Fire_Implementation(EBallModificators Modificator)
{
	if(!CheckScore(Modificator)) return; //enough points to shoot
	FTransform Transform;
	Transform.SetLocation(ShootDirectionArrow->GetComponentLocation());
	Transform.SetRotation(ShootDirectionArrow->GetComponentRotation().Quaternion());
	BallsPoolComponent->SpawnBallOnServer(this,GetOwner(),Transform,Modificator);
	//TODO why is it spawning when it should be already exist in pool?
}
