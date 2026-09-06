#include "ParasiteNest.h"
#include "ParasitePlayerState.h"
#include "ParasiteGameState.h"
#include "ParasiteAudio.h"
#include "PossessableComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/Material.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

AParasiteNest::AParasiteNest()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	InfectionZone = CreateDefaultSubobject<USphereComponent>(TEXT("InfectionZone"));
	SetRootComponent(InfectionZone);
	InfectionZone->SetSphereRadius(ParasiteRules::NestRadius);
	InfectionZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterial> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	auto MakeBlob = [&](const TCHAR* Name, const FVector& Offset, const FVector& Scale) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Blob = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Blob->SetupAttachment(InfectionZone);
		Blob->SetRelativeLocation(Offset);
		Blob->SetRelativeScale3D(Scale);
		Blob->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (SphereMesh.Succeeded())
		{
			Blob->SetStaticMesh(SphereMesh.Object);
		}
		if (BasicMaterial.Succeeded())
		{
			Blob->SetMaterial(0, BasicMaterial.Object);
		}
		return Blob;
	};

	// Deliberately lumpy: it should read as something growing in a corner, not a beacon.
	Core = MakeBlob(TEXT("Core"), FVector(0.f, 0.f, -60.f), FVector(1.1f, 1.f, 0.85f));
	Growth1 = MakeBlob(TEXT("Growth1"), FVector(70.f, 35.f, -85.f), FVector(0.6f, 0.55f, 0.4f));
	Growth2 = MakeBlob(TEXT("Growth2"), FVector(-55.f, -60.f, -80.f), FVector(0.45f, 0.7f, 0.5f));

	CoreLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("CoreLight"));
	CoreLight->SetupAttachment(Core);
	CoreLight->SetIntensity(600.f);
	CoreLight->SetAttenuationRadius(400.f);
	CoreLight->SetCastShadows(false);
	CoreLight->SetLightColor(FLinearColor(0.5f, 0.15f, 0.6f));
}

void AParasiteNest::BeginPlay()
{
	Super::BeginPlay();
	if (Core && Core->GetMaterial(0))
	{
		CoreMaterial = Core->CreateAndSetMaterialInstanceDynamic(0);
		CoreMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.35f, 0.08f, 0.42f));
	}
}

void AParasiteNest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AParasiteNest, OwningTeam);
	DOREPLIFETIME(AParasiteNest, Infection);
	DOREPLIFETIME(AParasiteNest, Health);
	DOREPLIFETIME(AParasiteNest, bUnderAttack);
}

void AParasiteNest::GatherAttackers(TArray<AParasitePlayerState*>& OutAttackers) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float RadiusSq = FMath::Square(ParasiteRules::NestRadius);

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		AParasitePlayerState* PS = PC->GetPlayerState<AParasitePlayerState>();
		if (!PS || PS->GetTeam() == EParasiteTeam::None || PS->GetTeam() == OwningTeam)
		{
			continue;
		}
		// The player's presence is wherever their body actually is: their host if
		// they are riding one, otherwise their own pawn.
		const AActor* Body = PS->CurrentHost ? PS->CurrentHost.Get() : Cast<AActor>(PC->GetPawn());
		if (Body && FVector::DistSquared(Body->GetActorLocation(), GetActorLocation()) <= RadiusSq)
		{
			OutAttackers.Add(PS);
		}
	}
}

void AParasiteNest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		TArray<AParasitePlayerState*> Attackers;
		GatherAttackers(Attackers);

		const bool bWasUnderAttack = bUnderAttack;
		bUnderAttack = Attackers.Num() > 0;

		if (bUnderAttack && Infection < 100.f)
		{
			// One attacker infects at the base rate; extra attackers help, but with
			// diminishing returns so a 5-stack cannot burst the nest down.
			const float Rate = (100.f / ParasiteRules::NestInfectSeconds) * (1.f + 0.25f * (Attackers.Num() - 1));
			Infection = FMath::Clamp(Infection + Rate * DeltaSeconds, 0.f, 100.f);
			Health = FMath::Clamp(100.f - Infection, 0.f, 100.f);

			DNATimer += DeltaSeconds;
			if (DNATimer >= 3.f)
			{
				DNATimer = 0.f;
				for (AParasitePlayerState* PS : Attackers)
				{
					PS->AwardDNA(ParasiteRules::DNA_NestTick);
					PS->InfectionTicks++;
				}
			}

			SoundTimer += DeltaSeconds;
			if (SoundTimer >= 1.5f)
			{
				SoundTimer = 0.f;
				FParasiteAudio::Play(this, EParasiteSound::NestInfect, GetActorLocation());
			}
		}
		else if (!bUnderAttack)
		{
			DNATimer = 0.f;
			SoundTimer = 0.f;
		}

		if (bWasUnderAttack != bUnderAttack)
		{
			FParasiteAudio::Play(this, bUnderAttack ? EParasiteSound::NestDamage : EParasiteSound::UIClick, GetActorLocation());
		}

		// Defensive pulse: periodically reveals attackers to everyone nearby.
		PulseTimer += DeltaSeconds;
		if (PulseTimer >= ParasiteRules::NestPulseInterval)
		{
			PulseTimer = 0.f;
			if (bUnderAttack)
			{
				for (AParasitePlayerState* PS : Attackers)
				{
					if (AActor* Host = PS->CurrentHost.Get())
					{
						if (UPossessableComponent* Comp = Host->FindComponentByClass<UPossessableComponent>())
						{
							// Shorten the intruder's stay: the nest fights back.
							Comp->PossessionEndTime = FMath::Min(Comp->PossessionEndTime, GetWorld()->GetTimeSeconds() + 5.f);
						}
					}
				}
				FParasiteAudio::Play(this, EParasiteSound::NestDamage, GetActorLocation());
			}
		}

		// Publish to the replicated match state.
		if (AParasiteGameState* GS = GetWorld()->GetGameState<AParasiteGameState>())
		{
			if (OwningTeam == EParasiteTeam::TeamA)
			{
				GS->NestAInfection = Infection;
			}
			else
			{
				GS->NestBInfection = Infection;
			}
		}
	}

	// Cosmetic pulse, everywhere.
	if (CoreLight)
	{
		const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		const float Base = bUnderAttack ? 1600.f : 600.f;
		CoreLight->SetIntensity(Base + FMath::Sin(Time * (bUnderAttack ? 9.f : 2.f)) * 250.f);
	}
	if (Core)
	{
		const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		const float Breathe = 1.f + FMath::Sin(Time * 1.6f) * 0.03f;
		Core->SetRelativeScale3D(FVector(1.1f, 1.f, 0.85f) * Breathe);
	}
}

void AParasiteNest::ApplyExpulsion()
{
	if (!HasAuthority())
	{
		return;
	}
	Infection = FMath::Max(0.f, Infection - ParasiteRules::NestResetOnExpel);
	Health = FMath::Clamp(100.f - Infection, 0.f, 100.f);
	FParasiteAudio::Play(this, EParasiteSound::NestDamage, GetActorLocation());
}
