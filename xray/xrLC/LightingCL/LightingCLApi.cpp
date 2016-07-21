#include "stdafx.h"

#include "calc.h"
#include "device.h"

#include "..\xrFace.h"
#include "..\Build.h"

#include "LightingCLApi.h"

#include "BufferImpl.h"

namespace LightingCL
{
	static Calc::Calc* GetCalc()
	{
		static Calc::Calc* s_calc = nullptr;
		if (!s_calc)
		{
			s_calc = Calc::CreateCalc(0);
		}

		return s_calc;
	}

	u32 LightingCL::ILightingCLApi::GetDeviceCount()
	{
		return GetCalc()->GetDeviceCount();
	}

	void LightingCL::ILightingCLApi::GetDeviceInfo(u32 DeviceIdx, DeviceInfo & DeviceInfo)
	{
		Calc::DeviceSpec spec = { 0 };

		GetCalc()->GetDeviceSpec(DeviceIdx, spec);

		DeviceInfo.name = spec.name;
		DeviceInfo.type = spec.type == Calc::DeviceType::kGpu ? DeviceInfo::kGpu : DeviceInfo::kCpu;
		DeviceInfo.vendor = spec.vendor;
	}

	ILightingCLApi * LightingCL::ILightingCLApi::Create(u32 DeviceIdx)
	{
		return new LightingCLApi(GetCalc()->CreateDevice(DeviceIdx));
	}

	void LightingCL::ILightingCLApi::Delete(ILightingCLApi * Api)
	{
		delete Api;
	}

	LightingCLApi::LightingCLApi(Calc::Device* Device):
		m_Device(Device)
	{
		m_Intersector = new BVHIntersector(Device);
	}

	LightingCLApi::~LightingCLApi()
	{
		if (m_Intersector)
		{
			delete m_Intersector;
			m_Intersector = nullptr;
		}

		if (m_Device)
		{
			GetCalc()->DeleteDevice(m_Device);
			m_Device = nullptr;
		}		
	}

	void LightingCLApi::LoadTextures(xr_vector<b_BuildTexture>& Textures)
	{
		m_Intersector->LoadTextures(Textures, nullptr);
	}

	void LightingCLApi::BuildCollisionModel(CDB::CollectorPacked & Collector, xr_vector<b_material>& Materials)
	{
		m_Intersector->BuildModel(Collector, Materials);
	}

	Buffer * LightingCLApi::CreateBuffer(size_t Size, void * InitData) const
	{
		Calc::Buffer* buffer = nullptr;

		if (InitData)
		{
			buffer = m_Device->CreateBuffer(Size, Calc::BufferType::kWrite, InitData);
		}
		else
		{
			buffer = m_Device->CreateBuffer(Size, Calc::BufferType::kWrite);
		}

		return new BufferImpl(buffer);
	}

	void LightingCLApi::DeleteBuffer(Buffer * Buffer) const
	{
		m_Device->DeleteBuffer(static_cast<BufferImpl*>(Buffer)->GetData());
	}

	void LightingCLApi::MapBuffer(Buffer * Buffer, MapType Type, size_t Offset, size_t Size, void ** Data, Event ** Event) const
	{
		R_ASSERT(!"Not implemented");
	}

	void LightingCLApi::UnmapBuffer(Buffer * Buffer, void * Data, Event ** Event) const
	{
		R_ASSERT(!"Not implemented");
	}

	void LightingCLApi::DeleteEvent(Event * Event) const
	{
		R_ASSERT(!"Not implemented");
	}

	void LightingCLApi::LightingPoints(
		Buffer* Colors,
		Buffer* Points,
		Buffer* RgbLights,
		Buffer* SunLights,
		Buffer* HemiLights,
		u64 NumPoints,
		u32 NumRgbLights,
		u32 NumSunLights,
		u32 NumHemiLights,
		u32 Samples
	)
	{

	}

	void LightingCLApi::LightingPoints(
		base_color_c* Colors, 
		Point* Points, 
		base_lighting& Lights, 
		u64 NumPoints,
		u32 Flags, 
		u32 Samples
	)
	{
		Buffer* colors = CreateBuffer(NumPoints * sizeof(base_color_c), Colors);
		Buffer* points = CreateBuffer(NumPoints * sizeof(Point), Points);

		u32 numRgbLights = (Flags & LP_dont_rgb) ? 0 : Lights.rgb.size();
		u32 numSunLights = (Flags & LP_dont_sun) ? 0 : Lights.sun.size();
		u32 numHemiLights = (Flags & LP_dont_hemi) ? 0 : Lights.hemi.size();

		Buffer* rgbLights = (numRgbLights) ? CreateBuffer(numRgbLights * sizeof(R_Light), &Lights.rgb[0]) : nullptr;
		Buffer* sunLights = (numSunLights) ? CreateBuffer(numSunLights * sizeof(R_Light), &Lights.sun[0]) : nullptr;
		Buffer* hemiLights = (numHemiLights) ? CreateBuffer(numHemiLights * sizeof(R_Light), &Lights.hemi[0]) : nullptr;

		LightingPoints(
			colors,
			points,
			rgbLights,
			sunLights,
			hemiLights,
			NumPoints,
			numRgbLights,
			numSunLights,
			numHemiLights,
			Samples
		);

		DeleteBuffer(colors);
		DeleteBuffer(points);
		DeleteBuffer(rgbLights);
		DeleteBuffer(sunLights);
		DeleteBuffer(hemiLights);
	}
}