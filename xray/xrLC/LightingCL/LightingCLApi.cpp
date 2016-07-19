#include "stdafx.h"
#include "LightingCLApi.h"

#include "calc.h"
#include "device.h"

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

	void LightingCLApi::BuildCollisionModel(CDB::CollectorPacked & Collector, xr_vector<b_material>& Materials, xr_vector<b_BuildTexture>& Textures)
	{
		m_Intersector->BuildModel(Collector, Materials, Textures);
	}

	Buffer * LightingCLApi::CreateBuffer(size_t Size, void * InitData) const
	{
		return nullptr;
	}
	void LightingCLApi::DeleteBuffer(Buffer * Buffer) const
	{
	}
	void LightingCLApi::MapBuffer(Buffer * Buffer, MapType Type, size_t Offset, size_t Size, void ** Data, Event ** Event) const
	{
	}
	void LightingCLApi::UnmapBuffer(Buffer * Buffer, void * Data, Event ** Event) const
	{
	}
	void LightingCLApi::DeleteEvent(Event * Event) const
	{
	}
	void LightingCLApi::LightingPoints(Buffer * Colors, Buffer * Points, Buffer * Lights, u32 Flags)
	{
	}
}