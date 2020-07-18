// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// ! DO NOT CHANGE THIS FILE: Any changes will be removed prior to the project defense. !
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

require(library definitions.click)
require(library library/router.click)

router :: Router(router_server_network_address, router_client_network1_address, router_client_network2_address);

FromHost(tap1) -> [0]router[0] -> ToHost(tap1);
FromHost(tap2) -> [1]router[1] -> ToHost(tap2);
FromHost(tap3) -> [2]router[2] -> ToHost(tap3);
router[3] -> Discard;
