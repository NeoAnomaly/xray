#pragma once

#include "ILightingCLApi.h"

#include "device.h"

namespace LightingCL
{
	class BufferImpl : public Buffer
	{
	public:
		BufferImpl(Calc::Buffer* Buffer) : m_Buffer(Buffer) {};
		~BufferImpl() = default;

		Calc::Buffer* GetData() { return m_Buffer; }
	private:
		Calc::Buffer* m_Buffer;
	};
}
