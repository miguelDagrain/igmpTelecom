AddressInfo(sourceAddr 192.168.0.2 1A:7C:3E:90:78:41)
AddressInfo(responderAddr 224.0.0.1 1A:7C:3E:90:78:42)

MembershipQueryMessageGen(sourceAddr, responderAddr)
-> EtherEncap(2048, sourceAddr, responderAddr)
-> Print
-> ToDump(testMembership.pcap)
-> Discard;
