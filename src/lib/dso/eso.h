#ifndef ESO_H_
#define ESO_H_
// #include<float.h>
// #include<math.h>
class ESO
{
public:
	ESO();
	~ESO();
	void setParameters(float para_h, float para_b0,  float para_beta1, float para_beta2);
	void setParameters(float para_h, float para_b0);
	void setH(float para_h);
	void setB0(float para_b0);
	float leso(const float &cur);
	void updateLastU(const float &u);
	void getState(float &z1, float &z2);
	void resetLeso();
private:
	/*parameters*/
	float _para_h;
	float _para_b0;
	float _para_beta1;
	float _para_beta2;
	float _state_last_u;
	/*state*/
	float _state_leso_z1;
	float _state_leso_z2;
};

#endif
