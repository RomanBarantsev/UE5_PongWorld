// Fill out your copyright notice in the Description page of Project Settings.


#include "ServerRow.h"

#include "Components/Button.h"
#include "Engine/Canvas.h"
#include "Kismet/GameplayStatics.h"
#include "PingPong/GameInstance/Pong_GameInstance.h"

void UServerRow::SetCurrentPlayers(int Players,int maxPlayers)
{
	FText text = (FText::FromString(FString::FromInt(Players)+"/"+FString::FromInt(maxPlayers)));
	PlayersTxt->SetText(text);
}

void UServerRow::SetServerName(FString Name)
{
	ServerNameTxt->SetText(FText::FromString(*Name));
}

void UServerRow::OnButtonClicked()
{
	if (bIsOnlineSession)
	{
		if (UPong_GameInstance* PongGameInstance = Cast<UPong_GameInstance>(UGameplayStatics::GetGameInstance(GetWorld())))
		{
			PongGameInstance->JoinSteamSession(OnlineSessionIndex);
		}
		return;
	}

	auto PC = GetOwningPlayer();
	if (PC)
	{
		const FString URL = FString::Printf(TEXT("%s:%d"), *IP, Port);
		PC->ClientTravel(URL,TRAVEL_Absolute);
	}
}

void UServerRow::NativeConstruct()
{
	Button->OnClicked.AddDynamic(this,&ThisClass::OnButtonClicked);
	Super::NativeConstruct();
}
