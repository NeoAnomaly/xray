#include "stdafx.h"

#include "..\xrCDB.h"

#include "device.h"
#include "executable.h"
#include "event.h"

#include "BVHIntersector.h"

#include "PlainBVHTranslator.h"

namespace LightingCL
{
	struct BVHIntersector::GpuData
	{
		Calc::Device* Device;

		Calc::Buffer* BvhNodes;

		Calc::Buffer* Vertices;
		Calc::Buffer* Faces;

		Calc::Executable* Executable;
		Calc::Function* LightingFunc;

		GpuData(Calc::Device* d):
			Device(d),
			BvhNodes(nullptr),
			Vertices(nullptr),
			Faces(nullptr),

			Executable(nullptr),
			LightingFunc(nullptr)
		{
		}

		~GpuData()
		{
			if (BvhNodes)
				Device->DeleteBuffer(BvhNodes);

			if (Vertices)
				Device->DeleteBuffer(Vertices);

			if (Faces)
				Device->DeleteBuffer(Faces);

			if (LightingFunc)
				Executable->DeleteFunction(LightingFunc);

			if (Executable)
				Device->DeleteExecutable(Executable);
		}
	};

	BVHIntersector::BVHIntersector(Calc::Device* Device):
		m_Device(Device)
	{
		m_GpuData = new GpuData(Device);
		m_BvhBuilder = new BVHBuilder();

#if (LCL_EMBED_KERNELS != 1)
		const char* headers[] = { "CL/common.cl" };

		int numheaders = sizeof(headers) / sizeof(char*);

		m_GpuData->Executable = m_Device->CompileExecutable("CL/bvh.cl", headers, numheaders);

#else
		m_gpudata->executable = m_device->CompileExecutable(cl_bvh, std::strlen(cl_bvh), nullptr);
#endif

		m_GpuData->LightingFunc = m_GpuData->Executable->CreateFunction("LightingPoints");
	}

	BVHIntersector::~BVHIntersector()
	{
		if (m_BvhBuilder)
		{
			delete m_BvhBuilder;
			m_BvhBuilder = nullptr;
		}

		if (m_GpuData)
		{
			delete m_GpuData;
			m_GpuData = nullptr;
		}		
	}

	void BVHIntersector::BuildModel(
		CDB::CollectorPacked& Collector,
		xr_vector<b_material>& Materials,
		xr_vector<b_BuildTexture>& Textures
	)
	{
		Fvector* vertices = Collector.getV();
		CDB::TRI* faces = Collector.getT();
		size_t numVertices = Collector.getVS();
		size_t numFaces = Collector.getTS();

		xr_vector<Fbox> bounds(numFaces);

		for (size_t i = 0; i < numFaces; i++)
		{
			Fvector p1, p2, p3;

			p1 = vertices[faces[i].verts[0]];
			p2 = vertices[faces[i].verts[1]];
			p3 = vertices[faces[i].verts[2]];

			bounds[i].set(v3Min(p1, p2), v3Max(p1, p2));
			bounds[i].modify(p3);
		}

		m_BvhBuilder->Build(&bounds[0], numFaces);

		PlainBVHTranslator plainTranslator;
		plainTranslator.Process(m_BvhBuilder);

		///
		/// Load data to GPU
		///

		/// BVH Nodes
		m_GpuData->BvhNodes = m_Device->CreateBuffer(
			plainTranslator.GetNodes().size() * sizeof(PlainBVHTranslator::Node),
			Calc::BufferType::kRead,
			&*plainTranslator.GetNodes().begin()
		);

		/// Vertex
		m_GpuData->Vertices = m_Device->CreateBuffer(
			numVertices * (sizeof(Fvector) * 3),
			Calc::BufferType::kRead,
			vertices
		);

		/// Faces
		{
			struct Face
			{
				u32 Idx[3];
				u32 TextureId;
				Fvector2 UV;
				bool CastShadow;
			};

			m_GpuData->Faces = m_Device->CreateBuffer(
				numFaces * sizeof(Face),
				Calc::BufferType::kRead
			);

			Face* faceData = nullptr;
			Calc::Event* event = nullptr;

			m_Device->MapBuffer(
				m_GpuData->Faces,
				0,
				0,
				numFaces * sizeof(Face),
				Calc::BufferType::kWrite,
				(void**)&faceData,
				&event
			);

			event->Wait();
			m_Device->DeleteEvent(event);

			const size_t* faceIds = m_BvhBuilder->GetFaceIds();

			for (size_t i = 0; i < numFaces; i++)
			{
				size_t currentFaceIdx = faceIds[i];

				faceData[i].Idx[0] = faces[currentFaceIdx].verts[0];
				faceData[i].Idx[1] = faces[currentFaceIdx].verts[1];
				faceData[i].Idx[2] = faces[currentFaceIdx].verts[2];

				base_Face* face = (base_Face*)(*((void**)&faces[currentFaceIdx].dummy));
				R_ASSERT(face);

				Shader_xrLC& shader = face->Shader();

				faceData[i].TextureId = Materials[faces[currentFaceIdx].material].surfidx;
				faceData[i].CastShadow = shader.flags.bLIGHT_CastShadow;
				faceData[i].UV = *face->getTC0();
			}

			m_Device->UnmapBuffer(m_GpuData->Faces, 0, faceData, &event);

			event->Wait();
			m_Device->DeleteEvent(event);
		}

		m_Device->Finish(0);
	}
}