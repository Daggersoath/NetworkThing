#include "PacketManager.h"

namespace {

	constexpr uint8_t Protocol_Identifier_Size = 16;
	constexpr uint8_t Protocol_Version_Size = 12;
	constexpr uint16_t IP_Header_Size = 48;

	constexpr uint32_t Protocol_Id = 0x0fbad000u | 0x001u;

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
	uint32_t checkSum = 0;
	uint32_t protocolId : Protocol_Identifier_Size + Protocol_Version_Size; // protocol(16 bit) + ver(12 bit)
	uint8_t msgType : eBits;
	PacketManager::SequenceType lastAck = 0;
	PacketManager::AckBacklogType ackBacklog = 0;
};

struct ReliableHeader : Header
{
	decltype(Header::lastAck) seq = 0; // 30p/s = 1657 days runtime before reset needed
};

struct OrderedHeader : ReliableHeader
{
	decltype(Header::lastAck) lastOrdered = 0;
};

struct HeaderOptionalData_MultiPart
{
	bool first : 1;
	bool last : 1;
	uint16_t msgPart : 14;
};

struct HeaderOptionalData_Resend
{
	decltype(ReliableHeader::seq) originalSeq;
};

struct HeaderOptionalData
{
	HeaderOptionalData_MultiPart multiPartData;
	HeaderOptionalData_Resend resendData;
};

PacketManager::PacketManager(uint16_t maxPacketSize)
{
	SetMaxPacketSize(maxPacketSize);
}

void PacketManager::SetMaxPacketSize(uint16_t maxPacketSize)
{
	constexpr uint16_t totalHeaderSize = (IP_Header_Size + sizeof(OrderedHeader) + sizeof(HeaderOptionalData));

#ifdef _DEBUG
	if (maxPacketSize <= totalHeaderSize)
	{
		static_assert("max packet size too small!");
	}
#endif

	m_MaxPacketSize = maxPacketSize - totalHeaderSize;
}

std::vector<Packet> PacketManager::GeneratePacketsFromData(uint8_t* data, uint32_t dataSize, bool ordered, bool reliable)
{
	std::vector<Packet> packets = GetPackets(dataSize, ordered, reliable);
	uint32_t copied = 0;
	for (Packet& p :  packets)
	{
		uint32_t copySize = (dataSize - copied) > m_MaxPacketSize ? m_MaxPacketSize : (dataSize - copied);
		memcpy(p.Data, data + copied, p.DataSize);
		copied += p.DataSize;
	}

	return packets;
}

std::vector<Packet> PacketManager::GetPackets(uint32_t dataSize, bool ordered, bool reliable)
{
	std::vector<Packet> packets;
	bool multi = false;
	uint32_t partNr = 0;
	while (dataSize)
	{
		uint32_t packetDataSize = dataSize <= m_MaxPacketSize ? dataSize : m_MaxPacketSize;
		dataSize -= packetDataSize;
		multi |= dataSize == 0;
		packets.push_back(GetPacket(packetDataSize, ordered, reliable, multi, false));
		Packet& p = packets.back();
		if (multi)
		{
		}
	}

	return packets;
}

void PacketManager::HandlePacket(Packet packet)
{
	if (packet.DataSize > m_MaxPacketSize || packet.HeaderSize + packet.DataSize < sizeof(Header)) return; // Invalid packet

	if (!HandleHeader((Header*)packet.Header)) return; // Invalid packet

	m_ReadyPackets.push_back(packet);
}

std::vector<Packet> PacketManager::GetRecievedPackets()
{
	return m_ReadyPackets;
}

Packet PacketManager::GetPacket(uint32_t size, bool ordered, bool reliable, bool multi, bool resend)
{
	Packet p;
	p.HeaderSize = (ordered ? sizeof(OrderedHeader) : reliable ? sizeof(ReliableHeader) : sizeof(Header))
		+ (multi ? sizeof(HeaderOptionalData_MultiPart) : 0)
		+ (resend ? sizeof(HeaderOptionalData_Resend) : 0);
	p.DataSize = size;
	p.Header = (Header*)new uint8_t[p.HeaderSize + p.DataSize];
	p.Data = ((uint8_t*)p.Header) + p.HeaderSize;

	p.Header->ackBacklog = m_AckBacklog;
	p.Header->lastAck = m_LastAckedSeq;
	p.Header->msgType = ordered * eOrdered | reliable * eReliable | multi * eMultiPart | resend * eResentPackage;
	p.Header->protocolId = Protocol_Id;
	return p;
}

void* PacketManager::HandleHeader(Header* message)
{
	if ((message->msgType & (eReliable | eOrdered)) != (eReliable | eOrdered)) return nullptr; // Invalid packet
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
	auto it = m_PacketsWaitingForAck.begin();
	if (it == m_PacketsWaitingForAck.end()) return;

	for (uint8_t i = 0; i < sizeof(ackBacklog) * 8; ++i)
	{
		if (ackBacklog & (1u << i))
		{
			SequenceType seq = lastAck - ((sizeof(SequenceType) - 1) - i);
			while (static_cast<ReliableHeader*>(it.operator*())->seq < seq) if (++it == m_PacketsWaitingForAck.end()) return;
			if (static_cast<ReliableHeader*>(it.operator*())->seq == seq) if ((it = m_PacketsWaitingForAck.erase(it)) == m_PacketsWaitingForAck.end()) return;
		}
	}
	while (static_cast<ReliableHeader*>(it.operator*())->seq < lastAck) if (++it == m_PacketsWaitingForAck.end()) return;
	if (static_cast<ReliableHeader*>(it.operator*())->seq == lastAck) m_PacketsWaitingForAck.erase(it);
}
