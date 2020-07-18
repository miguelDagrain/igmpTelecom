// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// ! DO NOT CHANGE THIS FILE: Any changes will be removed prior to the project defense. !
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

require(library definitions.click)
require(library library/client.click)

client21 :: Client(client21_address, router_client_network1_address);

FromHost(tap4) -> client21 -> ToHost(tap4); 

client21[1] -> IPPrint("client21 -- received a packet") -> Discard;
