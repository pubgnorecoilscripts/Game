#include "PossessableComponent.h"
#include "ParasitePlayerState.h"
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
	DOREPLIFETIME(UPossessableComponent, PossessionEndTime);
}

float UPossessableComponent::GetMaxDuration(const AParasitePlayerState* ForPlayer) const
{
	switch (HostType)
	{
	case EHostType::NPC:
		return ParasiteRules::NPCDuration;
	case EHostType::Player:
		return ParasiteRules::PlayerDuration +
			((ForPlayer && ForPlayer->HasUpgrade(EParasiteUpgrade::Mimic)) ? ParasiteRules::MimicDurationBonus : 0.f);
	case EHostType::Vehicle:
	case EHostType::Prop:
	default:
		return ParasiteRules::PropDuration;
	}
}

bool UPossessableComponent::CanBePossessedBy(const AParasitePlayerState* ByPlayer) const
{
	if (bPossessed || !IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed())
	{
		return false;
	}
	if (!ByPlayer)
	{
		return false;
	}
	// A parasite may never possess a host ridden by a team mate, nor an ally player.
	if (Rider && Rider->GetTeam() == ByPlayer->GetTeam())
	{
		return false;
	}
	return true;
}

void UPossessableComponent::BeginPossession(AParasitePlayerState* ByPlayer)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}
	bPossessed = true;
	Rider = ByPlayer;
	const UWorld* World = GetWorld();
	PossessionEndTime = (World ? World->GetTimeSeconds() : 0.f) + GetMaxDuration(ByPlayer);
}

void UPossessableComponent::EndPossession()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}
	bPossessed = false;
	Rider = nullptr;
	PossessionEndTime = 0.f;
}

int32 UPossessableComponent::GetDNAReward() const
{
	switch (HostType)
	{
	case EHostType::Player:	return ParasiteRules::DNA_PossessEnemy;
	case EHostType::NPC:	return ParasiteRules::DNA_PossessHost * 2;
	default:				return ParasiteRules::DNA_PossessHost;
	}
}
