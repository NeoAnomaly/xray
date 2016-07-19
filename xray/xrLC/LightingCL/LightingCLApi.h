#pragma once

#include "ILightingCLApi.h"
#include "BVHIntersector.h"

/// forward decl
namespace Calc
{
	class Device;
}

namespace LightingCL
{
	class LightingCLApi :
		public ILightingCLApi
	{
	public:
		// Унаследовано через ILightingCLApi
		virtual void BuildCollisionModel(CDB::CollectorPacked & Collector, xr_vector<b_material>& Materials, xr_vector<b_BuildTexture>& Textures) override;
		virtual Buffer * CreateBuffer(size_t Size, void * InitData) const override;
		virtual void DeleteBuffer(Buffer * Buffer) const override;
		virtual void MapBuffer(Buffer * Buffer, MapType Type, size_t Offset, size_t Size, void ** Data, Event ** Event) const override;
		virtual void UnmapBuffer(Buffer * Buffer, void * Data, Event ** Event) const override;
		virtual void DeleteEvent(Event * Event) const override;
		virtual void LightingPoints(Buffer * Colors, Buffer * Points, Buffer * Lights, u32 Flags) override;
	protected:
		friend ILightingCLApi* ILightingCLApi::Create(u32 DeviceIdx);
		LightingCLApi(Calc::Device* Device);
		~LightingCLApi();
	private:
		Calc::Device* m_Device;
		BVHIntersector* m_Intersector;
	};
}