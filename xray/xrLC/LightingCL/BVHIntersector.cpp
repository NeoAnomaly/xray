#include "stdafx.h"

#include "..\xrCDB.h"

#include "device.h"
#include "executable.h"
#include "event.h"

#include "BVHIntersector.h"

#include "PlainBVHTranslator.h"

static int const kWorkGroupSize = 64;

namespace LightingCL
{
	struct BVHIntersector::GpuData
	{
		Calc::Device* Device;

		Calc::Buffer* BvhNodes;

		Calc::Buffer* Vertices;
		Calc::Buffer* Faces;

		/// u64[i] = start offset of the i(mapped to face texture id) texture data in TexturesData buffer
		Calc::Buffer* TexturesOffset;
		Calc::Buffer* TexturesData;

		Calc::Executable* Executable;
		Calc::Function* LightingFunc;

		GpuData(Calc::Device* d):
			Device(d),
			BvhNodes(nullptr),
			Vertices(nullptr),
			Faces(nullptr),
			TexturesOffset(nullptr),
			TexturesData(nullptr),

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

			if (TexturesOffset)
				Device->DeleteBuffer(TexturesOffset);

			if (TexturesData)
				Device->DeleteBuffer(TexturesData);

			if (LightingFunc)
				Executable->DeleteFunction(LightingFunc);

			if (Executable)
				Device->DeleteExecutable(Executable);
		}
	};

	BVHIntersector::BVHIntersector(Calc::Device* Device):
		m_Device(Device),
		m_State(0)
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

		m_State |= sNone;
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

	void BVHIntersector::LoadTextures(xr_vector<b_BuildTexture>& Textures, Event ** Event)
	{
		/*m_GpuData->Textures = m_Device->CreateBuffer(
			Textures.size()
		);*/
		
		size_t totalTexturesSize = 0;
		size_t numTextures = Textures.size();
		xr_vector<u64> textureOffsets(numTextures);

		for (size_t i = 0; i < numTextures; i++)
		{
			b_BuildTexture& texture = Textures[i];
			if (!texture.pSurface)
			{
				continue;
			}

			/// save data offsets
			textureOffsets[i] = totalTexturesSize;

			totalTexturesSize += Textures[i].dwWidth * Textures[i].dwHeight * 4;
		}

//#pragma message("Add code for checking next statement: totalTexturesSize < Avaiable GPU memory")

		m_GpuData->TexturesOffset = m_Device->CreateBuffer(
			numTextures * sizeof(u64),
			Calc::BufferType::kRead,
			&textureOffsets[0]
		);

		///
		/// load textures data
		///
		Calc::Event* event = nullptr;
		u32* surfaceData = nullptr;

		m_GpuData->TexturesData = m_Device->CreateBuffer(totalTexturesSize, Calc::BufferType::kRead);
		
		m_Device->MapBuffer(
			m_GpuData->TexturesData,
			0,
			0,
			totalTexturesSize,
			Calc::BufferType::kWrite,
			(void**)&surfaceData,
			&event
		);

		event->Wait();
		m_Device->DeleteEvent(event);

		for (size_t i = 0; i < numTextures; i++)
		{
			b_BuildTexture& texture = Textures[i];
			if (!texture.pSurface)
			{
				continue;
			}

			RtlCopyMemory(surfaceData, texture.pSurface, texture.dwWidth * texture.dwHeight * 4);
		}		

		m_Device->UnmapBuffer(m_GpuData->TexturesData, 0, surfaceData, &event);

		event->Wait();
		m_Device->DeleteEvent(event);

		m_Device->Finish(0);

		m_State |= sTexturesLoaded;
	}

	void BVHIntersector::BuildModel(
		CDB::CollectorPacked& Collector,
		xr_vector<b_material>& Materials
	)
	{
		R_ASSERT2((m_State & sTexturesLoaded), "Textures must be loaded before building collision model");

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
				u32 TextureId;		/// mapped to m_GpuData->Texture
				Fvector2 UV;
				bool CastShadow;
				bool Opaque;
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

				faceData[i].CastShadow = shader.flags.bLIGHT_CastShadow;
				faceData[i].Opaque = face->flags.bOpaque;
				faceData[i].TextureId = Materials[face->dwMaterial].surfidx;
				faceData[i].UV = *face->getTC0();
			}

			m_Device->UnmapBuffer(m_GpuData->Faces, 0, faceData, &event);

			event->Wait();
			m_Device->DeleteEvent(event);
		}

		m_Device->Finish(0);

		m_State |= sCollisionModelLoaded;
	}

	void BVHIntersector::LightingPoints(
		Calc::Buffer * Colors, 
		Calc::Buffer * Points, 
		Calc::Buffer * RgbLights, 
		Calc::Buffer * SunLights, 
		Calc::Buffer * HemiLights, 
		u64 NumPoints, 
		u32 NumRgbLights, 
		u32 NumSunLights, 
		u32 NumHemiLights, 
		u32 Samples
	)
	{
		R_ASSERT2((m_State & sReady), "Intersector doesn't ready yet");

		auto& func = m_GpuData->LightingFunc;

		// Set args
		int arg = 0;

		func->SetArg(arg++, m_GpuData->BvhNodes);
		func->SetArg(arg++, m_GpuData->Vertices);
		func->SetArg(arg++, m_GpuData->Faces);
		func->SetArg(arg++, m_GpuData->TexturesOffset);
		func->SetArg(arg++, m_GpuData->TexturesData);
		func->SetArg(arg++, Colors);
		func->SetArg(arg++, Points);
		func->SetArg(arg++, RgbLights);
		func->SetArg(arg++, SunLights);
		func->SetArg(arg++, HemiLights);
		func->SetArg(arg++, sizeof(NumPoints), &NumPoints);
		func->SetArg(arg++, sizeof(NumRgbLights), &NumRgbLights);
		func->SetArg(arg++, sizeof(NumSunLights), &NumSunLights);
		func->SetArg(arg++, sizeof(NumHemiLights), &NumHemiLights);
		func->SetArg(arg++, sizeof(Samples), &Samples);

		size_t localsize = kWorkGroupSize;
		size_t globalsize = ((NumPoints + kWorkGroupSize - 1) / kWorkGroupSize) * kWorkGroupSize;

		m_Device->Execute(func, 0, globalsize, localsize, nullptr);
	}
}