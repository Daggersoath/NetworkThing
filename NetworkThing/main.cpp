#include <iostream>
#include <list>
#include <bitset>

struct Header
{
	uint32_t protocolId = 135866 + 1u; // protocol + ver
	uint8_t msgType : 2;
	uint32_t lastAck; 
	uint32_t ackBacklog;
};

struct ReliableHeader : Header
{
	uint32_t seq; // 30p/s = 1657 days runtime before reset needed
};

struct OrderedHeader : ReliableHeader
{
	uint32_t lastOrdered;
};


std::list<uint32_t> tl{0, 3, 6, 15, 28, 29, 30, 31, ~0u};
decltype(Header::lastAck) lack = 0;
decltype(Header::ackBacklog) abl = 0;



int main()
{
	Header x;

	x.lastAck = 12;
	x.ackBacklog = ~0u;

	ParseAck(x.lastAck, x.ackBacklog);

	for (uint8_t i = 1; i < 33; ++i)
	{
		PushSeq(i);
		std::cout << lack << " - " << abl << std::endl;;
	}

	return 0;
}