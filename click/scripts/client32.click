// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// ! DO NOT CHANGE THIS FILE: Any changes will be removed prior to the project defense. !
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

require(library definitions.click)
require(library library/client.click)

client32 :: Client(client32_address, router_client_network2_address);

FromHost(tap7) -> client32 -> ToHost(tap7); 

client32[1] -> IPPrint("client32 -- received a packet") -> Discard;
