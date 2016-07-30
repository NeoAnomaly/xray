#pragma once

#define LCL_EMBED_KERNELS 0

/// forward decl
//namespace CDB
//{
//	class CollectorPacked;
//}
//
//struct b_material;
//struct b_BuildTexture;
//class base_color;
//class base_lighting;

namespace LightingCL
{
	struct DeviceInfo
	{
		enum Type
		{
			kCpu,
			kGpu,
			kAccelerator
		};

		std::string name;
		std::string vendor;
		Type type;
	};

	class Buffer
	{
	public:
		virtual ~Buffer() {};
	};

	class Event
	{
	public:
		virtual ~Event() {};

		virtual bool Complete() const = 0;
		virtual void Wait() = 0;
	};

	class Exception
	{
	public:
		virtual ~Exception() {};
		virtual char const* What() const = 0;
	};

	enum MapType
	{
		MapRead = 0x1,
		MapWrite = 0x2
	};

#if (_MSC_VER < 1900)
	_declspec(align(16)) struct Point
#else
	struct alignas(16) Point
#endif
	{
		/// Fvector mapped to opencl as float3(sizeof 16) thus it must be aligned on 16 bytes boundary
		Fvector Position;
		Fvector Normal;
	};

	class ILightingCLApi
	{
	public:
		///
		/// Device management
		///
		static u32 GetDeviceCount();
		static void GetDeviceInfo(u32 DeviceIdx, DeviceInfo& DeviceInfo);

		///
		/// API lifetime management
		///
		static ILightingCLApi* Create(u32 DeviceIdx);
		static void Delete(ILightingCLApi* Api);

		///
		/// Scene management
		///
		virtual void LoadTextures(xr_vector<b_BuildTexture>& Textures) = 0;
		virtual void LoadLights(base_lighting& Lights) = 0;

		virtual void BuildCollisionModel(
			CDB::CollectorPacked& Collector, 
			xr_vector<struct b_material>& Materials
		) = 0;

		///
		/// Memory management
		///
		virtual Buffer* CreateBuffer(size_t Size, void* InitData) const = 0;
		virtual void DeleteBuffer(Buffer* Buffer) const = 0;

		virtual void ReadBuffer(
			const Buffer* Buffer,
			size_t Offset, 
			size_t Size, 
			void* Destination, 
			Event** Event
		) const = 0;
		
		virtual void MapBuffer(
			Buffer* Buffer, 
			MapType Type, 
			size_t Offset, 
			size_t Size, 
			void** Data, 
			Event** Event
		) const = 0;
		virtual void UnmapBuffer(Buffer* Buffer, void* Data, Event** Event) const = 0;

		/******************************************
		Events handling
		*******************************************/
		virtual void DeleteEvent(Event* Event) const = 0;

		///
		/// Lighting
		///
		//virtual void LightingPoints(
		//	Buffer* Colors, 
		//	Buffer* Points, 
		//	Buffer* RgbLights,
		//	Buffer* SunLights,
		//	Buffer* HemiLights,
		//	u64 NumPoints,
		//	u32 NumRgbLights,
		//	u32 NumSunLights,
		//	u32 NumHemiLights,
		//	u32 Samples
		//) = 0;

		virtual void LightingPoints(
			base_color_c* Colors, 
			Point* Points,
			u64 NumPoints,
			u32 Flags, 
			u32 Samples = 1
		) = 0;
	protected:
		ILightingCLApi() {};
		ILightingCLApi(ILightingCLApi const&);
		ILightingCLApi& operator = (ILightingCLApi const&);
		virtual ~ILightingCLApi() {};
		
	private:

	};
}