#pragma once

#include "calc.h"

#include "BVHBuilder.h"

namespace LightingCL
{
	class BVHIntersector
	{
	public:
		BVHIntersector(Calc::Device* Device);
		~BVHIntersector();

		void BuildModel(
			CDB::CollectorPacked& Collector,
			xr_vector<b_material>& Materials
		);

		void LoadLights(base_lighting& Lights);
		void LoadTextures(xr_vector<b_BuildTexture>& Textures, Event** Event);

		void LightingPoints(
			Calc::Buffer* Colors,
			Calc::Buffer* Points,
			u64 NumPoints,
			u32 Flags,
			u32 Samples
		);
	private:
		struct GpuData;

		///
		/// Intersector internal state flags
		enum State
		{
			sNone = 1 << 0,
			sTexturesLoaded = 1 << 1,
			sCollisionModelLoaded = 1 << 2,
			sLightsLoaded = 1 << 3,

			sReady = sTexturesLoaded | sLightsLoaded | sCollisionModelLoaded
		};

		void DebugDataTest();

		Calc::Device* m_Device;
		GpuData* m_GpuData;
		BVHBuilder* m_BvhBuilder;
		u32 m_State;
	};
}