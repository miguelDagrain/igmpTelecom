#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <unistd.h>
#include <string>
#include "../Helper/HelperFunc.hh"
#include "../Packets/V3Membership.hh"
#include "../Packets/MembershipQuery.hh"
#include "IGMPRouterMembershipHandler.hh"

CLICK_DECLS
	InterfaceReceptionState::InterfaceReceptionState(): groupTimer(this) {
	}

	InterfaceReceptionState::InterfaceReceptionState(IGMPRouterMembershipHandler* router): groupTimer(this) {
		owningRouter = router;
	}


	int InterfaceReceptionState::configure(Vector<String>&, ErrorHandler*) {
		groupTimer.initialize(this);
		return 0;
	}

	void InterfaceReceptionState::run_timer(Timer*) {
		//we change filtermode to include in correspondence with 6.5.
		filterMode = true;

		//however the previous was quite useless as we need to delete this element
		//there namely is never a source record specified as the list is always empty
		owningRouter->removeInterface(this);
	}

CLICK_ENDDECLS
EXPORT_ELEMENT(InterfaceReceptionState)


CLICK_DECLS
IGMPRouterMembershipHandler::IGMPRouterMembershipHandler(): queryIntervalTimer(this)
{
	robustnessVariable= 2;
	querierInterval = 125;
	queryResponseInterval = 100;
	startupQueryCount = robustnessVariable;
	//for values larger then 128 there is no guarantee that the exact value will be representable.
	lastMemberQueryInterval = 10; 
	lastMemberQueryCount = robustnessVariable;

	typeQueries = 0x11;
	multicastAddress.s_addr = 0;
	wrongAddress.s_addr = 0;
	checksumValid = true;
}

IGMPRouterMembershipHandler::~ IGMPRouterMembershipHandler()
{
	for(Vector<InterfaceReceptionState*>::iterator interface = this->interfaces.begin(); interface != this->interfaces.end(); ){
		interfaces.erase(interface);
	}
}

in_addr IGMPRouterMembershipHandler::getn1(){
	return network1;
}

in_addr IGMPRouterMembershipHandler::getn2(){
	return network2;
}

in_addr IGMPRouterMembershipHandler::getserver(){
	return server;
}

in_addr IGMPRouterMembershipHandler::getdst(){
	return _dstIP;
}

int IGMPRouterMembershipHandler::getseq(){
	return _sequence;
}

void IGMPRouterMembershipHandler::incrseq(){
	_sequence++;
}

void IGMPRouterMembershipHandler::removeInterface(InterfaceReceptionState* interface){
	Vector<InterfaceReceptionState*>::iterator removedPosition = NULL;
	for(Vector<InterfaceReceptionState*>::iterator recordIndex = interfaces.begin(); recordIndex != interfaces.end(); recordIndex++){
		if(interface == (*recordIndex)){
			removedPosition = recordIndex;
		}
	}
	if(removedPosition != NULL){
		interfaces.erase(removedPosition);
	} else {
		click_chatter("\033[0;30mNo record was recognized by the router for removal.\033[0m");
	}
}

int IGMPRouterMembershipHandler::configure(Vector<String> &conf, ErrorHandler *errh) {
	if (Args(conf, this, errh)
	.read_mp("DST", _dstIP)
	.read_mp("server", server)
	.read_mp("N1", network1)
	.read_mp("N2", network2)
	.read("interval", querierInterval)
    .complete() < 0){ return -1;}
	

	queryIntervalTimer.initialize(this);


	//Setting timer first to startup query interval, after this the timer will just be query interval
	startUpQueryTimerData* data = new startUpQueryTimerData();
	data->me = this;
	data->count = startupQueryCount; 

	Timer* startupQueryTimer = new Timer(&IGMPRouterMembershipHandler::handleExpirySQT, data);
	startupQueryTimer->initialize(this);
	//Though it doesn't seem correct we can't call the sendquery in configure so we schedule it with a small time
	//If we were to not do this the first queries would be lost due to the router not finding the host is needs to send them to
	startupQueryTimer->schedule_after_msec(200);

	return 0;
}

void IGMPRouterMembershipHandler::sendGeneralQuery(){
	MembershipQuery generalQuery=makeGeneralQuery();
	if (!checksumValid){
		click_chatter("\033[0;34mSetting checksum to wrong.\033[0m");
	}
	sendQuery(1,generalQuery.addToPacket(0,getn1(),getdst(),getseq(), checksumValid));
	incrseq();
	sendQuery(2,generalQuery.addToPacket(0,getn2(),getdst(),getseq(), checksumValid));
	incrseq();
	sendQuery(0,generalQuery.addToPacket(0,getserver(),getdst(),getseq(), checksumValid));
	incrseq();

}

void IGMPRouterMembershipHandler::run_timer(Timer* t){
	sendGeneralQuery();
	queryIntervalTimer.schedule_after_s(HelperFunc::deduceIntFromCode((uint8_t) querierInterval ));
}

void IGMPRouterMembershipHandler::handleExpirySQT(Timer* t, void * counter){
	startUpQueryTimerData* data = reinterpret_cast<startUpQueryTimerData *>(counter);

	data->me->sendGeneralQuery();
	data->count--;
	if (data->count == 0){
		data->me->queryIntervalTimer.schedule_after_sec((int)(data->me->querierInterval));

		delete data;
	}else{
		//reset timer
		Timer* newStartupQueryTimer = new Timer(&IGMPRouterMembershipHandler::handleExpirySQT, data);
		newStartupQueryTimer->initialize(data->me);
		newStartupQueryTimer->schedule_after_sec((int)((HelperFunc::deduceIntFromCode((uint8_t) data->me->querierInterval))/4));
	}

}

void IGMPRouterMembershipHandler::expireLMQT(InterfaceReceptionState* receptionState, bool cancelled){
	//we search the correct interface
	Vector<lastMemberQueryTimerData*>::iterator toRemove = NULL;
	for(Vector<lastMemberQueryTimerData*>::iterator iter = lmqtDataVec.begin(); iter != lmqtDataVec.end(); iter++){
		if ((*iter)->receptionState == receptionState){
			toRemove= iter;
			break;
		}
	}

	if(cancelled)
	//if cancelled we don't remove the receptionstate
	{}else{
		//though we never calculated the last member query time a simple rundown of the algorithm is enough to see that amount of time has now passed:
		//last member query count * last member query interval
		receptionState->filterMode = true;
		delete (*toRemove)->receptionState;
	}

	//we do anyhow need to remove the the receptionstate from the lmqt data vector
	lmqtDataVec.erase(toRemove);
}

void IGMPRouterMembershipHandler::handleExpiryLastMemberQueryTimer(Timer * t, void * counter) {
	lastMemberQueryTimerData* data = reinterpret_cast<lastMemberQueryTimerData *>(counter);
	data->count--;
	if (data->count == 0){
		IGMPRouterMembershipHandler* mePtr = data->me; 
		mePtr->expireLMQT(data->receptionState, data->cancelled);
	}else{
		//send query
		MembershipQuery change = data->me->makeGroupSpecificQuery(data->address.s_addr);

		uint32_t temp=htonl(255);
		uint32_t srcint=data->srcInt;
		uint32_t test=temp|srcint;
		test-=htonl(1);
		in_addr broadcast=in_addr();
		broadcast.s_addr=test;					
		WritablePacket * changepacket=change.addToPacket(0,broadcast,data->address,0, data->me->checksumValid);

		data->me->output(data->receptionState->interface).push(changepacket);

		//reset timer
		Timer* newLastMemberQueryTimer = new Timer(&IGMPRouterMembershipHandler::handleExpiryLastMemberQueryTimer, data);
		newLastMemberQueryTimer->initialize(data->me);
		newLastMemberQueryTimer->schedule_after_msec(data->me->lastMemberQueryInterval*100);
	}
}

void IGMPRouterMembershipHandler::push(int port, Packet *p){
	//if the port is 0 it is a udp packet from the server and should be distributed correctly
	if(port ==0){
		handleUDPPacket(port,p);
		return;
	}else{
		//if not it is an igmp message and you should update
		WritablePacket *q=p->uniqueify();
		V3Membership mem=V3Membership::readPacket(q);
		if(mem.isChecksumCorrect() && mem.getType() == 0x22){
			click_ip* ip=(click_ip*)q->data();
			handleMembershipReport(ip->ip_src,port,&mem);
		}else {
			p->kill();
			return;
		}
	}
}

void IGMPRouterMembershipHandler::handleUDPPacket(int interface,Packet* p){
	WritablePacket* q=p->uniqueify();
	click_ip* iphOfUdp=(click_ip*)q->data();
	igmp_udp* udp=(igmp_udp*)iphOfUdp+1;

	//check ip checksum
	int checksumBefore=iphOfUdp->ip_sum;
    //iphOfUdp->ip_sum=0;
    uint16_t checksum= click_in_cksum((unsigned char *) iphOfUdp, sizeof(click_ip));
    if(checksum!=0){
        click_chatter("\033[0;30mChecksum of ip incorrect\033[0m");
        return;
    }
    iphOfUdp->ip_sum=checksumBefore;
	

	//check udp checksum
	//as this calculation wasnt as the ip header, and was quite difficult to implement i used a click element in the router click script

	for(InterfaceReceptionState* i : interfaces){
		//we check if multicast is ok, and if the filtermode is exclude. If exclude it should forward packets as there is never a list of sources.
		if(i->multicast==iphOfUdp->ip_dst&& !i->filterMode){
			if (Packet *q = p->clone())output(i->interface).push(q);
		}
	}
	p->kill();
}

void IGMPRouterMembershipHandler::sendQuery(int port, Packet *p){
     output(port).push(p);
}

MembershipQuery IGMPRouterMembershipHandler::makeGeneralQuery(){
	
	MembershipQuery query;

	if (typeQueries != 0x11) {
		click_chatter("\033[0;34mSetting type of query wrong\033[0m");
	}
	query.setType(typeQueries);
	if (wrongAddress.s_addr == 0){
		query.setGroupAddr(multicastAddress.s_addr); //normally this is 0.0.0.0
	} else {
		click_chatter("\033[0;34mSetting wrong address general query\033[0m");
		query.setGroupAddr(wrongAddress.s_addr);
	}
	query.setMaxResp(queryResponseInterval);
	query.setSFlag(false);
	query.setQRV(robustnessVariable);
	query.setQQIC(querierInterval);
	return query;
}

MembershipQuery IGMPRouterMembershipHandler::makeGroupSpecificQuery(uint32_t group) {
	
	MembershipQuery query;
	
	query.setType(typeQueries);
	query.setMaxResp(lastMemberQueryInterval);
	query.setSFlag(false);
	if (wrongAddress.s_addr == 0){
		query.setGroupAddr(group);
	} else {
		click_chatter("\033[0;34mSetting wrong address group specific query\033[0m");
		query.setGroupAddr(wrongAddress.s_addr);
	}
	query.setQQIC(querierInterval);
	query.setQRV(robustnessVariable);

	return query;
}

int IGMPRouterMembershipHandler::sendQuery(const String &conf, Element *e,void * thunk, ErrorHandler * errh){
	click_chatter("\033[0;32mCalled send query in router.\033[0m");
	IGMPRouterMembershipHandler * me = (IGMPRouterMembershipHandler *) e;
	MembershipQuery query= me->makeGeneralQuery();

	me->sendQuery(0,query.addToPacket(0,me->getserver(),me->getdst(),me->getseq(), me->checksumValid));
	me->incrseq();
	me->sendQuery(1,query.addToPacket(0,me->getn1(),me->getdst(),me->getseq(), me->checksumValid));
	me->incrseq();
	me->sendQuery(2,query.addToPacket(0,me->getn2(),me->getdst(),me->getseq(), me->checksumValid));
	me->incrseq();
	return 0;
} 

int IGMPRouterMembershipHandler::setRobustness(const String &conf, Element *e, void *thunk, ErrorHandler *errh){
	click_chatter("\033[0;32mCalled set robustness in router.\033[0m");
	IGMPRouterMembershipHandler* routerPtr = reinterpret_cast<IGMPRouterMembershipHandler*>(e);
	Vector<String> vconf;
    cp_argvec(conf, vconf);

	int newRobustness;

	if (Args(vconf, routerPtr, errh)
	.read_mp("ROBUSTNESS", newRobustness)
    .complete() < 0){ return -1;}
	click_chatter("\033[0;32mSetting robustness to %d\033[0m",newRobustness);

	routerPtr->robustnessVariable = newRobustness;
	//we also need to update the lastmemberquerycount to be up to date with the robustnessvariable
	routerPtr->lastMemberQueryCount = newRobustness;
	//same for startupquerycount, though it isn't really usefull
	routerPtr->startupQueryCount = newRobustness;
}

int IGMPRouterMembershipHandler::setQueryInterval(const String &conf, Element *e, void *thunk, ErrorHandler *errh){
	click_chatter("\033[0;32mCalled set query interval in router.\033[0m");
	IGMPRouterMembershipHandler* routerPtr = reinterpret_cast<IGMPRouterMembershipHandler*>(e);
	Vector<String> vconf;
    cp_argvec(conf, vconf);

	int newQueryInterval;

	if (Args(vconf, routerPtr, errh)
	.read_mp("QUERY_INTERVAL", newQueryInterval)
    .complete() < 0){ return -1;}
	click_chatter("\033[0;32mSetting query interval to %d\033[0m",newQueryInterval);
	if (routerPtr->queryResponseInterval > newQueryInterval){
		routerPtr->queryResponseInterval = newQueryInterval*10;
	}
	routerPtr->querierInterval = newQueryInterval;
}

int IGMPRouterMembershipHandler::setMaxResponseTime(const String &conf, Element *e, void *thunk, ErrorHandler *errh){
	click_chatter("\033[0;32mCalled set max response time in router.\033[0m");
	IGMPRouterMembershipHandler* routerPtr = reinterpret_cast<IGMPRouterMembershipHandler*>(e);
	Vector<String> vconf;
    cp_argvec(conf, vconf);

	int newMaxResponseTime;

	if (Args(vconf, routerPtr, errh)
	.read_mp("MAX_RESPONSE_TIME", newMaxResponseTime)
    .complete() < 0){ return -1;}
	
	//we multiply by 10 because newMaxResponseTime is in deciseconds
	if (newMaxResponseTime > routerPtr->querierInterval*10){
		return errh->error("max response time larger then querier interval.");
	}
	click_chatter("\033[0;34Setting max response time to %d\033[0m",newMaxResponseTime);
	routerPtr->queryResponseInterval = newMaxResponseTime;
}

int IGMPRouterMembershipHandler::setLastMemberQueryInterval(const String &conf, Element *e, void *thunk, ErrorHandler *errh){
	click_chatter("\033[0;32mCalled set last member query interval in router.\033[0m");
	IGMPRouterMembershipHandler* routerPtr = reinterpret_cast<IGMPRouterMembershipHandler*>(e);
	Vector<String> vconf;
    cp_argvec(conf, vconf);

	int newLastMemberQueryInterval;

	if (Args(vconf, routerPtr, errh)
	.read_mp("LAST_MEMBER_QUERY_INTERVAL", newLastMemberQueryInterval)
    .complete() < 0){ return -1;}
	click_chatter("\033[0;32mSetting last member query interval to %d\033[0m",newLastMemberQueryInterval);

	routerPtr->lastMemberQueryInterval = newLastMemberQueryInterval;
}

int IGMPRouterMembershipHandler::setLastMemberQueryCount(const String &conf, Element *e, void *thunk, ErrorHandler *errh){
	click_chatter("\033[0;32mCalled set last member query interval in router.\033[0m");
	IGMPRouterMembershipHandler* routerPtr = reinterpret_cast<IGMPRouterMembershipHandler*>(e);
	Vector<String> vconf;
    cp_argvec(conf, vconf);

	int newLastMemberQueryCount;

	if (Args(vconf, routerPtr, errh)
	.read_mp("LAST_MEMBER_QUERY_COUNT", newLastMemberQueryCount)
    .complete() < 0){ return -1;}
	click_chatter("\033[0;32mSetting last member query count to %d\033[0m",newLastMemberQueryCount);

	routerPtr->lastMemberQueryCount = newLastMemberQueryCount;
}

int IGMPRouterMembershipHandler::setIGMPType(const String &conf, Element *e, void *thunk, ErrorHandler *errh){
	click_chatter("\033[0;32mCalled set igmp type in router.\033[0m");
	IGMPRouterMembershipHandler* routerPtr = reinterpret_cast<IGMPRouterMembershipHandler*>(e);
	Vector<String> vconf;
    cp_argvec(conf, vconf);

	int newIGMPType;

	if (Args(vconf, routerPtr, errh)
	.read_mp("QUERY", newIGMPType)
    .complete() < 0){ return -1;}
	click_chatter("\033[0;32mSetting igmp type to %d\033[0m",newIGMPType);

	routerPtr->typeQueries = newIGMPType;
}

int IGMPRouterMembershipHandler::setIGMPAddress(const String &conf, Element *e, void *thunk, ErrorHandler *errh){
	click_chatter("\033[0;32mCalled set igmp address in router.\033[0m");
	IGMPRouterMembershipHandler* routerPtr = reinterpret_cast<IGMPRouterMembershipHandler*>(e);
	Vector<String> vconf;
    cp_argvec(conf, vconf);

	in_addr newIGMPAddress;

	if (Args(vconf, routerPtr, errh)
	.read_mp("QUERY", newIGMPAddress)
    .complete() < 0){ return -1;}
	click_chatter("\033[0;32mChanging the igmp address for all queries\033[0m");

	routerPtr->wrongAddress = newIGMPAddress;
}

int IGMPRouterMembershipHandler::setIGMPCheckSum(const String &conf, Element *e, void *thunk, ErrorHandler *errh){
	click_chatter("\033[0;32mCalled set igmp checksum in router.\033[0m");
	IGMPRouterMembershipHandler* routerPtr = reinterpret_cast<IGMPRouterMembershipHandler*>(e);
	Vector<String> vconf;
    cp_argvec(conf, vconf);

	bool validOrFalse;

	if (Args(vconf, routerPtr, errh)
	.read_mp("QUERY", validOrFalse)
    .complete() < 0){ return -1;}
	if(validOrFalse){
		click_chatter("\033[0;32mThe router will now start sending wrong checksums\033[0m");
	}else{
		click_chatter("\033[0;32mThe router will now start sending correct checksums\033[0m");
	}

	routerPtr->checksumValid = !validOrFalse; 
}

void IGMPRouterMembershipHandler::add_handlers(){
	add_write_handler("sendQuery", &sendQuery, (void *)0);
	add_write_handler("robustness", &setRobustness, (void *)0);
	add_write_handler("query_interval", &setQueryInterval, (void *)0);
	add_write_handler("max_response_time", &setMaxResponseTime, (void *)0);
	add_write_handler("last_member_query_interval", &setLastMemberQueryInterval, (void *)0);
	add_write_handler("last_member_query_count", &setLastMemberQueryCount, (void *)0);
	add_write_handler("set_igmp_type", &setIGMPType, (void *)0);
	add_write_handler("set_igmp_address", &setIGMPAddress,(void *)0);
	add_write_handler("invalid_igmp_checksum", &setIGMPCheckSum,(void *)0);
}

//long function that analyses a membershipreport
void IGMPRouterMembershipHandler::handleMembershipReport(in_addr src,int interface,V3Membership* mem){
	
	for(int recordNumber=0;recordNumber<mem->getRecords().size();recordNumber++){
		GroupRecord rec=mem->getRecords()[recordNumber];
		bool found=false;
		//check if the interface/multicast addres combo already existed
		for(InterfaceReceptionState* i : interfaces){
			if(i->interface==interface&&i->multicast==rec.getMulticast()){
				found=true;
				//change to include mode
				if(rec.getRecordType()==3 &&!i->filterMode){
					//we check if there is already a lmqtData being used.
					bool exist = false;
					for(lastMemberQueryTimerData* iter:lmqtDataVec){
						if(iter->receptionState == i){
							exist = true;
							break;
						}
					}
					if(!exist){
						//we do not prune the group now because we first want to be sure there are no other interested members. See rfc 6.4.2 paragraph 4

						//send groupspecific query on this interface

						//this list is always empty
						i->sourceList = rec.getSources();
						//send change report
						MembershipQuery change = makeGroupSpecificQuery(rec.getMulticast().s_addr);
				
						//see here a pretty annoying way to set the last byte of the address to 254
						uint32_t temp=htonl(255);
						uint32_t srcint=src.s_addr;
						uint32_t test=temp|srcint;
						test-=htonl(1);
						in_addr broadcast=in_addr();
						broadcast.s_addr=test;			
						WritablePacket * changepacket=change.addToPacket(0,broadcast,rec.getMulticast(),0, checksumValid);

						output(interface).push(changepacket);

						//set group
						int groupMembershipInterval = ((robustnessVariable) * HelperFunc::deduceIntFromCode(querierInterval)) + (queryResponseInterval/10);
						i->groupTimer.schedule_after_sec(groupMembershipInterval);

						lastMemberQueryTimerData* lmqtData = new lastMemberQueryTimerData();
						lmqtData->me = this;
						lmqtData->count = lastMemberQueryCount;
						lmqtData->address = rec.getMulticast();
						lmqtData->receptionState = i;
						lmqtData->srcInt = src.s_addr;
						lmqtData->cancelled = false;
						Timer* newLastMemberQueryTimer = new Timer(&IGMPRouterMembershipHandler::handleExpiryLastMemberQueryTimer, lmqtData);
						newLastMemberQueryTimer->initialize(lmqtData->me);
						newLastMemberQueryTimer->schedule_after_msec(lastMemberQueryInterval*100);
						lmqtDataVec.push_back(lmqtData);

						break;
					}
				}
				//change to exclude mode
				else if(rec.getRecordType()==4 &&i->filterMode){
					i->filterMode=false;

					//this list is always empty
					i->sourceList =  rec.getSources();

					//here we use the groupMembeshipInterval but it gets calculated from other values
					//we multiply the robustness variable with deduced querier interval (variable querierInterval is actually QIC), the queryResponseInterval
					//is divided by 10 because the variable queryResponseInterval is in deciseconds and we want to calculate a value that is in seconds.
					int groupMembershipInterval = ((robustnessVariable) * HelperFunc::deduceIntFromCode(querierInterval)) + (queryResponseInterval/10);
					i->groupTimer.schedule_after_sec(groupMembershipInterval);

					break;
				}
				//type is already include or exclude so these are the Current-State Records
				else if(rec.getRecordType()==1||rec.getRecordType()==2){
					//this list is always empty
					i->sourceList = rec.getSources();

					//quite an ugly method to see if we are on a broadcast address as src
					uint32_t srcnormal=htonl(src.s_addr);
					uint32_t final=srcnormal%256;
        
					//if final is 1 or 2 then we have 254 or 255at the end
					if(final==1||final==2){
						i->filterMode=false;
					}

					if (rec.getRecordType()==1){
						break;
					} else {

						//if exclude then we reset the grouptimer and check if there is 
						//a last member query interval timer
						int groupMembershipInterval = ((robustnessVariable) * HelperFunc::deduceIntFromCode(querierInterval)) + (queryResponseInterval/10);
						i->groupTimer.schedule_after_sec(groupMembershipInterval);

						for(lastMemberQueryTimerData* iter:lmqtDataVec){
							if(iter->receptionState == i){
								iter->cancelled = true;
								break;
							}
						}
					}

					break;
				}
				//anything else must not be made
			}
		}
		//it didnt exist yet in the list so we need to make a new receptionState
		if(!found){
			if (rec.getRecordType()==1 || rec.getRecordType()==2){
				continue;
			}
			InterfaceReceptionState* state = new InterfaceReceptionState(this);
			state->groupTimer.initialize(this);
			state->interface=interface;
			if(rec.getRecordType()==4){
				state->filterMode=false;
				int groupMembershipInterval = ((robustnessVariable) * HelperFunc::deduceIntFromCode(querierInterval)) + (queryResponseInterval/10);
				state->groupTimer.schedule_after_sec(groupMembershipInterval);
			}
			if(rec.getRecordType()==3){
				state->filterMode=true;
				state->groupTimer.schedule_after_msec((uint32_t)(lastMemberQueryInterval*100));
			}

			state->sourceList = rec.getSources();

			state->multicast=rec.getMulticast();
			interfaces.push_back(state);
		}
	}

}

CLICK_ENDDECLS
EXPORT_ELEMENT(IGMPRouterMembershipHandler)
