#include "ParasitePlayerState.h"
#include "Net/UnrealNetwork.h"

AParasitePlayerState::AParasitePlayerState()
{
}

void AParasitePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AParasitePlayerState, Team);
	DOREPLIFETIME(AParasitePlayerState, DNA);
	DOREPLIFETIME(AParasitePlayerState, LifetimeDNA);
	DOREPLIFETIME(AParasitePlayerState, Upgrades);
	DOREPLIFETIME(AParasitePlayerState, PossessAvailableTime);
	DOREPLIFETIME(AParasitePlayerState, ScanAvailableTime);
	DOREPLIFETIME(AParasitePlayerState, LeapAvailableTime);
	DOREPLIFETIME(AParasitePlayerState, CurrentHost);
	DOREPLIFETIME(AParasitePlayerState, InfectionTicks);
}

void AParasitePlayerState::CopyProperties(APlayerState* NewPlayerState)
{
	Super::CopyProperties(NewPlayerState);

	// Keeps team + DNA across seamless travel / rematch reconnects.
	if (AParasitePlayerState* New = Cast<AParasitePlayerState>(NewPlayerState))
	{
		New->Team = Team;
		New->DNA = DNA;
		New->LifetimeDNA = LifetimeDNA;
		New->Upgrades = Upgrades;
	}
}

EParasiteTeam AParasitePlayerState::GetEnemyTeam() const
{
	switch (Team)
	{
	case EParasiteTeam::TeamA: return EParasiteTeam::TeamB;
	case EParasiteTeam::TeamB: return EParasiteTeam::TeamA;
	default: return EParasiteTeam::None;
	}
}

float AParasitePlayerState::GetPossessRange() const
{
	return ParasiteRules::BasePossessRange + (HasUpgrade(EParasiteUpgrade::Jumper) ? ParasiteRules::JumperRangeBonus : 0.f);
}

float AParasitePlayerState::GetStealthScale() const
{
	return HasUpgrade(EParasiteUpgrade::Infiltrator) ? ParasiteRules::InfiltratorScanScale : 1.f;
}

bool AParasitePlayerState::IsPossessCooldownReady() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() >= PossessAvailableTime;
}

bool AParasitePlayerState::IsScanReady() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() >= ScanAvailableTime;
}

bool AParasitePlayerState::IsLeapReady() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() >= LeapAvailableTime;
}

void AParasitePlayerState::AwardDNA(int32 Amount)
{
	if (!HasAuthority() || Amount <= 0)
	{
		return;
	}
	DNA += Amount;
	LifetimeDNA += Amount;
}

bool AParasitePlayerState::TryPurchaseUpgrade(EParasiteUpgrade Upgrade)
{
	if (!HasAuthority() || Upgrade == EParasiteUpgrade::None)
	{
		return false;
	}
	if (Upgrades.Num() >= ParasiteRules::MaxUpgrades || HasUpgrade(Upgrade) || DNA < ParasiteRules::UpgradeCost)
	{
		return false;
	}
	DNA -= ParasiteRules::UpgradeCost;
	Upgrades.Add(Upgrade);
	return true;
}

void AParasitePlayerState::StartPossessCooldown()
{
	if (const UWorld* World = GetWorld())
	{
		PossessAvailableTime = World->GetTimeSeconds() + ParasiteRules::PossessCooldown;
	}
}

void AParasitePlayerState::StartScanCooldown()
{
	if (const UWorld* World = GetWorld())
	{
		ScanAvailableTime = World->GetTimeSeconds() + ParasiteRules::ScanCooldown;
	}
}

void AParasitePlayerState::StartLeapCooldown()
{
	if (const UWorld* World = GetWorld())
	{
		LeapAvailableTime = World->GetTimeSeconds() + ParasiteRules::LeapCooldown;
	}
}

void AParasitePlayerState::ResetForNewMatch()
{
	DNA = 0;
	LifetimeDNA = 0;
	Upgrades.Reset();
	PossessAvailableTime = 0.f;
	ScanAvailableTime = 0.f;
	LeapAvailableTime = 0.f;
	CurrentHost = nullptr;
	InfectionTicks = 0;
}
