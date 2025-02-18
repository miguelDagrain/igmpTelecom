// Router with three interfaces
// The input/output configuration is as follows:
//
// Input:
//	[0]: packets received on the 192.168.1.0/24 network
//	[1]: packets received on the 192.168.2.0/24 network
//	[2]: packets received on the 192.168.3.0/24 network
//
// Output:
//	[0]: packets sent to the 192.168.1.0/24 network
//	[1]: packets sent to the 192.168.2.0/24 network
//	[2]: packets sent to the 192.168.3.0/24 network
//  [3]: packets destined for the router itself
require(library definitions.click)
elementclass Router {
	$server_address, $client1_address, $client2_address |
	server_arpq :: ARPQuerier($server_address);
	client1_arpq :: ARPQuerier($client1_address);
	client2_arpq :: ARPQuerier($client2_address); 
	//igmp stuff
	igmp::IGMPRouterMembershipHandler(224.0.0.1,$server_address,$client1_address,$client2_address,interval 20);
	igmp[0]->server_arpq;
	igmp[1]->client1_arpq;
	igmp[2]->client2_arpq;
	// Shared IP input path and routing table
	ip :: Strip(14)
		-> CheckIPHeader
		-> rt :: StaticIPLookup(
					$server_address:ip/32 0,
					$client1_address:ip/32 0,
					$client2_address:ip/32 0,
					$server_address:ipnet 1,
					$client1_address:ipnet 2,
					$client2_address:ipnet 3);
	
	// ARP responses are copied to each ARPQuerier and the host.
	arpt :: Tee (3);

	// Input and output paths for interface 0 all packets from the server enter here
	input[0]
		-> HostEtherFilter($server_address)
		// 0001 are arp requests, 0002 are arp replies, - are the others (mostly udp packages here)
		-> server_class :: Classifier(12/0806 20/0001, 12/0806 20/0002, -)
		-> ARPResponder($server_address)
		-> [0]output;

	server_arpq -> output;
	server_class[1] ->arpt[0] -> [1]server_arpq;
	//filter out correct udp packets and strip their ethernet header
	server_class[2]->serverudp::Classifier(12/0800 23/11,- )->CheckUDPHeader()->Strip(14)->[0]igmp;
	serverudp[1]->Paint(1) -> ip;


	// Input and output paths for interface all messages from network 1 comes here,these have the joins/leaves and udps
	input[1]
		-> HostEtherFilter($client1_address)
		// 0001 are arp requests, 0002 are arp replies, - are the others (udp mostly)
		-> client1_class :: Classifier(12/0806 20/0001, 12/0806 20/0002, -)
		-> ARPResponder($client1_address)
		-> [1]output;

	client1_arpq -> [1]output;
	client1_class[1] -> arpt[1] -> [1]client1_arpq;
	//take out igmp messages here to process them seperately
	client1_class[2]->client1igmp::Classifier(12/0800 22/0102 38/22,- )->Strip(14)->[1]igmp;
	client1igmp[1]-> Paint(2) -> ip;


	// Input and output paths for interface 2 all messages from network 1 comes here, joins ands leaves with udp
	input[2]
		-> HostEtherFilter($client2_address)
		-> client2_class :: Classifier(12/0806 20/0001, 12/0806 20/0002, -)
		-> ARPResponder($client2_address)
		-> [2]output;

	client2_arpq -> [2]output;
	client2_class[1] -> arpt[2] -> [1]client2_arpq;
	client2_class[2]->client2igmp::Classifier(12/0800 22/0102 38/22,- )->Strip(14)->[2]igmp;
	client2igmp[1] -> Paint(3) -> ip;


	// Local delivery
	rt[0]-> [3]output
	
	// Forwarding paths per interface
	rt[1]
		-> DropBroadcasts
		-> server_paint :: PaintTee(1)
		-> server_ipgw :: IPGWOptions($server_address)
		-> FixIPSrc($server_address)
		-> server_ttl :: DecIPTTL
		-> server_frag :: IPFragmenter(1500)
		-> server_arpq;
	
	server_paint[1] -> ICMPError($server_address, redirect, host) -> rt;
	server_ipgw[1]  -> ICMPError($server_address, parameterproblem) -> rt;
	server_ttl[1]   -> ICMPError($server_address, timeexceeded) -> rt;
	server_frag[1]  -> ICMPError($server_address, unreachable, needfrag) -> rt;


	rt[2]
		-> DropBroadcasts
		-> client1_paint :: PaintTee(2)
		-> client1_ipgw :: IPGWOptions($client1_address)
		-> FixIPSrc($client1_address)
		-> client1_ttl :: DecIPTTL
		-> client1_frag :: IPFragmenter(1500)
		-> client1_arpq;
	
	client1_paint[1] -> ICMPError($client1_address, redirect, host) -> rt;
	client1_ipgw[1]  -> ICMPError($client1_address, parameterproblem) -> rt;
	client1_ttl[1]   -> ICMPError($client1_address, timeexceeded) -> rt;
	client1_frag[1]  -> ICMPError($client1_address, unreachable, needfrag) -> rt;

	
	rt[3]
		-> DropBroadcasts
		-> client2_paint :: PaintTee(2)
		-> client2_ipgw :: IPGWOptions($client2_address)
		-> FixIPSrc($client2_address)
		-> client2_ttl :: DecIPTTL
		-> client2_frag :: IPFragmenter(1500)
		-> client2_arpq;
	
	client2_paint[1] -> ICMPError($client2_address, redirect, host) -> rt;
	client2_ipgw[1]  -> ICMPError($client2_address, parameterproblem) -> rt;
	client2_ttl[1]   -> ICMPError($client2_address, timeexceeded) -> rt;
	client2_frag[1]  -> ICMPError($client2_address, unreachable, needfrag) -> rt;
}
