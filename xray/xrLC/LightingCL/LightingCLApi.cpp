#include "stdafx.h"

#include "calc.h"
#include "device.h"

#include "..\Build.h"

#include "LightingCLApi.h"

#include "BufferImpl.h"
#include "Math\float2.h"
#include "Math\float3.h"

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

	void LightingCLApi::LoadLights(base_lighting& Lights)
	{
		m_Intersector->LoadLights(Lights);
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
		u64 NumPoints,
		u32 Flags,
		u32 Samples
	)
	{
		m_Intersector->LightingPoints(
			static_cast<BufferImpl*>(Colors)->GetData(),
			static_cast<BufferImpl*>(Points)->GetData(),
			NumPoints,
			Flags,
			Samples
		);
	}

	void LightingCLApi::LightingPoints(
		base_color_c* Colors, 
		Point* Points,
		u64 NumPoints,
		u32 Flags, 
		u32 Samples
	)
	{
		Buffer* colors = CreateBuffer(NumPoints * sizeof(base_color_c), Colors);
		Buffer* points = CreateBuffer(NumPoints * sizeof(Point), Points);

		LightingPoints(
			colors,
			points,
			NumPoints,
			Flags,
			Samples
		);

		DeleteBuffer(colors);
		DeleteBuffer(points);
	}
}