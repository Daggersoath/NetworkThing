#include "Network.h"

namespace {

	enum Constants : uint16_t
	{
		Protocol_Identifier_Size = 16,
		Protocol_Version_Size = 12,

		Max_Packet_Size = 512
	};

	enum MessageType : uint8_t
	{
		eReliable = 1 << 0,
		eOrdered = 1 << 1,

		eMultiPart = 1 << 2,
		eResentPackage = 1 << 3,

		eBits = 4
	};

}

struct Header
{
	Header() : protocolId(0x0fbad000u | 0x001u), msgType(0) {}
	uint32_t checkSum = 0;
	uint32_t protocolId : Protocol_Identifier_Size + Protocol_Version_Size; // protocol(16 bit) + ver(12 bit)
	uint8_t msgType : eBits;
	PacketManager::SequenceType lastAck = 0;
	uint32_t ackBacklog = 0;
};

struct ReliableHeader : Header
{
	ReliableHeader() { msgType = eReliable; }
	PacketManager::SequenceType seq = 0; // 30p/s = 1657 days runtime before reset needed
};

struct OrderedHeader : ReliableHeader
{
	OrderedHeader() { msgType = eReliable | eOrdered; }
	PacketManager::SequenceType lastOrdered = 0;
};

std::vector<uint8_t*> PacketManager::GetPackets(uint32_t dataSize, bool reliable, bool ordered)
{

	return std::vector<uint8_t*>();
}

void PacketManager::HandlePacket(uint8_t* packet, uint32_t packetSize)
{
	if (packetSize > Max_Packet_Size || packetSize < sizeof(Header)) return; // Invalid packet

	if (!HandleHeader((Header*)packet)) return; // Invalid packet
}

void* PacketManager::HandleHeader(Header* message)
{
	if (message->msgType & (eReliable | eOrdered) != eReliable | eOrdered) return nullptr; // Invalid packet
	ClearAckedMessages(message->lastAck, message->ackBacklog);

	if (message->msgType & eReliable)
	{
		UpdateRecievedSequence(static_cast<ReliableHeader*>(message)->seq); 
		if (message->msgType & eOrdered)
		{
			return ((uint8_t*)message) + sizeof(OrderedHeader);
		}
	}

	return ((uint8_t*)message) + (message->msgType & eReliable ? sizeof(ReliableHeader) : sizeof(Header));
}

void PacketManager::UpdateRecievedSequence(SequenceType seq)
{
	if (seq < m_LastAckedSeq)
	{
		m_AckBacklog &= (1u << 31u) >> (m_LastAckedSeq - seq);
	}
	else
	{
		m_AckBacklog >>= seq - m_LastAckedSeq;
		m_AckBacklog |= (1u << 31u);
		m_LastAckedSeq = seq;
	}
}

void PacketManager::ClearAckedMessages(SequenceType lastAck, AckBacklogType ackBacklog)
{
	auto it = m_WaitingMessages.begin();
	if (it == m_WaitingMessages.end()) return;

	for (uint8_t i = 0; i < sizeof(ackBacklog) * 8; ++i)
	{
		if (ackBacklog & (1u << i))
		{
			SequenceType seq = lastAck - ((sizeof(SequenceType) - 1) - i);
			while (static_cast<ReliableHeader*>(it.operator*())->seq < seq) if (++it == m_WaitingMessages.end()) return;
			if (static_cast<ReliableHeader*>(it.operator*())->seq == seq) if ((it = m_WaitingMessages.erase(it)) == m_WaitingMessages.end()) return;
		}
	}
	while (static_cast<ReliableHeader*>(it.operator*())->seq < lastAck) if (++it == m_WaitingMessages.end()) return;
	if (static_cast<ReliableHeader*>(it.operator*())->seq == lastAck) m_WaitingMessages.erase(it);
}
