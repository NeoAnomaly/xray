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
			xr_vector<b_material>& Materials,
			xr_vector<b_BuildTexture>& Textures
		);
	private:
		struct GpuData;

		Calc::Device* m_Device;
		GpuData* m_GpuData;
		BVHBuilder* m_BvhBuilder;
	};
}