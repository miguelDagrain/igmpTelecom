// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// ! DO NOT CHANGE THIS FILE: Any changes will be removed prior to the project defense. !
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

server_network :: ListenEtherSwitch;
client_network1 :: ListenEtherSwitch;
client_network2 :: ListenEtherSwitch;

FromDevice(tap0) -> [0]server_network[0] -> Queue -> ToDevice(tap0);
FromDevice(tap1) -> [1]server_network[1] -> Queue -> ToDevice(tap1);

FromDevice(tap2) -> [0]client_network1[0] -> Queue -> ToDevice(tap2);
FromDevice(tap4) -> [1]client_network1[1] -> Queue -> ToDevice(tap4);
FromDevice(tap5) -> [2]client_network1[2] -> Queue -> ToDevice(tap5);

FromDevice(tap3) -> [0]client_network2[0] -> Queue -> ToDevice(tap3);
FromDevice(tap6) -> [1]client_network2[1] -> Queue -> ToDevice(tap6);
FromDevice(tap7) -> [2]client_network2[2] -> Queue -> ToDevice(tap7);

server_network[2]  -> ToDump("server_network.pcap");
client_network1[3] -> ToDump("client_network1.pcap");
client_network2[3] -> ToDump("client_network2.pcap");
