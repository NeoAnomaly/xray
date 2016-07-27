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
	long NetxtIdx;
} BvhNode;

typedef struct _Face
{
	uint Idx[3];
	uint TextureId;	
	float2 UV;
	bool CastShadow;
	bool Opaque;
} Face;

typedef struct _Point
{
	float3 Position;
	float3 Normal;
} Point;

typedef struct _Color
{
	float3 Rgb;		// - all static lighting
	float Hemi;		// - hemisphere
	float Sun;		// - sun
} Color;

typedef struct _Light
{
	uchar Type;				// Type of light source		
	float3 Diffuse;			// Diffuse color of light	
	float3 Position;		// Position in world space	
	float3 Direction;		// Direction in world space	
	float2 Range;			// (Cutoff range; Cutoff range ^ 2)
	float3 Attenuation;		// (Constant attenuation; Linear attenuation; Quadratic attenuation)
	float Energy;			// For radiosity ONLY
} Light;

typedef struct 
{
	__global const BvhNode* Nodes;				// BVH structure

	__global const float3* Vertices;			// Scene positional data
	__global const Face* Faces;

	__global const ulong* TextureOffsets;		// Array of texture offsets in the TexturesData
	__global const char* TexturesData;

	__global const Light* Lights;
	uint NumRgbLights;
	uint NumSunLights;
	uint NumHemiLights;
} SceneData;

typedef struct _Ray
{
	float3 Origin;
	float3 Direction;
    float Range;
} Ray;

typedef struct _Intersection
{
	float4 uvwt;
	long FaceId;
} Intersection;

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
int IntersectTriangle(const Ray* Ray, float3 V1, float3 V2, float3 V3, Intersection* Isect)
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
        return 0;
    }
    else
    {
        Isect->uvwt = make_float4(b1, b2, 0.f, temp);
        return 1;
    }
}
/*************************************************************************
BVH FUNCTIONS
**************************************************************************/

//  intersect a ray with leaf BVH node
bool IntersectLeafClosest(
    const SceneData* Scene,
    const BvhNode* Node,
    const Ray* Ray,                // ray to instersect
    Intersection* Isect          // Intersection structure
    )
{
    float3 v1, v2, v3;
    Face face;
    long start = STARTIDX(Node);
    face = Scene->Faces[start];
    v1 = Scene->Vertices[face.Idx[0]];
    v2 = Scene->Vertices[face.Idx[1]];
    v3 = Scene->Vertices[face.Idx[2]];

	if (IntersectTriangle(Ray, v1, v2, v3, Isect))
	{
		Isect->FaceId = start;

		return true;
	}
     
    return false;
}

float RayTrace(
    const SceneData* Scene, 
    float3 StartPoint, 
    float3 Direction,
    float Range
    )
{
	float scale = 1.f;
	const float3 invDir  = make_float3(1.f, 1.f, 1.f) / Direction.xyz;	
	long idx = 0;
	Ray ray =
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
				Intersection isect = 
				{
					make_float4(0.f, 0.f, 0.f, Range),
					-1
				};

				IntersectLeafClosest(Scene, &node, &ray, &isect);

				idx = node.NetxtIdx;
			}
			// Traverse child nodes otherwise.
			else
			{
				++idx;
			}
		}
		else
		{
			idx = node.NetxtIdx;
		}
	}

	return scale;
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
	
	if (((Flags & LP_dont_rgb) == 0) && Scene->NumRgbLights)
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

				Color->Rgb.x += scale * light.Diffuse.x;
				Color->Rgb.y += scale * light.Diffuse.y;
				Color->Rgb.z += scale * light.Diffuse.z;
			}
			break;
			case LT_POINT:
			{
				// Distance
				float distance = fast_distance(Point->Position, light.Position);
				if (distance > light.Range.x) 
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
				// 	a = 1 - distance / light.Range.x;
				// else			
					a = 
					scale 
					/ 
					(
						light.Attenuation.x + 
						light.Attenuation.y * distance + 
						light.Attenuation.z * (distance * distance)
					);

				Color->Rgb.x += a * light.Diffuse.x;
				Color->Rgb.y += a * light.Diffuse.y;
				Color->Rgb.z += a * light.Diffuse.z;
			}
			break;
			// case LT_SECONDARY:
			// {
			// 	// Distance
			// 	float distance = fast_distance(Point->Position, light.Position);
			// 	if (distance > light.Range.x) 
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

			// 	C.rgb.x += A * L->diffuse.x;
			// 	C.rgb.y += A * L->diffuse.y;
			// 	C.rgb.z += A * L->diffuse.z;
			// }
			// break;
			}
		}
	}
	if (((Flags & LP_dont_sun) == 0) && Scene->NumSunLights)
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
				if (distance > light.Range.x) 
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
	if (((Flags & LP_dont_hemi) == 0) && Scene->NumHemiLights)
	{
		for (uint i = Scene->NumRgbLights + Scene->NumSunLights; i < Scene->NumHemiLights; ++i)
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
				if (distance > light.Range.x) 
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
}

//func->SetArg(arg++, m_GpuData->BvhNodes);
//func->SetArg(arg++, m_GpuData->Vertices);
//func->SetArg(arg++, m_GpuData->Faces);
//func->SetArg(arg++, m_GpuData->TextureOffsets);
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
__global const ulong* TextureOffsets,		// Array of texture offsets in the TexturesData
__global const char* TexturesData,			// Plain texture surfaces sequence
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

    // Fill scene data 
    SceneData scenedata =
    {
        Nodes,
        Vertices,
        Faces
    };

    // Handle only working subset
    if (global_id < NumPoints)
    {
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
__global const ulong* TextureOffsets,		// Array of texture offsets in the TexturesData
__global const char* TexturesData,			// Plain texture surfaces sequence
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

    // Fill scene data 
    SceneData scenedata =
    {
        Nodes,
        Vertices,
        Faces,
		TextureOffsets,
		TexturesData,
		Lights,
		NumRgbLights,
		NumSunLights,
		NumHemiLights
    };

    // Handle only working subset
    if (global_id < NumPoints)
    {
        /// fetch point data
        Point point = Points[global_id];
        Color color = Colors[global_id];
        
        LightPoint(
            &point,
            &scenedata,
			Flags, 
            &color
            );

        // write data back
        Colors[global_id] = color;
    }
}