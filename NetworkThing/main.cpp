#include <iostream>

#include "PacketManager.h"

int main()
{
	char hw[] = "Hello world!";
	PacketManager pm;

	std::vector<Packet> packets = pm.GeneratePacketsFromData((uint8_t*)hw, sizeof(hw), true, true);

	while (packets.size())
	{
		size_t i = std::rand() % packets.size();
		pm.HandlePacket(packets[i]);
		packets.erase(packets.begin() + i);
	}

	std::vector<Packet> ps = pm.GetRecievedPackets();
	for (Packet& p : ps)
	{
		std::cout << p.Data << std::endl;
	}

	return 0;
}