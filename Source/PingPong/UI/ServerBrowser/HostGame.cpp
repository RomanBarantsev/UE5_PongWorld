// Fill out your copyright notice in the Description page of Project Settings.


#include "HostGame.h"

#include "MapButton.h"
#include "ServerBrowser.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/UniformGridPanel.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "PingPong/GameInstance/Pong_GameInstance.h"
#include "PingPong/UI/HUDs/GameHUD.h"

void UHostGame::MapChoosed(FString Name)
{
	MapChoosedName = Name;
}

void UHostGame::ExitButtonBtnClicked()
{
	this->SetVisibility(ESlateVisibility::Collapsed);
}

void UHostGame::NativeConstruct()
{
	HostServerBtn->OnClicked.AddDynamic(this,&UHostGame::HostServerBtnClicked);
	ExitButton->OnClicked.AddDynamic(this,&UHostGame::ExitButtonBtnClicked);
	auto Childrens = MapsGridPanel->GetAllChildren();
	for (const auto& Children : Childrens)
	{
		UMapButton* MapButton = Cast<UMapButton>(Children);
		if (MapButton)
		{
			MapButton->OnMapButtonClicked.AddDynamic(this,&ThisClass::MapChoosed);
		}
	}
	Super::NativeConstruct();
}

void UHostGame::HostServerBtnClicked()
{
	if (!bIsLocalServer->IsChecked())
	{
		auto GI=UGameplayStatics::GetGameInstance(GetWorld());	
		if (GI)
		{
			UPong_GameInstance* Pong_GameInstance = Cast<UPong_GameInstance>(GI);
			if (Pong_GameInstance)
			{
				auto PS = UGameplayStatics::GetPlayerState(GetWorld(),0);
				if (PS)
				{
					Pong_GameInstance->CreateHost(MapChoosedName,PS->GetPlayerName(),GetUniqueID());
					auto HUD = GetOwningPlayer()->GetHUD();
					ABaseHUD* BaseHUD = Cast<ABaseHUD>(HUD);
					if (BaseHUD)
					{
						BaseHUD->SwitchUI(Widgets::Loading);
					}
					this->SetVisibility(ESlateVisibility::Collapsed);
				}			
			}
		}
	}
	else
	{
		auto GI=UGameplayStatics::GetGameInstance(GetWorld());
		if (GI)
		{
			UPong_GameInstance* Pong_GameInstance = Cast<UPong_GameInstance>(GI);
			if (Pong_GameInstance)
			{
				auto PS = UGameplayStatics::GetPlayerState(GetWorld(),0);
				const FString ServerName = PS ? PS->GetPlayerName() : TEXT("PingPong Server");
				Pong_GameInstance->HostSteamSession(MapChoosedName, ServerName);
				this->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}
