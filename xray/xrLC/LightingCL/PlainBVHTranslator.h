#pragma once

namespace LightingCL
{
	/// forward
	class BVHBuilder;

	class PlainBVHTranslator
	{
	public:
		struct Node
		{
			Fbox Bounds;
			int64_t StartIdx;		/// translates to cl_long
			int64_t NextIdx;
		};

	public:
		PlainBVHTranslator();
		~PlainBVHTranslator();

		void Process(const BVHBuilder* BvhBuilder);

		xr_vector<Node>& GetNodes() { return m_Nodes; }
	private:
		int64_t ProcessNode(const BVHBuilder::Node* Node);

		xr_vector<Node> m_Nodes;
		xr_vector<int64_t> m_StartIdx;
		xr_vector<size_t> m_Roots;
		size_t m_NodeCount;
		size_t m_RootIdx;
	};

	/*const size_t size = sizeof(PlainBVHTranslator::Node);
	const size_t size2 = sizeof(Fbox);
	*/
	//const size_t size3 = sizeof(Fvector);
}
