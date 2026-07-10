// Fill out your copyright notice in the Description page of Project Settings.


#include "NetworkGameInstance.h"
#include "CreateSessionCallbackProxy.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Online/OnlineSessionNames.h"

bool UNetworkGameInstance::HostSession(TSharedPtr<const FUniqueNetId> UserId, FName SessionName, bool bIsLAN,
                                       bool bIsPresence, int32 MaxNumPlayers)
{
	// Get the Online Subsystem to work with
	IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::Get();	
	if (OnlineSub)
	{
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();

		if (Sessions.IsValid() && UserId.IsValid())
		{
			/* 
				Fill in all the Session Settings that we want to use.
				
				There are more with SessionSettings.Set(...);
				For example the Map or the GameMode/Type.
			*/
			SessionSettings = MakeShareable(new FOnlineSessionSettings());

			SessionSettings->bIsLANMatch = bIsLAN;
			SessionSettings->bUsesPresence = bIsPresence;
			SessionSettings->NumPublicConnections = MaxNumPlayers;
			SessionSettings->NumPrivateConnections = 0;
			SessionSettings->bAllowInvites = true;
			SessionSettings->bAllowJoinInProgress = true;
			SessionSettings->bShouldAdvertise = true;
			SessionSettings->bAllowJoinViaPresence = true;
			SessionSettings->bAllowJoinViaPresenceFriendsOnly = false;
			SessionSettings->bUseLobbiesIfAvailable = true;

			SessionSettings->Set(SETTING_MAPNAME, PendingMapName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
			SessionSettings->Set(FName(TEXT("SERVER_NAME")), PendingServerName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
			// Set the delegate to the Handle of the SessionInterface
			OnCreateSessionCompleteDelegateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegate);
			// Our delegate should get called when this is complete (doesn't need to be successful!)
			return Sessions->CreateSession(*UserId, SessionName, *SessionSettings);
		
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("No OnlineSubsytem found!"));
		}
	}
	return false;
}

bool UNetworkGameInstance::StartOnlineGame(const FString& MapName, const FString& ServerName, bool bIsLAN,
                                           bool bIsPresence, int32 MaxNumPlayers)
{
	ULocalPlayer* const Player = GetFirstGamePlayer();
	if (!Player)
	{
		return false;
	}

	PendingMapName = MapName.IsEmpty() ? TEXT("GameMap") : MapName;
	PendingServerName = ServerName.IsEmpty() ? TEXT("PingPong Server") : ServerName;

	DestroySessionAndLeaveGame();
	return HostSession(Player->GetPreferredUniqueNetId().GetUniqueNetId(), NAME_GameSession, bIsLAN, bIsPresence, MaxNumPlayers);
}

UNetworkGameInstance::UNetworkGameInstance(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	/** Bind function for CREATING a Session */
	OnCreateSessionCompleteDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(this, &UNetworkGameInstance::OnCreateSessionComplete);
	/** Bind function for STARTING a Session */
	OnStartSessionCompleteDelegate = FOnStartSessionCompleteDelegate::CreateUObject(this, &UNetworkGameInstance::OnStartOnlineGameComplete);
	/** Bind function for FINDING a Session */
	OnFindSessionsCompleteDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(this, &UNetworkGameInstance::OnFindSessionsComplete);
	/** Bind function for JOINING a Session */
	OnJoinSessionCompleteDelegate = FOnJoinSessionCompleteDelegate::CreateUObject(this, &UNetworkGameInstance::OnJoinSessionComplete);
	/** Bind function for DESTROYING a Session */
	OnDestroySessionCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &UNetworkGameInstance::OnDestroySessionComplete);
	
}

void UNetworkGameInstance::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
		if (Sessions.IsValid())
		{
			// Clear the SessionComplete delegate handle, since we finished this call
			Sessions->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegateHandle);
			if (bWasSuccessful)
			{
				// Set the StartSession delegate handle
				OnStartSessionCompleteDelegateHandle = Sessions->AddOnStartSessionCompleteDelegate_Handle(OnStartSessionCompleteDelegate);

				// Our StartSessionComplete delegate should get called after this
				Sessions->StartSession(SessionName);
			}
		}
	}
}

void UNetworkGameInstance::OnStartOnlineGameComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogTemp, Error, TEXT("OnStartSessionComplete %s, %d"), *SessionName.ToString(), bWasSuccessful);
	// Get the Online Subsystem so we can get the Session Interface
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		// Get the Session Interface to clear the Delegate
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
		if (Sessions.IsValid())
		{
			// Clear the delegate, since we are done with this call
			Sessions->ClearOnStartSessionCompleteDelegate_Handle(OnStartSessionCompleteDelegateHandle);
		}
	}

	// If the start was successful, we can open a NewMap if we want. Make sure to use "listen" as a parameter!
	if (bWasSuccessful)
	{
		UGameplayStatics::OpenLevel(GetWorld(), FName(*PendingMapName), true, "listen");
	}
}

void UNetworkGameInstance::FindSessions(TSharedPtr<const FUniqueNetId> UserId, bool bIsLAN, bool bIsPresence)
{
	// Get the OnlineSubsystem we want to work with
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
		if (Sessions.IsValid() && UserId.IsValid())
		{
			SessionSearch = MakeShareable(new FOnlineSessionSearch());

			SessionSearch->bIsLanQuery = bIsLAN;
			SessionSearch->MaxSearchResults = 20;
			SessionSearch->PingBucketSize = 50;
			// We only want to set this Query Setting if "bIsPresence" is true
			if (bIsPresence)
			{
				SessionSearch->QuerySettings.Set(SEARCH_PRESENCE, bIsPresence, EOnlineComparisonOp::Equals);
			}
			TSharedRef<FOnlineSessionSearch> SearchSettingsRef = SessionSearch.ToSharedRef();
			
			// Set the Delegate to the Delegate Handle of the FindSession function
			OnFindSessionsCompleteDelegateHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(OnFindSessionsCompleteDelegate);
			
			// Finally call the SessionInterface function. The Delegate gets called once this is finished
			Sessions->FindSessions(*UserId, SearchSettingsRef);			
		}
		else
		{
			// If something goes wrong, just call the Delegate Function directly with "false".
			OnFindSessionsComplete(false);
		}
	}
}

void UNetworkGameInstance::OnFindSessionsComplete(bool bWasSuccessful)
{
	UE_LOG(LogTemp, Error, TEXT("OFindSessionsComplete bSuccess: %d"), bWasSuccessful);
	IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
		if (Sessions.IsValid())
		{
			// Clear the Delegate handle, since we finished this call
			Sessions->ClearOnFindSessionsCompleteDelegate_Handle(OnFindSessionsCompleteDelegateHandle);
			if (!SessionSearch.IsValid())
			{
				return;
			}

			// Just debugging the Number of Search results. Can be displayed in UMG or something later on
			UE_LOG(LogTemp, Error, TEXT("Num Search Results: %d"), SessionSearch->SearchResults.Num());
		
			// If we have found at least 1 session, we just going to debug them. You could add them to a list of UMG Widgets, like it is done in the BP version!
			if (SessionSearch->SearchResults.Num() > 0)
			{
				// "SessionSearch->SearchResults" is an Array that contains all the information. You can access the Session in this and get a lot of information.
				// This can be customized later on with your own classes to add more information that can be set and displayed
				for (int32 SearchIdx = 0; SearchIdx < SessionSearch->SearchResults.Num(); SearchIdx++)
				{
					// OwningUserName is just the SessionName for now. I guess you can create your own Host Settings class and GameSession Class and add a proper GameServer Name here.
					// This is something you can't do in Blueprint for example!
					UE_LOG(LogTemp, Error, TEXT("Session Number: %d | Sessionname: %s "), SearchIdx+1, *(SessionSearch->SearchResults[SearchIdx].Session.OwningUserName));
				}
			}
		}
	}
}

bool UNetworkGameInstance::JoinFoundSession(TSharedPtr<const FUniqueNetId> UserId, FName SessionName,
	const FOnlineSessionSearchResult& SearchResult)
{
	// Return bool
	bool bSuccessful = false;

	// Get OnlineSubsystem we want to work with
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();

	if (OnlineSub)
	{
		// Get SessionInterface from the OnlineSubsystem
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();

		if (Sessions.IsValid() && UserId.IsValid())
		{
			// Set the Handle again
			OnJoinSessionCompleteDelegateHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegate);
			
			// Call the "JoinSession" Function with the passed "SearchResult". The "SessionSearch->SearchResults" can be used to get such a
			// "FOnlineSessionSearchResult" and pass it. Pretty straight forward!
			bSuccessful = Sessions->JoinSession(*UserId, SessionName, SearchResult);
		}
	}
		
	return bSuccessful;
}

void UNetworkGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	UE_LOG(LogTemp, Error, TEXT("OnJoinSessionComplete %s, %d"), *SessionName.ToString(), static_cast<int32>(Result));

	// Get the OnlineSubsystem we want to work with
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		// Get SessionInterface from the OnlineSubsystem
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();

		if (Sessions.IsValid())
		{
			// Clear the Delegate again
			Sessions->ClearOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegateHandle);

			// Get the first local PlayerController, so we can call "ClientTravel" to get to the Server Map
			// This is something the Blueprint Node "Join Session" does automatically!
			APlayerController * const PlayerController = GetFirstLocalPlayerController();

			// We need a FString to use ClientTravel and we can let the SessionInterface contruct such a
			// String for us by giving him the SessionName and an empty String. We want to do this, because
			// Every OnlineSubsystem uses different TravelURLs
			FString TravelURL;

			if (PlayerController && Sessions->GetResolvedConnectString(SessionName, TravelURL))
			{
				// Finally call the ClienTravel. If you want, you could print the TravelURL to see
				// how it really looks like
				PlayerController->ClientTravel(TravelURL, ETravelType::TRAVEL_Absolute);
			}
		}
	}
}

void UNetworkGameInstance::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogTemp, Error, TEXT("OnDestroySessionComplete %s, %d"), *SessionName.ToString(), bWasSuccessful);

	// Get the OnlineSubsystem we want to work with
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		// Get the SessionInterface from the OnlineSubsystem
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();

		if (Sessions.IsValid())
		{
			// Clear the Delegate
			Sessions->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegateHandle);

			// If it was successful, we just load another level (could be a MainMenu!)
			if (bWasSuccessful)
			{
				UGameplayStatics::OpenLevel(GetWorld(), "EntryMap", true);
			}
		}
	}
}

void UNetworkGameInstance::StartOnlineGame()
{
	StartOnlineGame(TEXT("GameMap"), TEXT("PingPong Server"), false, true, 4);
}

void UNetworkGameInstance::FindOnlineGames()
{
	FindOnlineGames(false, true);
}

void UNetworkGameInstance::FindOnlineGames(bool bIsLAN, bool bIsPresence)
{
	ULocalPlayer* const Player = GetFirstGamePlayer();
	if (!Player)
	{
		OnFindSessionsComplete(false);
		return;
	}

	FindSessions(Player->GetPreferredUniqueNetId().GetUniqueNetId(), bIsLAN, bIsPresence);
}

void UNetworkGameInstance::JoinOnlineGame()
{
	ULocalPlayer* const Player = GetFirstGamePlayer();
	UE_LOG(LogTemp, Error,TEXT("JoinOnlineGame"));
	// Just a SearchResult where we can save the one we want to use, for the case we find more than one!
	FOnlineSessionSearchResult SearchResult;

	// If the Array is not empty, we can go through it
	if (SessionSearch->SearchResults.Num() > 0)
	{
		for (int32 i = 0; i < SessionSearch->SearchResults.Num(); i++)
		{
			// To avoid something crazy, we filter sessions from ourself
			if (SessionSearch->SearchResults[i].Session.OwningUserId != Player->GetPreferredUniqueNetId())
			{
				SearchResult = SessionSearch->SearchResults[i];
				UE_LOG(LogTemp, Error, TEXT("OFindSessionsComplete bSuccess: %d"), SessionSearch->SearchResults[i].PingInMs);
				// Once we found sounce a Session that is not ours, just join it. Instead of using a for loop, you could
				// use a widget where you click on and have a reference for the GameSession it represents which you can use
				// here
				JoinFoundSession(Player->GetPreferredUniqueNetId().GetUniqueNetId(), NAME_GameSession, SearchResult);
				break;
			}
		}
	}	
}

bool UNetworkGameInstance::JoinOnlineGameByIndex(int32 SearchResultIndex)
{
	ULocalPlayer* const Player = GetFirstGamePlayer();
	if (!Player || !SessionSearch.IsValid() || !SessionSearch->SearchResults.IsValidIndex(SearchResultIndex))
	{
		return false;
	}

	return JoinFoundSession(Player->GetPreferredUniqueNetId().GetUniqueNetId(), NAME_GameSession,
		SessionSearch->SearchResults[SearchResultIndex]);
}

void UNetworkGameInstance::DestroySessionAndLeaveGame()
{
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();

		if (Sessions.IsValid())
		{
			Sessions->AddOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegate);

			Sessions->DestroySession(NAME_GameSession);
		}
	}
}
