// Fill out your copyright notice in the Description page of Project Settings.


#include "ServerBrowser.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "PingPong/GameInstance/Pong_GameInstance.h"

void UServerBrowser::OnConnectPressed()
{
	
}

void UServerBrowser::OnRefreshPressed()
{
	if (IsLocalCheckBox && IsLocalCheckBox->IsChecked())
	{
		PongGameInstance->GetSteamServersList();
	}
	else
	{
		PongGameInstance->GetServersList();
	}
}

void UServerBrowser::OnBackPressed()
{
	this->SetVisibility(ESlateVisibility::Collapsed);
}

void UServerBrowser::OnServerListReady(const TArray<FServerInfo>& Servers)
{
	ServerList->ClearChildren();
	for (const auto& Server : Servers)
	{
		UServerRow* RowWidger = CreateWidget<UServerRow>(this,ServerRowSubClass);
		RowWidger->IP = Server.IP;
		RowWidger->Port = Server.Port;
		RowWidger->bIsOnlineSession = Server.bIsOnlineSession;
		RowWidger->OnlineSessionIndex = Server.OnlineSessionIndex;
		RowWidger->SetServerName(Server.Name);
		RowWidger->SetCurrentPlayers(Server.CurrentPlayers,Server.MaxPlayers);
		ServerList->AddChild(RowWidger);
	}
}

void UServerBrowser::VisibilityChanged(ESlateVisibility InVisibility)
{
	if (ESlateVisibility::Visible == InVisibility)
	{
		OnRefreshPressed();
	}
}

void UServerBrowser::NativeConstruct()
{
	Super::NativeConstruct();
	auto GI = GetGameInstance();
	check(GI);
	PongGameInstance = Cast<UPong_GameInstance>(GI);
	check(PongGameInstance);
	PongGameInstance->OnServerListReady.BindDynamic(this, &UServerBrowser::OnServerListReady);
	RefreshBtn->OnClicked.AddDynamic(this,&UServerBrowser::OnRefreshPressed);
	if (ConnectBtn)
	{
		ConnectBtn->OnClicked.AddDynamic(this,&UServerBrowser::OnConnectPressed);
	}
	BackButton->OnClicked.AddDynamic(this,&UServerBrowser::OnBackPressed);
	OnNativeVisibilityChanged.Add(FNativeOnVisibilityChangedEvent::FDelegate::CreateUObject(this, &UServerBrowser::VisibilityChanged));
}
