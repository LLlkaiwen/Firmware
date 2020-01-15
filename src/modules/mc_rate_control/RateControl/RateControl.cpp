/****************************************************************************
 *
 *   Copyright (c) 2019 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file RateControl.cpp
 */

#include <RateControl.hpp>
#include <px4_platform_common/defines.h>

using namespace matrix;

void RateControl::setGains(const Vector3f &P, const Vector3f &I, const Vector3f &D)
{
	_gain_p = P;
	_gain_i = I;
	_gain_d = D;
}

void RateControl::setDTermCutoff(const float loop_rate, const float cutoff, const bool force)
{
	// only do expensive filter update if the cutoff changed
	if (force || fabsf(_lp_filters_d.get_cutoff_freq() - cutoff) > 0.01f) {
		_lp_filters_d.set_cutoff_frequency(loop_rate, cutoff);
		_lp_filters_d.reset(_rate_prev);
	}
}

void RateControl::setSaturationStatus(const MultirotorMixer::saturation_status &status)
{
	_mixer_saturation_positive[0] = status.flags.roll_pos;
	_mixer_saturation_positive[1] = status.flags.pitch_pos;
	_mixer_saturation_positive[2] = status.flags.yaw_pos;
	_mixer_saturation_negative[0] = status.flags.roll_neg;
	_mixer_saturation_negative[1] = status.flags.pitch_neg;
	_mixer_saturation_negative[2] = status.flags.yaw_neg;
}

Vector3f RateControl::update(const Vector3f &rate, const Vector3f &rate_sp, const float dt, const bool landed)
{
	// angular rates error
	Vector3f rate_error = rate_sp - rate;

	// prepare D-term based on low-pass filtered rates
	const Vector3f rate_filtered(_lp_filters_d.apply(rate));
	Vector3f rate_d;

	if (dt > FLT_EPSILON) {
		rate_d = (rate_filtered - _rate_prev_filtered) / dt;
	}
	_eso_rollspeed.setH(dt);
	_eso_pitchspeed.setH(dt);
	_eso_yawspeed.setH(dt);
	Vector3f disturbance = Vector3f(0.5f*_eso_rollspeed.leso(rate(0))
									, 0.5f*_eso_pitchspeed.leso(rate(1))
									, 0.5f*_eso_yawspeed.leso(rate(2)));
	if(landed)//未起飞/着陆时不补偿扰动
	{
		disturbance = Vector3f(0.0f,0.0f,0.0f);
	}
	// PID control with feed forward
	const Vector3f torque = _gain_p.emult(rate_error) + _rate_int - _gain_d.emult(rate_d) + _gain_ff.emult(rate_sp) - disturbance;

	_eso_rollspeed.updateLastU(torque(0));
	_eso_pitchspeed.updateLastU(torque(1));
	_eso_yawspeed.updateLastU(torque(2));

	_rate_prev = rate;
	_rate_prev_filtered = rate_filtered;

	// update integral only if we are not landed
	if (!landed) {
		updateIntegral(rate_error, dt);
	}

	return torque;
}

void RateControl::updateIntegral(Vector3f &rate_error, const float dt)
{
	for (int i = 0; i < 3; i++) {
		// prevent further positive control saturation
		if (_mixer_saturation_positive[i]) {
			rate_error(i) = math::min(rate_error(i), 0.f);
		}

		// prevent further negative control saturation
		if (_mixer_saturation_negative[i]) {
			rate_error(i) = math::max(rate_error(i), 0.f);
		}

		// I term factor: reduce the I gain with increasing rate error.
		// This counteracts a non-linear effect where the integral builds up quickly upon a large setpoint
		// change (noticeable in a bounce-back effect after a flip).
		// The formula leads to a gradual decrease w/o steps, while only affecting the cases where it should:
		// with the parameter set to 400 degrees, up to 100 deg rate error, i_factor is almost 1 (having no effect),
		// and up to 200 deg error leads to <25% reduction of I.
		float i_factor = rate_error(i) / math::radians(400.f);
		i_factor = math::max(0.0f, 1.f - i_factor * i_factor);

		// Perform the integration using a first order method
		float rate_i = _rate_int(i) + i_factor * _gain_i(i) * rate_error(i) * dt;

		// do not propagate the result if out of range or invalid
		if (PX4_ISFINITE(rate_i)) {
			_rate_int(i) = math::constrain(rate_i, -_lim_int(i), _lim_int(i));
		}
	}
}

void RateControl::getRateControlStatus(rate_ctrl_status_s &rate_ctrl_status)
{
	rate_ctrl_status.rollspeed_integ = _rate_int(0);
	rate_ctrl_status.pitchspeed_integ = _rate_int(1);
	rate_ctrl_status.yawspeed_integ = _rate_int(2);
}

void RateControl::setEsoRateParam(const Vector3f & para_h,const Vector3f & para_b0)
{
	_eso_rollspeed.setH(para_h(0));
	_eso_pitchspeed.setH(para_h(1));
	_eso_yawspeed.setH(para_h(2));
	PX4_INFO("set rate eso h: %f, %f, %f", (double)para_h(0), (double)para_h(1), (double)para_h(2));
	_eso_rollspeed.setB0(para_b0(0));
	_eso_pitchspeed.setB0(para_b0(1));
	_eso_yawspeed.setB0(para_b0(2));
	PX4_INFO("set rate eso b0: %f, %f, %f", (double)para_b0(0), (double)para_b0(1), (double)para_b0(2));
}

void RateControl::getRateEsoState(Vector3f &rate_eso_z1, Vector3f &rate_eso_z2)
{
	_eso_rollspeed.getState(rate_eso_z1(0), rate_eso_z2(0));
	_eso_pitchspeed.getState(rate_eso_z1(1), rate_eso_z2(1));
	_eso_yawspeed.getState(rate_eso_z1(2), rate_eso_z2(2));
}