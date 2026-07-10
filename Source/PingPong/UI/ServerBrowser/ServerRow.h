// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "ServerRow.generated.h"

class UButton;
/**
 * 
 */
UCLASS()
class PINGPONG_API UServerRow : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(meta=(BindWidget))
	UButton* Button;
	UPROPERTY(meta=(BindWidget))
	UTextBlock* ServerNameTxt;
	UPROPERTY(meta=(BindWidget))
	UTextBlock* PlayersTxt;
	UPROPERTY()
	FString IP;
	UPROPERTY()
	int Port;
	UPROPERTY()
	bool bIsOnlineSession = false;
	UPROPERTY()
	int32 OnlineSessionIndex = INDEX_NONE;
	void SetCurrentPlayers(int Players,int maxPlayers);
	void SetServerName(FString Name);
	UFUNCTION()
	void OnButtonClicked();
	virtual void NativeConstruct() override;
};
