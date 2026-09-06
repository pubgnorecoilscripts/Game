#include "ParasiteAudio.h"
#include "Sound/SoundWaveProcedural.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

namespace
{
	struct FToneSpec
	{
		float StartHz;
		float EndHz;
		float Duration;
		float Noise;		// 0..1 blend of white noise
		float Volume;
	};

	FToneSpec GetSpec(EParasiteSound Sound)
	{
		switch (Sound)
		{
		case EParasiteSound::Possess:		return { 220.f,  660.f, 0.35f, 0.10f, 0.55f };
		case EParasiteSound::PossessFail:	return { 300.f,  110.f, 0.25f, 0.15f, 0.45f };
		case EParasiteSound::PossessExit:	return { 640.f,  180.f, 0.30f, 0.10f, 0.50f };
		case EParasiteSound::ParasiteMove:	return { 900.f, 1100.f, 0.08f, 0.55f, 0.15f };
		case EParasiteSound::ScanPulse:		return { 1400.f, 300.f, 0.50f, 0.05f, 0.50f };
		case EParasiteSound::Detected:		return { 500.f,  900.f, 0.40f, 0.20f, 0.60f };
		case EParasiteSound::NestInfect:	return { 130.f,  190.f, 0.60f, 0.35f, 0.50f };
		case EParasiteSound::NestDamage:	return { 180.f,   90.f, 0.45f, 0.45f, 0.55f };
		case EParasiteSound::MatchStart:	return { 260.f,  780.f, 0.90f, 0.05f, 0.60f };
		case EParasiteSound::MatchEnd:		return { 700.f,  160.f, 1.20f, 0.05f, 0.60f };
		case EParasiteSound::UIClick:
		default:							return { 1200.f, 900.f, 0.06f, 0.05f, 0.35f };
		}
	}

	/** Builds a one-shot mono procedural wave from a tone spec. */
	USoundWaveProcedural* BuildWave(EParasiteSound Sound)
	{
		const FToneSpec Spec = GetSpec(Sound);
		constexpr int32 SampleRate = 22050;
		const int32 NumSamples = FMath::Max(64, FMath::RoundToInt(SampleRate * Spec.Duration));

		USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>();
		if (!Wave)
		{
			return nullptr;
		}
		Wave->SetSampleRate(SampleRate);
		Wave->NumChannels = 1;
		Wave->Duration = Spec.Duration;
		Wave->SoundGroup = SOUNDGROUP_Default;
		Wave->bLooping = false;

		TArray<uint8> PCM;
		PCM.SetNumUninitialized(NumSamples * sizeof(int16));
		int16* Samples = reinterpret_cast<int16*>(PCM.GetData());

		FRandomStream Random(FMath::Rand());
		float Phase = 0.f;
		for (int32 Index = 0; Index < NumSamples; ++Index)
		{
			const float Alpha = static_cast<float>(Index) / static_cast<float>(NumSamples);
			const float Hz = FMath::Lerp(Spec.StartHz, Spec.EndHz, Alpha);
			Phase += 2.f * PI * Hz / static_cast<float>(SampleRate);

			// Short attack, long decay: keeps clicks out of the buffer.
			const float Envelope = FMath::Min(1.f, Alpha * 25.f) * FMath::Pow(1.f - Alpha, 1.5f);
			const float Tone = FMath::Sin(Phase);
			const float Noise = Random.FRandRange(-1.f, 1.f);
			const float Value = FMath::Lerp(Tone, Noise, Spec.Noise) * Envelope * Spec.Volume;

			Samples[Index] = static_cast<int16>(FMath::Clamp(Value, -1.f, 1.f) * 32767.f);
		}

		Wave->QueueAudio(PCM.GetData(), PCM.Num());
		return Wave;
	}

	void StopLater(const UObject* WorldContext, UAudioComponent* Component, float Delay)
	{
		UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
		if (!World || !Component)
		{
			return;
		}
		TWeakObjectPtr<UAudioComponent> Weak(Component);
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak]()
		{
			if (Weak.IsValid())
			{
				Weak->Stop();
			}
		}), Delay + 0.15f, false);
	}
}

void FParasiteAudio::Play(const UObject* WorldContext, EParasiteSound Sound, const FVector& Location)
{
	if (!WorldContext || !GEngine || !GEngine->UseSound())
	{
		return;
	}
	USoundWaveProcedural* Wave = BuildWave(Sound);
	if (!Wave)
	{
		return;
	}
	UAudioComponent* Component = UGameplayStatics::SpawnSoundAtLocation(WorldContext, Wave, Location);
	StopLater(WorldContext, Component, GetSpec(Sound).Duration);
}

void FParasiteAudio::Play2D(const UObject* WorldContext, EParasiteSound Sound)
{
	if (!WorldContext || !GEngine || !GEngine->UseSound())
	{
		return;
	}
	USoundWaveProcedural* Wave = BuildWave(Sound);
	if (!Wave)
	{
		return;
	}
	UAudioComponent* Component = UGameplayStatics::SpawnSound2D(WorldContext, Wave);
	StopLater(WorldContext, Component, GetSpec(Sound).Duration);
}
