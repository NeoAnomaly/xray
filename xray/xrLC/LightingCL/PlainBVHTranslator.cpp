#include "stdafx.h"

#include "BVHBuilder.h"

#include "PlainBVHTranslator.h"

const int64_t cInvalidNodeId = -1;

namespace LightingCL
{
	PlainBVHTranslator::PlainBVHTranslator():
		m_NodeCount(0),
		m_RootIdx(0)
	{
	}

	PlainBVHTranslator::~PlainBVHTranslator()
	{
	}

	void PlainBVHTranslator::Process(const BVHBuilder* BvhBuilder)
	{
		m_NodeCount = 0;
		size_t newSize = BvhBuilder->GetNodeCount();

		m_Nodes.resize(newSize);
		m_StartIdx.resize(newSize);

		const BVHBuilder::Node* rootNode = BvhBuilder->GetRootNode();
		R_ASSERT2(rootNode, "BVHBuilder seems doesn't ready.");

		size_t currentRootIdx = 0;

		ProcessNode(rootNode);

		m_Nodes[currentRootIdx].NextIdx = cInvalidNodeId;

		for (size_t i = 0; i < m_Nodes.size(); i++)
		{
			if (m_Nodes[i].StartIdx != cInvalidNodeId)
			{
				m_Nodes[i + 1].NextIdx = m_Nodes[i].StartIdx;

				m_Nodes[m_Nodes[i].StartIdx].NextIdx = m_Nodes[i].NextIdx;
			}
		}

		for (size_t i = 0; i < m_Nodes.size(); i++)
		{
			if (m_Nodes[i].StartIdx == cInvalidNodeId)
			{
				m_Nodes[i].StartIdx = m_StartIdx[i];
			}
			else
			{
				m_Nodes[i].StartIdx = cInvalidNodeId;
			}
		}
	}

	int64_t PlainBVHTranslator::ProcessNode(const BVHBuilder::Node* Node)
	{
		size_t idx = m_NodeCount;

		PlainBVHTranslator::Node& node = m_Nodes[m_NodeCount];
		node.Bounds = Node->Bounds;
		int64_t& startIdx = m_StartIdx[m_NodeCount++];

		if (Node->Type == BVHBuilder::Leaf)
		{
			startIdx = Node->StartIdx;
			node.StartIdx = cInvalidNodeId;
		}
		else
		{
			ProcessNode(Node->Left);
			node.StartIdx = ProcessNode(Node->Right);
		}

		return idx;
	}
}