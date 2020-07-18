// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// ! DO NOT CHANGE THIS FILE: Any changes will be removed prior to the project defense. !
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

require(library definitions.click)
require(library library/client.click)

client22 :: Client(client22_address, router_client_network1_address);

FromHost(tap5) -> client22 -> ToHost(tap5);

client22[1] -> IPPrint("client22 -- received a packet") -> Discard;
