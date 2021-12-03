#pragma once

#include <cstdint>
#include <list>
#include <vector>

struct Packet
{
	struct Header* Header;
	uint8_t* Data;
	uint8_t HeaderSize;
	uint32_t DataSize;
};

class PacketManager
{
public:
	PacketManager(uint16_t maxPacketSize = 512);

	void SetMaxPacketSize(uint16_t maxPacketSize);

	std::vector<Packet> GeneratePacketsFromData(uint8_t* data, uint32_t dataSize, bool ordered = false, bool reliable = false);
	std::vector<Packet> GetPackets(uint32_t dataSize, bool ordered = false, bool reliable = false);
	void HandlePacket(Packet packet);

	std::vector<Packet> GetRecievedPackets();

private:
	friend struct Header;
	typedef uint32_t SequenceType;
	typedef uint32_t AckBacklogType;

	Packet GetPacket(uint32_t size, bool ordered, bool reliable, bool multi, bool resend);

	void* HandleHeader(struct Header* message);

	void UpdateRecievedSequence(SequenceType seq);
	void ClearAckedMessages(SequenceType lastAck, AckBacklogType ackBacklog);

	std::list<void*> m_PacketsWaitingForAck;

	struct PartialPacket
	{
		SequenceType previousSeq;
		Header* data;
	};
	std::list<PartialPacket> m_PacketParts;
	std::vector<Packet> m_ReadyPackets;

	SequenceType m_CurSeq = 0;
	SequenceType m_LastAckedSeq = 0;
	AckBacklogType m_AckBacklog = 0;
	uint16_t m_MaxPacketSize = 512;

};

