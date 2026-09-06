#include "PossessableComponent.h"
#include "ParasiteGameMode.h"
#include "ParasitePlayerState.h"
#include "ParasiteRevealable.h"
#include "Net/UnrealNetwork.h"

UPossessableComponent::UPossessableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UPossessableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UPossessableComponent, HostType);
	DOREPLIFETIME(UPossessableComponent, Mobility);
	DOREPLIFETIME(UPossessableComponent, HostDisplayName);
	DOREPLIFETIME(UPossessableComponent, CameraDistance);
	DOREPLIFETIME(UPossessableComponent, CameraHeight);
	DOREPLIFETIME(UPossessableComponent, bPossessed);
	DOREPLIFETIME(UPossessableComponent, Rider);
	DOREPLIFETIME(UPossessableComponent, PossessionTimeRemaining);
	DOREPLIFETIME(UPossessableComponent, bRevealed);
}

void UPossessableComponent::BeginPlay()
{
	Super::BeginPlay();

	// The server is the only machine that runs the simulation, so it is the only
	// one that registers hosts.
	if (GetOwnerRole() == ROLE_Authority && !bManagedExternally)
	{
		if (AParasiteGameMode* GameMode = AParasiteGameMode::Get(this))
		{
			GameMode->RegisterHost(this);
		}
	}
}

void UPossessableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwnerRole() == ROLE_Authority && !bManagedExternally && HostId != Parasite::InvalidHost)
	{
		if (AParasiteGameMode* GameMode = AParasiteGameMode::Get(this))
		{
			GameMode->UnregisterHost(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void UPossessableComponent::OnRep_Revealed()
{
	// The owning actor decides what "revealed" looks like.
	if (IParasiteRevealable* Revealable = Cast<IParasiteRevealable>(GetOwner()))
	{
		Revealable->OnRevealChanged(bRevealed);
	}
}
