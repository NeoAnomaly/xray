#include "stdafx.h"

#include "..\xrCDB.h"
#include "..\xrFace.h"

#include "device.h"
#include "executable.h"
#include "event.h"
#include "except.h"

#include "Math\float2.h"
#include "Math\float3.h"
#include "Math\bbox.h"

#include "BVHIntersector.h"

#include "PlainBVHTranslator.h"

static int const kWorkGroupSize = 64;

namespace LightingCL
{
	struct LightCL
	{
		u8 Type;				// Type of light source		
		float3 Diffuse;			// Diffuse color of light	
		float3 Position;		// Position in world space	
		float3 Direction;		// Direction in world space	
		float2 Range;			// (Cutoff range; Cutoff range ^ 2)
		float3 Attenuation;		// (Constant attenuation; Linear attenuation; Quadratic attenuation)
		float Energy;			// For radiosity ONLY
	};

	struct BVHIntersector::GpuData
	{
		Calc::Device* Device;

		Calc::Buffer* BvhNodes;

		Calc::Buffer* Vertices;
		Calc::Buffer* Faces;

		/// TextureOffsets -> u64[i] = start offset of the i(face->textureId) texture data in TexturesData buffer
		Calc::Buffer* TextureOffsets;
		Calc::Buffer* TexturesData;

		/// Plain lights buffer, i.e. 
		/// rgb1, rgb2...rgbNumRgbLights, sun1, sun2...sunNumSunLights, hemi1, hemi2...hemiNumHemiLights
		Calc::Buffer* Lights;

		u32 NumRgbLights;
		u32 NumSunLights;
		u32 NumHemiLights;

		Calc::Executable* Executable;
		Calc::Function* LightingFunc;

		GpuData(Calc::Device* d):
			Device(d),
			BvhNodes(nullptr),
			Vertices(nullptr),
			Faces(nullptr),
			TextureOffsets(nullptr),
			TexturesData(nullptr),

			Lights(nullptr),
			NumRgbLights(0),
			NumSunLights(0),
			NumHemiLights(0),

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

			if (TextureOffsets)
				Device->DeleteBuffer(TextureOffsets);

			if (TexturesData)
				Device->DeleteBuffer(TexturesData);

			if (Lights)
				Device->DeleteBuffer(Lights);

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
		string_path clCommonFilepath, clBvh;

		FS.update_path(clCommonFilepath, "$cl_kernels$", "common.cl");
		FS.update_path(clBvh, "$cl_kernels$", "bvh.cl");

		const char* clHeaders[] = { clCommonFilepath };

		int numheaders = sizeof(clHeaders) / sizeof(char*);

		try
		{
			m_GpuData->Executable = m_Device->CompileExecutable(clBvh, clHeaders, numheaders);
		}
		catch (const Calc::Exception& e)
		{
			Msg("!ERROR: Exception has occured at compiling CL kernel(%s). Reason:\n%s", clBvh, e.what());
			Debug.do_exit("LightingCL fatal break");
		}		

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

		m_GpuData->TextureOffsets = m_Device->CreateBuffer(
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

	IC void CollectLights(u32& Counter, xr_vector<LightCL>& Store, xr_vector<R_Light>& Source)
	{
		for each (const R_Light& xrLigth in Source)
		{
			Store[Counter].Type = xrLigth.type;

			Store[Counter].Diffuse.x = xrLigth.diffuse.x;
			Store[Counter].Diffuse.y = xrLigth.diffuse.y;
			Store[Counter].Diffuse.z = xrLigth.diffuse.z;

			Store[Counter].Position.x = xrLigth.position.x;
			Store[Counter].Position.y = xrLigth.position.y;
			Store[Counter].Position.z = xrLigth.position.z;

			Store[Counter].Direction.x = xrLigth.direction.x;
			Store[Counter].Direction.y = xrLigth.direction.y;
			Store[Counter].Direction.z = xrLigth.direction.z;

			Store[Counter].Range.x = xrLigth.range;
			Store[Counter].Range.y = xrLigth.range2;

			Store[Counter].Attenuation.x = xrLigth.attenuation0;
			Store[Counter].Attenuation.y = xrLigth.attenuation1;
			Store[Counter].Attenuation.z = xrLigth.attenuation2;

			Counter++;
		}
	}

	void BVHIntersector::LoadLights(base_lighting& Lights)
	{
		m_GpuData->NumRgbLights = Lights.rgb.size();
		m_GpuData->NumSunLights = Lights.sun.size();
		m_GpuData->NumHemiLights = Lights.hemi.size();

		u32 totalLights = m_GpuData->NumRgbLights + m_GpuData->NumSunLights + m_GpuData->NumHemiLights;

		R_ASSERT2(totalLights > 0, "We want to perform lighting, exactly?");		

		u32 idx = 0;
		xr_vector<LightCL> lights(totalLights);

		CollectLights(idx, lights, Lights.rgb);
		CollectLights(idx, lights, Lights.sun);
		CollectLights(idx, lights, Lights.hemi);

		m_GpuData->Lights = m_Device->CreateBuffer(
			totalLights * sizeof(LightCL), 
			Calc::BufferType::kRead,
			&lights[0]
		);

		m_State |= sLightsLoaded;
	}

	void BVHIntersector::BuildModel(
		CDB::CollectorPacked& Collector,
		xr_vector<b_material>& Materials
	)
	{
		R_ASSERT2((m_State & sTexturesLoaded), "Textures must be loaded before building collision model");
		//m_State &= ~sCollisionModelLoaded;
		R_ASSERT2(!(m_State & sCollisionModelLoaded), "Not supported(not implemented) operation at now");

		Calc::Event* event = nullptr;

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
		{
			m_GpuData->Vertices = m_Device->CreateBuffer(numVertices * (sizeof(float3) * 3),	Calc::BufferType::kRead);

			float3* clVertices = nullptr;

			m_Device->MapBuffer(
				m_GpuData->Vertices,
				0,
				0,
				numVertices * (sizeof(float3) * 3),
				Calc::BufferType::kWrite,
				(void**)&clVertices,
				&event
			);

			event->Wait();
			m_Device->DeleteEvent(event);

			for (size_t i = 0; i < numVertices; i++)
			{
				clVertices[i].x = vertices[i].x;
				clVertices[i].y = vertices[i].y;
				clVertices[i].z = vertices[i].z;
				clVertices[i].w = 0.f;
			}

			m_Device->UnmapBuffer(m_GpuData->Vertices, 0, clVertices, &event);

			event->Wait();
			m_Device->DeleteEvent(event);
		}

		/// Faces
		{
			struct Face
			{
				u32 Idx[3];
				u32 TextureId;		/// mapped to m_GpuData->Texture
				float2 UV[3];
				bool CastShadow;
				bool Opaque;
			};

			m_GpuData->Faces = m_Device->CreateBuffer(
				numFaces * sizeof(Face),
				Calc::BufferType::kRead
			);

			Face* faceData = nullptr;			

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
				
				Fvector2* tc = face->getTC0();
				faceData[i].UV[0].x = tc[0].x;
				faceData[i].UV[0].y = tc[0].y;
				faceData[i].UV[1].x = tc[1].x;
				faceData[i].UV[1].y = tc[1].y;
				faceData[i].UV[2].x = tc[2].x;
				faceData[i].UV[2].y = tc[2].y;
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
		u64 NumPoints,
		u32 Flags,
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
		func->SetArg(arg++, m_GpuData->TextureOffsets);
		func->SetArg(arg++, m_GpuData->TexturesData);
		func->SetArg(arg++, Points);		
		func->SetArg(arg++, m_GpuData->Lights);
		func->SetArg(arg++, sizeof(NumPoints), &NumPoints);
		func->SetArg(arg++, sizeof(m_GpuData->NumRgbLights), &m_GpuData->NumRgbLights);
		func->SetArg(arg++, sizeof(m_GpuData->NumSunLights), &m_GpuData->NumSunLights);
		func->SetArg(arg++, sizeof(m_GpuData->NumHemiLights), &m_GpuData->NumHemiLights);
		func->SetArg(arg++, sizeof(Flags), &Flags);
		//func->SetArg(arg++, sizeof(Samples), &Samples);
		func->SetArg(arg++, Colors);

		size_t localsize = kWorkGroupSize;
		size_t globalsize = ((NumPoints + kWorkGroupSize - 1) / kWorkGroupSize) * kWorkGroupSize;

		m_Device->Execute(func, 0, globalsize, localsize, nullptr);
	}
}