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
		// ������������ ����� ILightingCLApi
		virtual void LoadTextures(xr_vector<b_BuildTexture>& Textures) override;
		virtual void LoadLights(base_lighting& Lights) override;
		virtual void BuildCollisionModel(CDB::CollectorPacked & Collector, xr_vector<b_material>& Materials) override;
		virtual Buffer * CreateBuffer(size_t Size, void * InitData) const override;
		virtual void DeleteBuffer(Buffer * Buffer) const override;
		virtual void MapBuffer(Buffer * Buffer, MapType Type, size_t Offset, size_t Size, void ** Data, Event ** Event) const override;
		virtual void UnmapBuffer(Buffer * Buffer, void * Data, Event ** Event) const override;
		virtual void DeleteEvent(Event * Event) const override;

		virtual void LightingPoints(
			base_color_c* Colors, 
			Point* Points,
			u64 NumPoints,
			u32 Flags, 
			u32 Samples
		) override;
	protected:
		friend ILightingCLApi* ILightingCLApi::Create(u32 DeviceIdx);
		LightingCLApi(Calc::Device* Device);
		~LightingCLApi();
	private:
		void LightingPoints(
			Buffer* Colors,
			Buffer* Points,
			u64 NumPoints,
			u32 Flags,
			u32 Samples
		);

		Calc::Device* m_Device;
		BVHIntersector* m_Intersector;
	};
}