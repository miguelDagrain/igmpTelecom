// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// ! DO NOT CHANGE THIS FILE: Any changes will be removed prior to the project defense. !
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

require(library definitions.click)
require(library library/definitions.click)
require(library library/server.click)

multicast_server :: Server(multicast_server_address, router_server_network_address);

FromHost(tap0) -> multicast_server -> ToHost(tap0); 
multicast_server[1] -> Discard;

// Generate traffic for the multicast server.
RatedSource("data", 1, -1, true)
	-> DynamicUDPIPEncap(multicast_server_address:ip, 1234, multicast_client_address:ip, 1234) 
	// The MAC addresses here should be from the multicast_server to get past the HostEtherFilter. 
	// This way we can reuse the input from the network for the applications.
	-> EtherEncap(0x0800, multicast_server_address:eth, multicast_server_address:eth) 
	-> IPPrint("multicast_server -- transmitted a UDP packet")
	-> [0]multicast_server
