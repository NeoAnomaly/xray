/*************************************************************************
 INCLUDES
 **************************************************************************/

/*************************************************************************
EXTENSIONS
**************************************************************************/

/*************************************************************************
DEFINES
**************************************************************************/
#define LT_DIRECT 0
#define LT_POINT 1
#define LT_SECONDARY 2

#define STARTIDX(x)     (x->StartIdx)
#define LEAFNODE(x)     (((x).StartIdx) != -1)

enum
{
	LP_DEFAULT			= 0,
	LP_UseFaceDisable	= (1<<0),
	LP_dont_rgb			= (1<<1),
	LP_dont_hemi		= (1<<2),
	LP_dont_sun			= (1<<3),
};
/*************************************************************************
 TYPE DEFINITIONS
 **************************************************************************/
typedef struct _bbox
{
	float4 pmin;
	float4 pmax;
} bbox;

typedef struct _BvhNode
{
	bbox Bounds;
	long StartIdx;
	long NextIdx;
} BvhNode;

typedef struct _Face
{
	uint Idx[3];		// +0: 12 bytes
	ushort TextureId;	// +12: 2 bytes
	uchar CastShadow;	// +14
	uchar Opaque;		// +15
	float2 UV[3];		// +16: 24 bytes
	

	//float2 Padding;	// +40: 8 bytes
} Face;

typedef struct _Point
{
	float3 Position;
	float3 Normal;
} Point;

typedef struct _Color
{
	float R;		// - all static lighting
	float G;
	float B;

	float Hemi;		// - hemisphere
	float Sun;		// - sun

	float Nop;		// base_color_c::_tmp_

	float2 Padding;
} Color;

typedef struct _Light
{		
	float3 Diffuse;			// Diffuse color of light	
	float3 Position;		// Position in world space	
	float3 Direction;		// Direction in world space	
	float3 Attenuation;		// (Constant attenuation; Linear attenuation; Quadratic attenuation)
	float Range;			// Cutoff range
	float Energy;			// For radiosity ONLY
	ushort Type;			// Type of light source

	char Padding[6];
} Light;

typedef struct _Texture
{
	int Width;
	int Height;
	
	// Offset in texture data array
	ulong DataOffset;
} Texture;

typedef struct 
{
	__global const BvhNode* Nodes;				// BVH structure

	__global const float3* Vertices;			// Scene positional data
	__global const Face* Faces;

	__global const Texture* Textures;		// Array of textures desc
	__global const char* TextureData;

	__global const Light* Lights;
	uint NumRgbLights;
	uint NumSunLights;
	uint NumHemiLights;
} SceneData;

typedef struct _RayInternal
{
	float3 Origin;
	float3 Direction;
    float Range;
} RayInternal;

/*************************************************************************
HELPER FUNCTIONS
**************************************************************************/
float3 invert_float3(float3 Src)
{
    float3 res;
    res.x = -Src.x;
    res.y = -Src.y;
    res.z = -Src.z;
    return res;
}

float4 make_float4(float x, float y, float z, float w)
{
    float4 res;
    res.x = x;
    res.y = y;
    res.z = z;
    res.w = w;
    return res;
}

float3 make_float3(float x, float y, float z)
{
    float3 res;
    res.x = x;
    res.y = y;
    res.z = z;
    return res;
}

float2 make_float2(float x, float y)
{
    float2 res;
    res.x = x;
    res.y = y;
    return res;
}

// Intersect ray with the axis-aligned box
int IntersectBox(float3 RayOrigin, float3 InvDir, bbox Box, float MaxRange)
{
    const float3 f = (Box.pmax.xyz - RayOrigin.xyz) * InvDir;
    const float3 n = (Box.pmin.xyz - RayOrigin.xyz) * InvDir;

    const float3 tmax = max(f, n);
    const float3 tmin = min(f, n);

    const float t1 = min(min(tmax.x, min(tmax.y, tmax.z)), MaxRange);
    const float t0 = max(max(tmin.x, max(tmin.y, tmin.z)), 0.f);

    return (t1 >= t0) ? 1 : 0;
}

// Intersect Ray against triangle
bool IntersectTriangle(const RayInternal* Ray, float3 V1, float3 V2, float3 V3, float2* UV)
{
    const float3 e1 = V2 - V1;
    const float3 e2 = V3 - V1;
    const float3 s1 = cross(Ray->Direction.xyz, e2);
    const float  invd = native_recip(dot(s1, e1));
    const float3 d = Ray->Origin.xyz - V1;
    const float  b1 = dot(d, s1) * invd;
    const float3 s2 = cross(d, e1);
    const float  b2 = dot(Ray->Direction.xyz, s2) * invd;
    const float temp = dot(e2, s2) * invd;
    
    if (
		b1 < 0.f 
		|| 
		b1 > 1.f 
		|| 
		b2 < 0.f 
		|| 
		b1 + b2 > 1.f
        || 
		temp < 0.f 
		|| 
		temp > Ray->Range
		)
    {
        return false;
    }
    else
    {
        *UV = make_float2(b1, b2);

        return true;
    }
}


float4 Texture_Sample2D(
	float2 uv,
	ulong TextureId,
	__global const Texture* Textures,
	__global const char* TextureData
	)
{
// int U = iFloor(uv.x*float(T.dwWidth) + .5f);
// int V = iFloor(uv.y*float(T.dwHeight)+ .5f);
// U %= T.dwWidth;		if (U<0) U+=T.dwWidth;
// V %= T.dwHeight;	if (V<0) V+=T.dwHeight;

// u32 pixel		= T.pSurface[V*T.dwWidth+U];
// u32 pixel_a		= color_get_A(pixel);


	// Get width and height
	int width = Textures[TextureId].Width;
	int height = Textures[TextureId].Height;

	// Find the origin of the data in the pool
	__global const char* textureData = TextureData + Textures[TextureId].DataOffset;

	// Calculate integer coordinates
	int u = clamp((int)floor(uv.x * width), 0, width - 1);
	int v = clamp((int)floor(uv.y * height), 0, height - 1);

	// Get value
	__global const uchar4* textureDataUC = (__global const uchar4*)textureData;
	uchar4 value = *(textureDataUC + width * v + u); 
	float4 result = make_float4(
		(float)value.x / 255.f, 
		(float)value.y / 255.f, 
		(float)value.z / 255.f, 
		(float)value.w / 255.f
		);
	return result;
}

/*************************************************************************
BVH FUNCTIONS
**************************************************************************/

//  intersect a ray with leaf BVH node
float GetFaceOpacity(
    const SceneData* Scene,
    const BvhNode* Node,
    const RayInternal* Ray                // ray to instersect
    )
{
	float opacity = 1.f;
	float2 isectUV;
	float3 v1, v2, v3;
	
	long start = STARTIDX(Node);
	Face face = Scene->Faces[start];
	v1 = Scene->Vertices[face.Idx[0]];
	v2 = Scene->Vertices[face.Idx[1]];
	v3 = Scene->Vertices[face.Idx[2]];

	if (IntersectTriangle(Ray, v1, v2, v3, &isectUV))
	{
		if (!face.CastShadow)
			return 1.f;

		if (face.Opaque)
			return 0.f;

		// barycentric coords
		// note: W,U,V order
		float3 bc = make_float3(1.0f - isectUV.x - isectUV.y, isectUV.x, isectUV.y);

		// calc UV
		float2 uv;
		uv.x = face.UV[0].x * bc.x + face.UV[1].x * bc.y + face.UV[2].x * bc.z;
		uv.y = face.UV[0].y * bc.x + face.UV[1].y * bc.y + face.UV[2].y * bc.z;

		float a = Texture_Sample2D(uv, face.TextureId, Scene->Textures, Scene->TextureData).w;
		opacity = 1 - (a * a);
	}

	return opacity;
}

float RayTrace(
    const SceneData* Scene, 
    float3 StartPoint, 
    float3 Direction,
    float Range
    )
{
	uint loopBreaker = 0;
	float scale = 1.f;
	const float3 invDir = make_float3(1.f, 1.f, 1.f) / Direction.xyz;	
	long idx = 0;
	RayInternal ray =
	{
		StartPoint,
		Direction,
		Range
	};

	while (idx != -1)
	{
		// Try intersecting against current node's bounding box.
		// If this is the leaf try to intersect against contained triangle.
		BvhNode node = Scene->Nodes[idx];

		if (IntersectBox(StartPoint, invDir, node.Bounds, Range))
		{
			if (LEAFNODE(node))
			{
				/*
				/// perform intersection check and then calculate opacity value of the intersected face
				scale *= 0.5f;//GetFaceOpacity(Scene, &node, &ray);

				/// so, scale was too small, stop tracing
				if (scale < FLT_EPSILON)
					return scale;
*/
				idx = node.NextIdx;
			}
			// Traverse child nodes otherwise.
			else
			{
				++idx;
			}
		}
		else
		{
			idx = node.NextIdx;
		}

		if (++loopBreaker > 100)
		{
			scale = 0.f;
			break;
		}
	}

	return scale;
}

void LightPointForHemi(
// Input
const Point* Point,
const SceneData* Scene,
uint LightOffset,
// output
Color* Color
)
{
	float3 dirToLight;
    float3 startPoint = mad(Point->Normal, 0.01f, Point->Position);

	for (uint i = LightOffset; i < Scene->NumHemiLights; i++)
	{
		/// fetch light
		Light light = Scene->Lights[i];

		if (light.Type == LT_DIRECT) 
		{
			// Cos
			dirToLight = invert_float3(light.Direction);
			float D = dot(dirToLight, Point->Normal);
			if (D <= 0) 
				continue;

			// Trace Light
			float3 transformedPoint = mad(dirToLight, 0.001f, startPoint); 
			float scale = light.Energy * RayTrace(Scene, transformedPoint, dirToLight, 1000.f);
			Color->Hemi += scale;
		}
		else 
		{
			// Distance
			float distance = fast_distance(Point->Position, light.Position);
			if (distance > light.Range) 
				continue;

			// Dir
			dirToLight = light.Position - Point->Position;
			dirToLight = normalize(dirToLight);
			
			float D = dot(dirToLight, Point->Normal);
			if (D <= 0)			
				continue;

			// Trace Light
			float scale = D * light.Energy * RayTrace(Scene, startPoint, dirToLight, distance);
			float a = 
				scale 
				/ 
				(
					light.Attenuation.x + 
					light.Attenuation.y * distance + 
					light.Attenuation.z * (distance * distance)
				);

			Color->Hemi += a;
		}
	}
}

void LightPoint(
// Input
const Point* Point,
const SceneData* Scene,
uint Flags,
// output
Color* Color
)
{
    float3 dirToLight;
    float3 startPoint = mad(Point->Normal, 0.01f, Point->Position);
	
	bool needProcessing = ((Flags & LP_dont_rgb) == 0) && Scene->NumRgbLights;
	if (needProcessing)
	{
		for (uint i = 0; i < Scene->NumRgbLights; ++i)
		{
            /// fetch light
            Light light = Scene->Lights[i];

			switch (light.Type)
			{
			case LT_DIRECT:
			{
				// Cos
				dirToLight = invert_float3(light.Direction);
				float D = dot(dirToLight, Point->Normal);
				if (D <= 0) 
                    continue;

				// Trace LightCL
				float scale = D * light.Energy * RayTrace(Scene, startPoint, dirToLight, 1000.f);

				Color->R += scale * light.Diffuse.x;
				Color->G += scale * light.Diffuse.y;
				Color->B += scale * light.Diffuse.z;
			}
			break;
			case LT_POINT:
			{
				// Distance
				float distance = fast_distance(Point->Position, light.Position);
				if (distance > light.Range) 
					continue;

				// Dir
				dirToLight = light.Position - Point->Position;
				dirToLight = normalize(dirToLight);
				
				float D = dot(dirToLight, Point->Normal);
				if (D <= 0)			
					continue;

				// Trace Light
				float scale = D * light.Energy * RayTrace(Scene, startPoint, dirToLight, distance);
				float a;
				
				// if (gl_linear)	
				// 	a = 1 - distance / light.Range;
				// else			
					a = 
					scale 
					/ 
					(
						light.Attenuation.x + 
						light.Attenuation.y * distance + 
						light.Attenuation.z * (distance * distance)
					);

				Color->R += a * light.Diffuse.x;
				Color->G += a * light.Diffuse.y;
				Color->B += a * light.Diffuse.z;
			}
			break;
			// case LT_SECONDARY:
			// {
			// 	// Distance
			// 	float distance = fast_distance(Point->Position, light.Position);
			// 	if (distance > light.Range) 
			// 		continue;

			// 	// Dir
			// 	dirToLight.sub(L->position, P);
			// 	dirToLight.normalize_safe();
			// 	float	D = dirToLight.dotproduct(N);
			// 	if (D <= 0) continue;
			// 	D *= -dirToLight.dotproduct(L->direction);
			// 	if (D <= 0) continue;

			// 	// Jitter + trace light -> monte-carlo method
			// 	Fvector	Psave = L->position, Pdir;
			// 	L->position.mad(Pdir.random_dir(L->direction, PI_DIV_4), .05f);
			// 	float R = _sqrt(sqD);
			// 	float scale = powf(D, 1.f / 8.f)*L->energy*RayTrace(DB, MDL, *L, Pnew, dirToLight, R, skip, bUseFaceDisable);
			// 	float A = scale * (1 - R / L->range);
			// 	L->position = Psave;

			// 	Color->R += A * L->diffuse.x;
			// 	Color->G += A * L->diffuse.y;
			// 	Color->B += A * L->diffuse.z;
			// }
			// break;
			}
		}
	}

	needProcessing = ((Flags & LP_dont_sun) == 0) && Scene->NumSunLights;
	if (needProcessing)
	{
		for (uint i = Scene->NumRgbLights; i < Scene->NumSunLights; ++i)
		{
            /// fetch light
            Light light = Scene->Lights[i];

			if (light.Type == LT_DIRECT) 
			{
				// Cos
				dirToLight = invert_float3(light.Direction);
				float D = dot(dirToLight, Point->Normal);
				if (D <= 0) 
                    continue;

				// Trace Light
				float scale = light.Energy * RayTrace(Scene, startPoint, dirToLight, 1000.f);
				Color->Sun += scale;
			}
			else 
			{
				// Distance
				float distance = fast_distance(Point->Position, light.Position);
				if (distance > light.Range) 
					continue;

				// Dir
				dirToLight = light.Position - Point->Position;
				dirToLight = normalize(dirToLight);
				
				float D = dot(dirToLight, Point->Normal);
				if (D <= 0)			
					continue;

				// Trace Light
				float scale = D * light.Energy * RayTrace(Scene, startPoint, dirToLight, distance);
				float a = 
					scale 
					/ 
					(
						light.Attenuation.x + 
						light.Attenuation.y * distance + 
						light.Attenuation.z * (distance * distance)
					);

				Color->Sun += a;
			}
		}
	}

	needProcessing = ((Flags & LP_dont_hemi) == 0) && Scene->NumHemiLights;
	if (needProcessing)
	{
		LightPointForHemi(Point, Scene, Scene->NumRgbLights + Scene->NumSunLights, Color);
	}
}

//func->SetArg(arg++, m_GpuData->BvhNodes);
//func->SetArg(arg++, m_GpuData->Vertices);
//func->SetArg(arg++, m_GpuData->Faces);
//func->SetArg(arg++, m_GpuData->Textures);
//func->SetArg(arg++, m_GpuData->TexturesData);
//func->SetArg(arg++, Colors);
//func->SetArg(arg++, Points);
//func->SetArg(arg++, RgbLights);
//func->SetArg(arg++, SunLights);
//func->SetArg(arg++, HemiLights);
//func->SetArg(arg++, sizeof(NumPoints), &NumPoints);
//func->SetArg(arg++, sizeof(NumRgbLights), &NumRgbLights);
//func->SetArg(arg++, sizeof(NumSunLights), &NumSunLights);
//func->SetArg(arg++, sizeof(NumHemiLights), &NumHemiLights);
//func->SetArg(arg++, sizeof(Samples), &Samples);

__attribute__((reqd_work_group_size(64, 1, 1)))
__kernel void LightingPointsMS(
// Input
__global const BvhNode* Nodes,				// BVH nodes
__global const float3* Vertices,			// Scene positional data
__global const Face* Faces,					// Scene indices
__global const Texture* Textures,			// Array of textures desc
__global const char* TextureData,			// Plain texture surfaces sequence
__global const Point* Points,				// Points that will be lighted
__global const Light* Lights,
ulong NumPoints,
uint NumRgbLights,
uint NumSunLights,
uint NumHemiLights,
uint Flags,
uint Samples,
// output
__global Color* Colors
)
{
    int global_id = get_global_id(0);

    // Handle only working subset
    if (global_id < NumPoints)
    {
		// Fill scene data 
		SceneData scenedata =
		{
			Nodes,
			Vertices,
			Faces
		};

		for (uint sample = 0; sample < Samples; sample++)
		{
			/// fetch and transform point
			Point point = Points[global_id];
            
            // float a	= 0.2f * float(sample) / float(Samples);
			// Fvector				P,N;

			// N.random_dir(vN, deg2rad(30.f));
			// P.mad				(vP,N,a);

			// LightPoint			(&DB, MDL, vC, P, N, lights, flags, 0);
		}
    }
}

__attribute__((reqd_work_group_size(64, 1, 1)))
__kernel void LightingPoints(
// Input
__global const BvhNode* Nodes,				// BVH nodes
__global const float3* Vertices,			// Scene positional data
__global const Face* Faces,					// Scene indices
__global const Texture* Textures,			// Array of textures desc
__global const char* TextureData,			// Plain texture surfaces sequence
__global const Point* Points,				// Points that will be lighted
__global const Light* Lights,
ulong NumPoints,
uint NumRgbLights,
uint NumSunLights,
uint NumHemiLights,
uint Flags,
// output
__global Color* Colors
)
{
    int global_id = get_global_id(0);

	// if (global_id == 0)
	// {
	// 	printf("Global id hello: %d of %d\n", global_id, get_global_id(0));
	// }

    /// Handle only working subset
    if (global_id < NumPoints)
    {
		/// Fill scene data 
		SceneData scenedata =
		{
			Nodes,
			Vertices,
			Faces,
			Textures,
			TextureData,
			Lights,
			NumRgbLights,
			NumSunLights,
			NumHemiLights
		};

        /// fetch point data
        Point point = Points[global_id];
        Color color = Colors[global_id];

		size_t size = sizeof(BvhNode);
		size_t size2 = sizeof(Color);
        
        LightPoint(
				&point,
				&scenedata,
				Flags, 
				&color
            );

        /// write data back
        Colors[global_id] = color;
    }
}

__attribute__((reqd_work_group_size(64, 1, 1)))
__kernel void DataTest(
// Input
//__global const BvhNode* Nodes,	
//__global const float3* Vertices,
//__global const Face* Faces,		
__global const Texture* Textures,
__global const char* TextureData,	/// entry size = sizeof(Texture) 
//__global const Point* Points,	
__global const Light* Lights,
//__global const Color* Colors,
ulong NumEntries
)
{
	int global_id = get_global_id(0);

	if (global_id < NumEntries)
	{
		// BvhNode node = Nodes[global_id];
		// float3 vertice = Vertices[global_id];
		// Face face = Faces[global_id];
		Texture texture = Textures[global_id];
		Light light = Lights[global_id];
		// Point point = Points[global_id];
        // Color color = Colors[global_id];

		size_t size = sizeof(Face);
		size_t size2 = sizeof(Color);

		__global const char* textureData = TextureData + texture.DataOffset;
		//__global const Texture* textureDataTex = textureData;
		Texture textureFromData = ((__global const Texture*)(textureData))[0];

		mem_fence(CLK_GLOBAL_MEM_FENCE);
	}
}