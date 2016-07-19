#include "stdafx.h"

#include <numeric>

#include "BVHBuilder.h"

namespace LightingCL
{
	BVHBuilder::BVHBuilder()
	{
	}

	BVHBuilder::~BVHBuilder()
	{
	}

	void BVHBuilder::BuildNode(
		const SplitRequest & Request, 
		const Fbox * Bounds, 
		const Fvector * Centroids, 
		size_t * FaceIds
	)
	{
		m_TreeHeight = _max(m_TreeHeight, Request.Level);

		Node* node = AllocateNode();
		node->Bounds = Request.Bounds;

		if (Request.NumFaces < 2)
		{
			node->Type = Leaf;
			node->StartIdx = Request.StartIdx;
			node->NumFaces = Request.NumFaces;
		}
		else
		{
			node->Type = Internal;

			/// get maximum dim axis idx(0: x axis, 1: y axis, 2: z axis)
			int maxDimAxis = 0;
			float border = 0.f;
			Fvector size, center;

			Request.CentroidBounds.getsize(size);
			Request.CentroidBounds.getcenter(center);			

			if (size.x >= size.y && size.x >= size.z)
				maxDimAxis = 0;
			if (size.y >= size.x && size.y >= size.z)
				maxDimAxis = 1;
			if (size.z >= size.x && size.z >= size.y)
				maxDimAxis = 2;

			border = center[maxDimAxis];

			Fbox leftbounds, rightbounds, leftcentroid_bounds, rightcentroid_bounds;
			leftbounds.invalidate();
			rightbounds.invalidate();
			leftcentroid_bounds.invalidate();
			rightcentroid_bounds.invalidate();

			size_t splitIdx = Request.StartIdx;

			bool near2far = (Request.NumFaces + Request.StartIdx) & 0x1;

			if (size[maxDimAxis] > 0.f)
			{
				size_t first = Request.StartIdx;
				size_t last = Request.StartIdx + Request.NumFaces;

				if (near2far)
				{
					while (1)
					{
						while (
							(first != last) 
							&&
							Centroids[FaceIds[first]][maxDimAxis] < border
							)
						{
							leftbounds.merge(Bounds[FaceIds[first]]);
							leftcentroid_bounds.modify(Centroids[FaceIds[first]]);
							++first;
						}

						if (first == last--) 
							break;

						rightbounds.merge(Bounds[FaceIds[first]]);
						rightcentroid_bounds.modify(Centroids[FaceIds[first]]);

						while (
							(first != last) 
							&&
							Centroids[FaceIds[last]][maxDimAxis] >= border
							)
						{
							rightbounds.merge(Bounds[FaceIds[last]]);
							rightcentroid_bounds.modify(Centroids[FaceIds[last]]);
							--last;
						}

						if (first == last) 
							break;

						leftbounds.merge(Bounds[FaceIds[last]]);
						leftcentroid_bounds.modify(Centroids[FaceIds[last]]);

						std::swap(FaceIds[first++], FaceIds[last]);
					}
				}
				else
				{
					while (1)
					{
						while (
							(first != last)
							&&
							Centroids[FaceIds[first]][maxDimAxis] >= border
							)
						{
							leftbounds.merge(Bounds[FaceIds[first]]);
							leftcentroid_bounds.modify(Centroids[FaceIds[first]]);
							++first;
						}

						if (first == last--)
							break;

						rightbounds.merge(Bounds[FaceIds[first]]);
						rightcentroid_bounds.modify(Centroids[FaceIds[first]]);

						while (
							(first != last)
							&&
							Centroids[FaceIds[last]][maxDimAxis] < border
							)
						{
							rightbounds.merge(Bounds[FaceIds[last]]);
							rightcentroid_bounds.modify(Centroids[FaceIds[last]]);
							--last;
						}

						if (first == last)
							break;

						leftbounds.merge(Bounds[FaceIds[last]]);
						leftcentroid_bounds.modify(Centroids[FaceIds[last]]);

						std::swap(FaceIds[first++], FaceIds[last]);
					}					
				}

				splitIdx = first;
			}

			if (splitIdx == Request.StartIdx || splitIdx == Request.StartIdx + Request.NumFaces)
			{
				splitIdx = Request.StartIdx + (Request.NumFaces >> 1);

				for (size_t i = Request.StartIdx; i < splitIdx; ++i)
				{
					leftbounds.merge(Bounds[FaceIds[i]]);
					leftcentroid_bounds.modify(Centroids[FaceIds[i]]);
				}

				for (size_t i = splitIdx; i < Request.StartIdx + Request.NumFaces; ++i)
				{
					rightbounds.merge(Bounds[FaceIds[i]]);
					rightcentroid_bounds.modify(Centroids[FaceIds[i]]);
				}
			}

			// Left request
			SplitRequest leftrequest = 
			{ 
				Request.StartIdx,
				splitIdx - Request.StartIdx,
				&node->Left, 
				leftbounds, 
				leftcentroid_bounds, 
				Request.Level + 1
			};

			// Right request
			SplitRequest rightrequest =
			{
				splitIdx,
				Request.NumFaces - (splitIdx - Request.StartIdx),
				&node->Right,
				rightbounds,
				rightcentroid_bounds,
				Request.Level + 1
			};

			BuildNode(leftrequest, Bounds, Centroids, FaceIds);			

			BuildNode(rightrequest, Bounds, Centroids, FaceIds);
		}

		// Set parent ptr if any
		if (Request.RootNode) 
			*Request.RootNode = node;
	}

	void BVHBuilder::Build(const Fbox* Bounds, size_t  NumBounds)
	{
		Reset();

		for (size_t i = 0; i < NumBounds; i++)
		{
			m_Bounds.merge(Bounds[i]);
		}

		InitNodeAllocator(2 * NumBounds - 1);

		// preallocate some stuff memory
		FvectorVec centroids(NumBounds);
		m_FaceIds.resize(NumBounds);
		std::iota(m_FaceIds.begin(), m_FaceIds.end(), 0);

		Fbox centroidBounds;
		centroidBounds.invalidate();

		for (size_t i = 0; i < NumBounds; i++)
		{
			Fvector center;
			Bounds[i].getcenter(center);

			centroidBounds.modify(center);
			centroids[i] = center;
		}

		SplitRequest initial =
		{
			0,
			NumBounds,
			nullptr,
			m_Bounds,
			centroidBounds,
			0
		};

		BuildNode(initial, Bounds, &centroids[0], &m_FaceIds[0]);

		m_RootNode = &m_Nodes[0];
	}

	void BVHBuilder::InitNodeAllocator(size_t MaxNum)
	{
		m_NodeCount = 0;
		m_Nodes.resize(MaxNum);
	}

	BVHBuilder::Node* BVHBuilder::AllocateNode()
	{
		return &m_Nodes[m_NodeCount++];
	}

	void BVHBuilder::Reset()
	{
		m_Nodes.clear();
		m_Bounds.invalidate();
		m_RootNode = nullptr;
		m_TreeHeight = 0;
	}
}