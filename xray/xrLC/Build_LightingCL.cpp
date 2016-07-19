#include "stdafx.h"

#include "LightingCL\ILightingCLApi.h"

void CBuild::InitLightingCL()
{
	m_LightingCLApi = LightingCL::ILightingCLApi::Create(0);
}