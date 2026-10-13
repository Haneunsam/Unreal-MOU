#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Voice/VoiceCaptureSource.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceInputGateTest, "MOU.Voice.InputSensitivity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// [MICTEST-010] 실제 PCM 프레이밍 경로로 작은 목소리, 무음, 임계값과 음량 곡선을 검증한다.
bool FVoiceInputGateTest::RunTest(const FString& Parameters)
{
	FVoiceCaptureSource Capture;
	auto Feed = [&](int16 Amplitude)
	{
		TArray<int16> Samples;
		Samples.Init(Amplitude, MOUVoice::SamplesPerFrame);
		Capture.PendingBytes.Append(reinterpret_cast<const uint8*>(Samples.GetData()), MOUVoice::BytesPerFrame);
		TArray<FMOUVoiceFrame> Frames;
		Capture.DrainPendingSamples(Frames);
		return Frames[0];
	};
	Capture.SetVadThreshold(0.f);
	TestFalse(TEXT("Zero threshold does not transmit digital silence"), Feed(0).bIsSpeaking);
	TestTrue(TEXT("Quiet PCM passes zero threshold"), Feed(8).bIsSpeaking);
	Capture.Shutdown();
	Capture.SetVadThreshold(.005f);
	const FMOUVoiceFrame Quiet = Feed(8);
	TestFalse(TEXT("Below-threshold input is blocked"), Quiet.bIsSpeaking);
	TestTrue(TEXT("Blocked input remains measurable by settings meter"), Capture.GetCurrentLoudness() > 0.f);
	TestTrue(TEXT("Above-threshold input passes"), Feed(328).bIsSpeaking);
	Capture.SilenceStartedAt = FPlatformTime::Seconds() - MOUVoice::VadHangoverSeconds - 1.;
	TestFalse(TEXT("Noise gate closes after short speech tail"), Feed(8).bIsSpeaking);
	for (float Value : { .01f, .25f, .5f, .75f, 1.f })
	{
		TestTrue(TEXT("Logarithmic threshold roundtrip"), FMath::IsNearlyEqual(MOUVoice::MicRmsToSlider(MOUVoice::MicSliderToRms(Value)), Value, .00001f));
	}
	TestEqual(TEXT("Left endpoint disables gate"), MOUVoice::MicSliderToRms(0.f), 0.f);
	TestEqual(TEXT("Below threshold has no normalized intensity"), MOUVoice::NormalizeLoudness(.001f, .005f), 0.f);
	TestTrue(TEXT("Louder speech increases transmitted intensity"), MOUVoice::NormalizeLoudness(.02f, .005f) < MOUVoice::NormalizeLoudness(.1f, .005f));
	TestEqual(TEXT("Proximity receive boost tripled"), MOUVoice::ProximityPlaybackGain, 6.f);
	TestEqual(TEXT("Radio receive boost doubled"), MOUVoice::RadioPlaybackGain, 5.f);
	const float Half = MOUVoice::NormalizeLoudnessWithCeiling(.03f, .01f, .05f);
	TestTrue(TEXT("Developer ceiling: halfway input is 50 percent"), FMath::IsNearlyEqual(Half, .5f));
	TestEqual(TEXT("Developer ceiling: full input is 100 percent"), MOUVoice::NormalizeLoudnessWithCeiling(.05f, .01f, .05f), 1.f);
	TestEqual(TEXT("At sensitivity threshold intensity is zero"), MOUVoice::NormalizeLoudnessWithCeiling(.01f, .01f, .05f), 0.f);
	TestTrue(TEXT("Lower ceiling increases noise radius for same voice"),
		MOUVoice::GetRadiusScaleFromNormalized(Half) > MOUVoice::GetRadiusScaleFromNormalized(MOUVoice::NormalizeLoudnessWithCeiling(.03f, .01f, .25f)));
	TestEqual(TEXT("High sensitivity does not invert range"), MOUVoice::NormalizeLoudnessWithCeiling(.04f, .06f, .05f), 0.f);
	TestTrue(TEXT("High sensitivity retains gradual range"), MOUVoice::NormalizeLoudnessWithCeiling(.062f, .06f, .05f) < 1.f);
	return true;
}
#endif
