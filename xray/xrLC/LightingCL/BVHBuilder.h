#pragma once

#include <atomic>

namespace LightingCL
{
	class BVHBuilder
	{
	public:
		/// forward
		struct Node;

		enum NodeType
		{
			Internal,
			Leaf
		};

	public:
		BVHBuilder();
		~BVHBuilder();

		void Build(const Fbox* Bounds, size_t  NumBounds);

		size_t GetNodeCount() const { return m_NodeCount; }
		const Node* GetRootNode() const { return m_RootNode; }
		const size_t* GetFaceIds() const { return &m_FaceIds[0]; }
	private:
		void Reset();		

		Node* AllocateNode();
		void InitNodeAllocator(size_t MaxNum);

		struct SplitRequest
		{
			// Starting index of a request
			size_t StartIdx;
			// Number of faces
			size_t NumFaces;
			// Root node
			Node** RootNode;
			// Bounding box
			Fbox Bounds;
			// Centroid bounds
			Fbox CentroidBounds;
			// Level
			size_t Level;
		};

		struct SahSplit
		{
			int Dimension;
			float Split;
		};

		void BuildNode(const SplitRequest& Request, const Fbox* Bounds, const Fvector* Centroids, size_t* FaceIds);

		std::atomic<size_t> m_NodeCount;
		xr_vector<Node> m_Nodes;
		xr_vector<size_t> m_FaceIds;	// ids of leaf faces
		Fbox m_Bounds;
		Node* m_RootNode;
		size_t m_TreeHeight;
	};

	struct BVHBuilder::Node
	{
		Fbox Bounds;		/// Node bounds in world space
		NodeType Type;

		union
		{
			/// For internal nodes: left and right children
			struct
			{
				Node* Left;
				Node* Right;
			};

			/// For leaves: starting face index and number of faces
			struct
			{
				size_t StartIdx;
				size_t NumFaces;
			};
		};
	};
}
