#include "ParasiteNest.h"
#include "ParasiteAudio.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"

AParasiteNest::AParasiteNest()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	InfectionZone = CreateDefaultSubobject<USphereComponent>(TEXT("InfectionZone"));
	SetRootComponent(InfectionZone);
	InfectionZone->SetSphereRadius(350.f);
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

	// Lumpy on purpose: it should read as something growing in a corner, not as
	// a glowing objective marker.
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

void AParasiteNest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	// Beats faster and brighter the closer it is to being taken.
	if (CoreLight)
	{
		const float Base = bUnderAttack ? 1600.f : 600.f;
		CoreLight->SetIntensity(Base + FMath::Sin(Now * (bUnderAttack ? 9.f : 2.f)) * 250.f);
	}
	if (Core)
	{
		Core->SetRelativeScale3D(FVector(1.1f, 1.f, 0.85f) * (1.f + FMath::Sin(Now * 1.6f) * 0.03f));
	}
	if (CoreMaterial)
	{
		// Sickens towards a hot pink as the infection climbs.
		CoreMaterial->SetVectorParameterValue(TEXT("Color"),
			FMath::Lerp(FLinearColor(0.35f, 0.08f, 0.42f), FLinearColor(1.f, 0.15f, 0.55f), Infection / 100.f));
	}

	if (bUnderAttack)
	{
		InfectSoundTimer -= DeltaSeconds;
		if (InfectSoundTimer <= 0.f)
		{
			InfectSoundTimer = 1.5f;
			FParasiteAudio::Play(this, EParasiteSound::NestInfect, GetActorLocation());
		}
	}
	else
	{
		InfectSoundTimer = 0.f;
	}
}
