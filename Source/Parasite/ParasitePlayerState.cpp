#include "ParasitePlayerState.h"
#include "Net/UnrealNetwork.h"

void AParasitePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AParasitePlayerState, Team);
	DOREPLIFETIME(AParasitePlayerState, DNA);
	DOREPLIFETIME(AParasitePlayerState, LifetimeDNA);
	DOREPLIFETIME(AParasitePlayerState, InfectionTicks);
	DOREPLIFETIME(AParasitePlayerState, Upgrades);
	DOREPLIFETIME(AParasitePlayerState, NumUpgrades);
	DOREPLIFETIME(AParasitePlayerState, PossessCooldownRemaining);
	DOREPLIFETIME(AParasitePlayerState, ScanCooldownRemaining);
	DOREPLIFETIME(AParasitePlayerState, CurrentHost);
	DOREPLIFETIME(AParasitePlayerState, CurrentHostName);
	DOREPLIFETIME(AParasitePlayerState, HostTimeRemaining);
}

void AParasitePlayerState::CopyProperties(APlayerState* NewPlayerState)
{
	Super::CopyProperties(NewPlayerState);

	// Keeps the team and the scoreboard intact across a travel or reconnect.
	if (AParasitePlayerState* New = Cast<AParasitePlayerState>(NewPlayerState))
	{
		New->Team = Team;
		New->DNA = DNA;
		New->LifetimeDNA = LifetimeDNA;
		New->InfectionTicks = InfectionTicks;
		New->Upgrades = Upgrades;
		New->NumUpgrades = NumUpgrades;
	}
}
