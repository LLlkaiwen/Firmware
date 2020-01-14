#include "eso.h"

ESO::ESO()
	: _para_h(0.02f)
	, _para_b0(20.0f)
	, _para_beta1(50.0f)
	, _para_beta2(833.3)
	, _state_last_u(0.0f)
	, _state_leso_z1(0.0f)
	, _state_leso_z2(0.0f)
{

}
ESO::~ESO()
{

}
void ESO::setParameters(float para_h, float para_b0,  float para_beta1, float para_beta2)
{
	_para_h = para_h;
	_para_b0 = para_b0;
	_para_beta1 = para_beta1;
	_para_beta2 = para_beta2;
}
void ESO::setParameters(float para_h, float para_b0)
{
	setH(para_h);
	setB0(para_b0);
}
void ESO::setH(float para_h)
{
	_para_h = para_h;
	_para_beta1 =  1.0f/para_h;
	_para_beta2 = 1.0f / (3.0f*para_h*para_h);
}

void ESO::setB0(float para_b0)
{
	_para_b0 = para_b0;
}
float ESO::leso(const float &cur)
{
	float e =  _state_leso_z1 - cur;
	_state_leso_z1 +=  _para_h*( _state_leso_z2 +_para_b0*_state_last_u - _para_beta1*e);
	_state_leso_z2 +=  _para_h*(- _para_beta2*e);
	return _state_leso_z2/_para_b0;
}

void ESO::updateLastU(const float &u)
{
	_state_last_u = u;
}

void ESO::getState(float &z1, float &z2)
{
	z1 = _state_leso_z1;
	z2 = _state_leso_z2;
}
void ESO::resetLeso()
{
	_state_leso_z1 = 0.0f;
	_state_leso_z2 = 0.0f;
}

