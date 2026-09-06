#include "PossessablePawn.h"
#include "PossessableComponent.h"
#include "ParasiteAudio.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Materials/Material.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"

namespace
{
	/** Cube is 100 uu on a side, so extent -> scale is a straight divide. */
	FVector ExtentToScale(const FVector& Extent)
	{
		return FVector(FMath::Max(Extent.X, 1.f), FMath::Max(Extent.Y, 1.f), FMath::Max(Extent.Z, 1.f)) / 50.f;
	}
}

APossessablePawn::APossessablePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	AutoPossessPlayer = EAutoReceiveInput::Disabled;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterial> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionObjectType(ECC_PhysicsBody);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	if (BasicMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, BasicMaterial.Object);
	}

	Detail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Detail"));
	Detail->SetupAttachment(Mesh);
	Detail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Detail->SetVisibility(false);
	if (CubeMesh.Succeeded())
	{
		Detail->SetStaticMesh(CubeMesh.Object);
	}
	if (BasicMaterial.Succeeded())
	{
		Detail->SetMaterial(0, BasicMaterial.Object);
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Mesh);
	CameraBoom->TargetArmLength = 350.f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->bInheritPitch = true;
	CameraBoom->bInheritYaw = true;
	CameraBoom->bInheritRoll = false;
	CameraBoom->SetUsingAbsoluteScale(true);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	Movement->UpdatedComponent = Mesh;
	Movement->MaxSpeed = 220.f;
	Movement->Acceleration = 900.f;
	Movement->Deceleration = 1600.f;

	Possessable = CreateDefaultSubobject<UPossessableComponent>(TEXT("Possessable"));
	Possessable->HostType = EHostType::Prop;
	Possessable->Mobility = EHostMobility::Slide;
}

void APossessablePawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APossessablePawn, VisualExtent);
	DOREPLIFETIME(APossessablePawn, VisualDetailOffset);
	DOREPLIFETIME(APossessablePawn, VisualDetailExtent);
	DOREPLIFETIME(APossessablePawn, VisualColour);
}

void APossessablePawn::BeginPlay()
{
	Super::BeginPlay();
	HingeClosedYaw = GetActorRotation().Yaw;
	ApplyVisuals();
}

void APossessablePawn::Configure(const FString& DisplayName, EHostType InHostType, EHostMobility InMobility,
	const FVector& BoxExtent, const FLinearColor& Colour, const FVector& DetailOffset, const FVector& DetailExtent)
{
	Possessable->HostDisplayName = DisplayName;
	Possessable->HostType = InHostType;
	Possessable->Mobility = InMobility;

	VisualExtent = BoxExtent;
	VisualColour = Colour;
	VisualDetailOffset = DetailOffset;
	VisualDetailExtent = DetailExtent;

	switch (InMobility)
	{
	case EHostMobility::Wheeled:
		Movement->MaxSpeed = (InHostType == EHostType::Vehicle) ? 700.f : 320.f;
		Possessable->CameraDistance = (InHostType == EHostType::Vehicle) ? 650.f : 320.f;
		break;
	case EHostMobility::Slide:
		Movement->MaxSpeed = 190.f;
		Possessable->CameraDistance = 300.f;
		break;
	case EHostMobility::Static:
	case EHostMobility::Hinge:
	default:
		Movement->MaxSpeed = 45.f;
		Possessable->CameraDistance = 280.f;
		break;
	}
	Possessable->CameraHeight = BoxExtent.Z + 40.f;
	ApplyVisuals();
}

void APossessablePawn::OnRep_Visual()
{
	ApplyVisuals();
}

void APossessablePawn::ApplyVisuals()
{
	if (!Mesh)
	{
		return;
	}
	Mesh->SetWorldScale3D(ExtentToScale(VisualExtent));

	if (!MeshMaterial && Mesh->GetMaterial(0))
	{
		MeshMaterial = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	}
	if (MeshMaterial)
	{
		MeshMaterial->SetVectorParameterValue(TEXT("Color"), VisualColour);
	}

	const bool bHasDetail = !VisualDetailExtent.IsNearlyZero();
	Detail->SetVisibility(bHasDetail);
	if (bHasDetail)
	{
		// Detail is a child of a scaled mesh, so undo the parent scale first.
		const FVector ParentScale = Mesh->GetComponentScale();
		Detail->SetWorldScale3D(ExtentToScale(VisualDetailExtent));
		Detail->SetRelativeLocation(FVector(
			VisualDetailOffset.X / FMath::Max(ParentScale.X, KINDA_SMALL_NUMBER),
			VisualDetailOffset.Y / FMath::Max(ParentScale.Y, KINDA_SMALL_NUMBER),
			VisualDetailOffset.Z / FMath::Max(ParentScale.Z, KINDA_SMALL_NUMBER)));

		if (!DetailMaterial && Detail->GetMaterial(0))
		{
			DetailMaterial = Detail->CreateAndSetMaterialInstanceDynamic(0);
		}
		if (DetailMaterial)
		{
			DetailMaterial->SetVectorParameterValue(TEXT("Color"), VisualColour * 0.7f);
		}
	}

	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = Possessable ? Possessable->CameraDistance : 350.f;
		CameraBoom->SetWorldLocation(GetActorLocation() + FVector(0.f, 0.f, VisualExtent.Z + 40.f));
	}
}

void APossessablePawn::DriveForward(float Value)
{
	if (FMath::IsNearlyZero(Value))
	{
		return;
	}
	switch (Possessable->Mobility)
	{
	case EHostMobility::Wheeled:
		// Wheeled hosts drive forwards along their own facing.
		AddMovementInput(GetActorForwardVector(), Value);
		break;
	case EHostMobility::Static:
	case EHostMobility::Hinge:
		// Only a nudge: vending machines shuffle, they do not sprint.
		AddMovementInput(GetActorForwardVector(), Value * 0.25f);
		break;
	default:
	{
		const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Value);
		break;
	}
	}
}

void APossessablePawn::DriveRight(float Value)
{
	if (FMath::IsNearlyZero(Value))
	{
		return;
	}
	switch (Possessable->Mobility)
	{
	case EHostMobility::Wheeled:
		// Steering rather than strafing.
		AddActorWorldRotation(FRotator(0.f, Value * 90.f * GetWorld()->GetDeltaSeconds(), 0.f));
		break;
	case EHostMobility::Static:
	case EHostMobility::Hinge:
		AddActorWorldRotation(FRotator(0.f, Value * 45.f * GetWorld()->GetDeltaSeconds(), 0.f));
		break;
	default:
	{
		const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Value);
		break;
	}
	}
}

void APossessablePawn::ToggleHinge()
{
	if (Possessable->Mobility != EHostMobility::Hinge)
	{
		return;
	}
	bHingeOpen = !bHingeOpen;
	FParasiteAudio::Play(this, EParasiteSound::UIClick, GetActorLocation());
}

void APossessablePawn::SetRevealed(float Seconds)
{
	const UWorld* World = GetWorld();
	RevealEndTime = (World ? World->GetTimeSeconds() : 0.f) + Seconds;
}

void APossessablePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	// Scan reveal tint.
	const bool bShouldReveal = Now < RevealEndTime;
	if (bShouldReveal != bRevealApplied && MeshMaterial)
	{
		bRevealApplied = bShouldReveal;
		MeshMaterial->SetVectorParameterValue(TEXT("Color"), bShouldReveal ? FLinearColor(1.f, 0.1f, 0.6f) : VisualColour);
	}

	// Hinge props swing towards their target angle instead of driving.
	if (Possessable && Possessable->Mobility == EHostMobility::Hinge)
	{
		const float Target = bHingeOpen ? 1.f : 0.f;
		if (!FMath::IsNearlyEqual(HingeAlpha, Target, 0.005f))
		{
			HingeAlpha = FMath::FInterpConstantTo(HingeAlpha, Target, DeltaSeconds, 2.5f);
			FRotator Rot = GetActorRotation();
			Rot.Yaw = HingeClosedYaw + HingeAlpha * 95.f;
			SetActorRotation(Rot);
		}
	}
	// Static hosts breathe a little so a possessed one is subtly readable.
	else if (Possessable && Possessable->Mobility == EHostMobility::Static && Possessable->bPossessed)
	{
		WobbleTime += DeltaSeconds;
		const float Wobble = FMath::Sin(WobbleTime * 6.f) * 1.5f;
		SetActorRotation(FRotator(0.f, GetActorRotation().Yaw, Wobble));
	}
}
