#pragma once

#include <cstdint>
#include <list>
#include <vector>

class PacketManager
{
public:
	PacketManager() = default;

	std::vector<uint8_t*> GetPackets(uint32_t dataSize, bool reliable = false, bool ordered = false);
	void HandlePacket(uint8_t* packet, uint32_t packetSize);

private:
	friend struct Header;
	friend struct ReliableHeader;
	friend struct OrderedHeader;
	typedef uint32_t SequenceType;
	typedef uint32_t AckBacklogType;

	void* HandleHeader(struct Header* message);

	void UpdateRecievedSequence(SequenceType seq);
	void ClearAckedMessages(SequenceType lastAck, AckBacklogType ackBacklog);

	std::list<struct Header*> m_WaitingMessages;
	std::list<struct Header*> m_WaitingMessages;

	SequenceType m_CurSeq = 0;
	SequenceType m_LastAckedSeq = 0;
	AckBacklogType m_AckBacklog = 0;

};

