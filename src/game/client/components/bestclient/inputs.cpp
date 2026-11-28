/* Copyright © 2026 BestProject Team */
#include "inputs.h"

#include <base/math.h>
#include <engine/shared/config.h>

#include <algorithm>
#include <cmath>

namespace
{
	// State of the modes that follow ping or speed. It is only touched by BcInputs::Update.
	int s_SheylunTicks = 0;
	float s_AutoTicks = -1.0f;
	float s_MotionFactor = 0.0f;

	float FastModeOffsetTicks()
	{
		if(g_Config.m_BcInputs != BC_INPUTS_FAST || g_Config.m_TcFastInputAmount <= 0)
			return 0.0f;
		return g_Config.m_TcFastInputAmount / 20.0f;
	}

	float BestModeOffsetTicks()
	{
		if(g_Config.m_BcInputs != BC_INPUTS_BEST || g_Config.m_BcBestInputAmount <= 0)
			return 0.0f;

		float Offset = g_Config.m_BcBestInputAmount / 100.0f;

		if(g_Config.m_BcBestInputSmoothing > 0)
			Offset *= 1.0f - (g_Config.m_BcBestInputSmoothing / 200.0f);

		if(g_Config.m_BcBestInputLatencyComp > 0)
			Offset *= 1.0f + (g_Config.m_BcBestInputLatencyComp / 100.0f);

		return Offset;
	}

	float SaikoModeOffsetTicks()
	{
		if(g_Config.m_BcInputs != BC_INPUTS_SAIKO || g_Config.m_BcSaikoInputAmount <= 0)
			return 0.0f;
		return g_Config.m_BcSaikoInputAmount / 100.0f;
	}

	float DeltaModeOffsetTicks()
	{
		if(g_Config.m_BcInputs != BC_INPUTS_DELTA || g_Config.m_BcDeltaInputAmount <= 0)
			return 0.0f;
		return g_Config.m_BcDeltaInputAmount / 100.0f;
	}

	float FModeOffsetTicks()
	{
		if(g_Config.m_BcInputs != BC_INPUTS_F || g_Config.m_BcFInputAmount <= 0)
			return 0.0f;
		return g_Config.m_BcFInputAmount / 1000.0f;
	}

	float SheylunModeOffsetTicks()
	{
		if(g_Config.m_BcInputs != BC_INPUTS_SHEYLUN)
			return 0.0f;
		// Whole ticks only: as many as your ping needs, never fewer than one.
		return (float)std::clamp(s_SheylunTicks, 1, std::max(1, g_Config.m_BcSheylunMaxTicks));
	}

	float AutoModeOffsetTicks()
	{
		if(g_Config.m_BcInputs != BC_INPUTS_AUTO || s_AutoTicks <= 0.0f)
			return 0.0f;
		return s_AutoTicks;
	}

	float MotionModeOffsetTicks()
	{
		if(g_Config.m_BcInputs != BC_INPUTS_MOTION || g_Config.m_BcMotionInputAmount <= 0)
			return 0.0f;
		return g_Config.m_BcMotionInputAmount / 100.0f * s_MotionFactor;
	}
} // namespace

void BcInputs::Update(int PredictionTimeMs, float SpeedTilesPerSecond, float FrameTime)
{
	const int Mode = g_Config.m_BcInputs;
	const float Frames = std::clamp(FrameTime, 0.0f, 0.25f) * 60.0f; // how many 60 Hz frames passed

	if(Mode != BC_INPUTS_SHEYLUN)
		s_SheylunTicks = 0;
	if(Mode != BC_INPUTS_AUTO)
		s_AutoTicks = -1.0f;
	if(Mode != BC_INPUTS_MOTION)
		s_MotionFactor = 0.0f;

	if(Mode == BC_INPUTS_SHEYLUN)
	{
		// Predict whole ticks, as many as your ping needs. The value only jumps once the ping has really moved,
		// so the picture does not flicker between two neighbouring tick counts.
		const float DelayTicks = PredictionTimeMs / 20.0f * g_Config.m_BcSheylunStrength / 100.0f;
		if(s_SheylunTicks <= 0 || absolute(DelayTicks - s_SheylunTicks) > 0.8f)
			s_SheylunTicks = round_to_int(DelayTicks);
	}
	else if(Mode == BC_INPUTS_AUTO)
	{
		const float MaxTicks = g_Config.m_BcAutoInputMax / 100.0f;
		const float Target = std::clamp(PredictionTimeMs / 20.0f * g_Config.m_BcAutoInputStrength / 100.0f, 0.0f, MaxTicks);
		if(s_AutoTicks < 0.0f)
			s_AutoTicks = Target;
		else
		{
			const float Keep = std::pow(std::clamp(g_Config.m_BcAutoInputSmoothing / 100.0f, 0.0f, 0.95f), Frames);
			s_AutoTicks = Target + (s_AutoTicks - Target) * Keep;
		}
	}
	else if(Mode == BC_INPUTS_MOTION)
	{
		const float Linear = std::clamp(SpeedTilesPerSecond / std::max(1, g_Config.m_BcMotionInputSpeed), 0.0f, 1.0f);
		const float Target = Linear * Linear * (3.0f - 2.0f * Linear);
		const float Keep = std::pow(0.8f, Frames);
		s_MotionFactor = Target + (s_MotionFactor - Target) * Keep;
	}
}

float BcInputs::EffectiveOffsetTicks()
{
	switch(g_Config.m_BcInputs)
	{
	case BC_INPUTS_FAST: return FastModeOffsetTicks();
	case BC_INPUTS_BEST: return BestModeOffsetTicks();
	case BC_INPUTS_SAIKO: return SaikoModeOffsetTicks();
	case BC_INPUTS_DELTA: return DeltaModeOffsetTicks();
	case BC_INPUTS_F: return FModeOffsetTicks();
	case BC_INPUTS_SHEYLUN: return SheylunModeOffsetTicks();
	case BC_INPUTS_AUTO: return AutoModeOffsetTicks();
	case BC_INPUTS_MOTION: return MotionModeOffsetTicks();
	default: return 0.0f;
	}
}

int BcInputs::PredictionTicks(float OffsetTicks)
{
	if(OffsetTicks <= 0.0f)
		return 0;
	return (int)std::ceil(OffsetTicks);
}

int BcInputs::PredictionTicksOthers(float OffsetTicks)
{
	return PredictionTicks(OffsetTicks);
}

void BcInputs::ApplyOffset(float OffsetTicks, int &Tick, float &Intra)
{
	if(OffsetTicks <= 0.0f)
		return;

	const int WholeTicks = (int)OffsetTicks;
	const float OffsetIntra = OffsetTicks - WholeTicks;

	const float CombinedIntra = Intra + OffsetIntra;
	const int CarryOverTicks = (int)CombinedIntra;

	Tick += WholeTicks + CarryOverTicks;
	Intra = CombinedIntra - CarryOverTicks;
}

float BcInputs::BestInterpolationAmount(float Fraction, float DeltaLength, bool Enable)
{
	if(!Enable)
		return Fraction;

	const float T = std::clamp(Fraction, 0.0f, 1.0f);
	const float T2 = T * T;
	const float CubicT = 3.0f * T2 - 2.0f * T2 * T;
	switch(std::clamp(g_Config.m_BcBestInputInterpolation, 1, 3))
	{
	case 2:
		return CubicT;
	case 3:
		return mix(T, CubicT, std::clamp(DeltaLength / 1000.0f, 0.0f, 1.0f));
	default:
		return T;
	}
}

vec2 BcInputs::BestInterpolate(vec2 PrevPos, vec2 CurPos, float Fraction, bool Enable)
{
	return mix(PrevPos, CurPos, BestInterpolationAmount(Fraction, length(CurPos - PrevPos), Enable));
}

float BcInputs::BestInterpolate(float PrevPos, float CurPos, float Fraction, bool Enable)
{
	return mix(PrevPos, CurPos, BestInterpolationAmount(Fraction, absolute(CurPos - PrevPos), Enable));
}

bool BcInputs::FastOthers()
{
	return g_Config.m_BcInputs == BC_INPUTS_FAST && g_Config.m_TcFastInputOthers != 0;
}

bool BcInputs::BestOthers()
{
	return g_Config.m_BcInputs == BC_INPUTS_BEST && g_Config.m_BcBestInputOthers != 0;
}

bool BcInputs::SaikoOthers()
{
	return g_Config.m_BcInputs == BC_INPUTS_SAIKO && g_Config.m_BcSaikoInputOthers != 0;
}

bool BcInputs::DeltaOthers()
{
	return g_Config.m_BcInputs == BC_INPUTS_DELTA && g_Config.m_BcDeltaInputOthers != 0;
}

bool BcInputs::FOthers()
{
	return g_Config.m_BcInputs == BC_INPUTS_F && g_Config.m_BcFInputOthers != 0;
}

bool BcInputs::CloudOthers()
{
	return g_Config.m_BcInputs == BC_INPUTS_CLOUD && g_Config.m_BcCloudInputOthers != 0;
}

bool BcInputs::SheylunOthers()
{
	return g_Config.m_BcInputs == BC_INPUTS_SHEYLUN && g_Config.m_BcSheylunOthers != 0;
}

bool BcInputs::AutoOthers()
{
	return g_Config.m_BcInputs == BC_INPUTS_AUTO && g_Config.m_BcAutoInputOthers != 0;
}

bool BcInputs::MotionOthers()
{
	return g_Config.m_BcInputs == BC_INPUTS_MOTION && g_Config.m_BcMotionInputOthers != 0;
}

bool BcInputs::AnyOthers()
{
	return FastOthers() || BestOthers() || SaikoOthers() || DeltaOthers() || FOthers() || CloudOthers() || SheylunOthers() || AutoOthers() || MotionOthers();
}

bool BcInputs::ImmediateOthers()
{
	return BestOthers() || SaikoOthers() || DeltaOthers();
}
