#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/timer.hh>
#include "../Packets/V3Membership.hh"
#include "../Packets/MembershipQuery.hh"
#include"../Helper/HelperFunc.hh"
#include "ClientInterface.hh"
CLICK_DECLS


ClientInterface::ClientInterface()
{}

ClientInterface::~ClientInterface()
{}

int ClientInterface::configure(Vector<String> &conf, ErrorHandler *errh) {
	if (Args(conf, this, errh)
	.read_mp("SRC", _srcIP)
    .read_mp("MA", _maIP)
    .read_mp("broadcast", igmpBroadcast)
	.complete() < 0){ return -1;}

    mode = MODE_IS_INCLUDE;
    currentJoinedGroup.s_addr = 0;
    wrongJoinedGroup.s_addr = 0;
    _sequence = 0;
	return 0;
}

const MembershipQueryMessage* reconstructQuery(const unsigned char * data)
{
    MembershipQueryMessage* query = new MembershipQueryMessage();
    query->type = *(reinterpret_cast<const unsigned *>(data));
    query->max_resp_code = *(reinterpret_cast<const unsigned *>(data+8));
    query->checksum = *(reinterpret_cast<const unsigned *>(data+16));
    query->group_address = *(reinterpret_cast<const unsigned *>(data+32));
    query->resv = *(reinterpret_cast<const unsigned *>(data+64));
    query->s = *(reinterpret_cast<const unsigned *>(data+68));
    query->qrv = *(reinterpret_cast<const unsigned *>(data+69));
    query->qqic = *(reinterpret_cast<const unsigned *>(data+72));
    query->numberOfSources = *(reinterpret_cast<const unsigned *>(data+80));
    return query;
}

void ClientInterface::push(int port, Packet* p)
{   
    //packet is an igmp message
    if(port==0){
        MembershipQuery query=MembershipQuery::readPacket(p->uniqueify());
        if (query.getType() != 0x11 || 
         (query.getGroupAddr() != 0 && query.getGroupAddr() != currentJoinedGroup)){
            p->kill();
            return;
        } 
        int robust=HelperFunc::deduceIntFromCode(query.getQRV());
        if(robust>7)robust=2;
        robustness=robust;

        if (this->mode == MODE_IS_EXCLUDE || this->mode == CHANGE_TO_EXCLUDE_MODE) {
            if(query.getReadIpDst()==igmpBroadcast||query.getReadIpDst()==this->currentJoinedGroup){
                int resptime=HelperFunc::deduceIntFromCode(query.getMaxResp())*100;
                this->mode = MODE_IS_EXCLUDE;
                Packet* q = makeGroupRecordPacket(0);
                sendRobustMembershipPacket(q,1,resptime);
            }
        }
        p->kill();
    //packet is an udp packet
    }else if(port==1){
        click_ip* ipheader=(click_ip*)p->data();
        in_addr dst=ipheader->ip_dst;
        if(this->mode == MODE_IS_EXCLUDE || this->mode == CHANGE_TO_EXCLUDE_MODE){
            if(dst==this->currentJoinedGroup){
                output(1).push(p); 
            }else{
                p->kill();
            }
        }else{
            p->kill();
        }
    }else{
        p->kill();
    }
}

void ClientInterface::expire(Packet* p,int sendsLeft){
    if (Packet *q = p->clone()) output(0).push(q);
    int interval=uri;
    mergedTimer = nullptr;
    sendRobustMembershipPacket(p,sendsLeft,interval*1000);
};

void ClientInterface::handleExpiry(Timer*, void * data){
    groupRecordTimerStruct * recordData = (groupRecordTimerStruct*) data;
    recordData->me->expire(recordData->membershipPacket,recordData->sendsLeft);
}

void ClientInterface::sendRobustMembershipPacket(Packet *p,int left,double maxresp){ 
    if(maxresp==-1){
        maxresp=0;
    }
    if(left>0){
        groupRecordTimerStruct* timerdata = new groupRecordTimerStruct();
        timerdata->membershipPacket=p;
        timerdata->sendsLeft=left-1;
        timerdata->me = this;
        Timer* refresh = new Timer(&ClientInterface::handleExpiry,timerdata);
        refresh->initialize(this);
        int interval=click_random(0,maxresp);
        refresh->schedule_after_msec(interval);
        if (mergedTimer){
            Timestamp minTime = (refresh->expiry_steady() < mergedTimer->expiry_steady())? 
                                refresh->expiry_steady(): mergedTimer->expiry_steady();
            refresh->reschedule_at(minTime);
            delete mergedTimer;
        }
        mergedTimer = refresh;
    }
}

int ClientInterface::join(const String &conf, Element *e, void* thunk, ErrorHandler * errh)
{
    click_chatter("\033[0;32mCalled join in client.\033[0m");
    ClientInterface* inter = reinterpret_cast<ClientInterface*>(e);
    Vector<String> vconf;
    cp_argvec(conf, vconf);
    Vector<IPAddress> join;
	if (!IPAddressArg().parse(conf, join)){return errh->error("syntax error");}
    
    if (inter->currentJoinedGroup.s_addr == 0){
        click_chatter("\033[0;34mWas not yet joined.\033[0m");

        inter->currentJoinedGroup  = join[0].in_addr();

        inter->mode = CHANGE_TO_EXCLUDE_MODE;

        Packet* q = inter->makeGroupRecordPacket(0);

        inter->sendRobustMembershipPacket(q,inter->getRobustness(),-1);
    }

    return 0;
}

int ClientInterface::leave(const String &conf, Element *e, void* thunk, ErrorHandler * errh)
{
    click_chatter("\033[0;32mCalled leave in client.\033[0m");
    ClientInterface* inter = reinterpret_cast<ClientInterface*>(e);
    Vector<String> vconf;
    cp_argvec(conf, vconf);
    Vector<IPAddress> leave;
	if (!IPAddressArg().parse(conf, leave)){return errh->error("syntax error");}
    
    if (inter->currentJoinedGroup == leave[0].in_addr()){
        click_chatter("\033[0;34mLeaving joined group.\033[0m");
        inter->mode = CHANGE_TO_INCLUDE_MODE;
        Packet* q = inter->makeGroupRecordPacket(0);

        inter->currentJoinedGroup.s_addr  = 0;

        inter->sendRobustMembershipPacket(q,inter->getRobustness(),-1);
    }else{
        click_chatter("\033[0;34mTried to leave an address that a client is not joined on.\033[0m");
    }
    return 0;
}

int ClientInterface::getRobustness(){
    return robustnessOverride==0?robustness:robustnessOverride;
}

void ClientInterface::add_handlers() {
    add_write_handler("join", &join, (void *)0);
    add_write_handler("leave", &leave, (void *)0);
    add_write_handler("robustness", &robustnessHandler, (void *)0);
    add_write_handler("unsolicited_report_interval", &unriHandler, (void *)0);
    add_write_handler("set_igmp_type", &igmpTypeHandler, (void *)0);
    add_write_handler("set_igmp_address", &igmpAddressHandler, (void *)0);
    add_write_handler("set_igmp_group_record_type", &groupRecordTypeHandler, (void *)0);
    add_write_handler("invalid_igmp_checksum", &checksumHandler, (void *)0);
}

int ClientInterface::robustnessHandler(const String &conf, Element *e, void* thunk, ErrorHandler * errh){
    click_chatter("\033[0;32mCalled robustness handler in client.\033[0m");
    ClientInterface* inter = reinterpret_cast<ClientInterface*>(e);
    Vector<String> vconf;
    cp_argvec(conf, vconf);

    int newRobust=0;
    if (Args(vconf, inter, errh)
	.read_mp("REPORT", newRobust)
    .complete() < 0){ return -1;}
    
    inter->setRobustness(newRobust);

    return 0;
}

int ClientInterface::unriHandler(const String &conf, Element *e, void* thunk, ErrorHandler * errh){
    click_chatter("\033[0;32mCalled unsolicited report interval handler in client.\033[0m");
    ClientInterface* inter = reinterpret_cast<ClientInterface*>(e);
    Vector<String> vconf;
    cp_argvec(conf, vconf);

    int newUnri=0;
    if (Args(vconf, inter, errh)
	.read_mp("UNSOLICITED_REPORT_INTERVAL", newUnri)
    .complete() < 0){ return -1;}

    inter->setUnsolicitedReportInterval(newUnri);

    return 0;
}

int ClientInterface::igmpTypeHandler(const String &conf, Element *e, void* thunk, ErrorHandler * errh){
    click_chatter("\033[0;32mCalled set igmp type handler in client.\033[0m");
    ClientInterface* inter = reinterpret_cast<ClientInterface*>(e);
    Vector<String> vconf;
    cp_argvec(conf, vconf);

    int newtype=0;
    if (Args(vconf, inter, errh)
	.read_mp("REPORT", newtype)
    .complete() < 0){ return -1;}
    
    inter->setIgmpType(newtype);

    return 0;
}

int ClientInterface::igmpAddressHandler(const String &conf, Element *e, void* thunk, ErrorHandler * errh){
    click_chatter("\033[0;32mCalled set igmp address handler in client.\033[0m");
    ClientInterface* inter = reinterpret_cast<ClientInterface*>(e);
    Vector<String> vconf;
    cp_argvec(conf, vconf);
    
    in_addr newIGMPAddress;

    if (Args(vconf, inter, errh)
	.read_mp("REPORT", newIGMPAddress)
    .complete() < 0){ return -1;}
	click_chatter("\033[0;32mChanging the igmp address for all reports of client\033[0m");


    inter->wrongJoinedGroup = newIGMPAddress;

    return 0;
}

int ClientInterface::groupRecordTypeHandler(const String &conf, Element *e, void* thunk, ErrorHandler * errh){
    click_chatter("\033[0;32mCalled set igmp group record type handler in client.\033[0m");
    ClientInterface* inter = reinterpret_cast<ClientInterface*>(e);
    Vector<String> vconf;
    cp_argvec(conf, vconf);

    int newtype=0;
    if (Args(vconf, inter, errh)
	.read_mp("REPORT", newtype)
    .complete() < 0){ return -1;}
    
    inter->setGroupRecordType(newtype);

    return 0;
}

int ClientInterface::checksumHandler(const String &conf, Element *e, void* thunk, ErrorHandler * errh){
    click_chatter("\033[0;32mCalled invalid igmp checksum handler in client.\033[0m");
    ClientInterface* inter = reinterpret_cast<ClientInterface*>(e);
    Vector<String> vconf;
    cp_argvec(conf, vconf);

    bool checksumInCorrect=false;
    if (Args(vconf, inter, errh)
	.read_mp("REPORT", checksumInCorrect)
    .complete() < 0){ return -1;}
    
    inter->setInvalidChecksum(checksumInCorrect);

    return 0;
}

Packet* ClientInterface::makeGroupRecordPacket(WritablePacket *q)
{
    V3Membership packet;
    Vector<in_addr> vec1=Vector<in_addr>();
    in_addr sendedAddr;
    if (wrongJoinedGroup.s_addr == 0) {
        sendedAddr = currentJoinedGroup;
    } else {
        click_chatter("\033[0;34mSetting the address of report wrong\033[0m");
        sendedAddr = wrongJoinedGroup;
    }

    if(groupRecordType==0){
        packet.addGroupRecord(sendedAddr,vec1, this->mode);
    }else{
        click_chatter("\033[0;34mSetting the group record type wrong\033[0m");
        packet.addGroupRecord(sendedAddr,vec1, groupRecordType);
    }
    packet.setChecksumCorrect(!invalidChecksum);
    if(igmpType==0){
        q=packet.addToPacket(0,_srcIP,_maIP,_sequence);
    }else{
        click_chatter("\033[0;34mSetting type of igmp report wrong\033[0m");
        q=packet.addToPacket(0,_srcIP,_maIP,_sequence,igmpType);
    }
    _sequence++;
    
    q->timestamp_anno().assign_now();
    
	return q;
}

void ClientInterface::setRobustness(int newValue){
    if(newValue!=0){
        click_chatter("\033[0;32mOverriding robustness of a client with %d\033[0m",newValue);
    }else{
        click_chatter("\033[0;32mThe igmp protocol will now regain control of the robustness for a certain client\033[0m");
    }
    robustnessOverride=newValue;
}

void ClientInterface::setUnsolicitedReportInterval(int newValue){
    click_chatter("\033[0;32mOverriding the Unsolicited Report Interval of a client with %d\033[0m",newValue);
    
    uri=newValue;
}

void ClientInterface::setIgmpType(int newValue){
    click_chatter("\033[0;32mSetting the igmpType of all reports in a client to %d\033[0m",newValue);
    igmpType=newValue;
}

void ClientInterface::setIgmpAddress(in_addr newValue){
    click_chatter("\033[0;32mChanging the igmpAddres of a certain client\033[0m");
    if(currentJoinedGroup!=0){
        currentJoinedGroup=newValue;
    }
}

void ClientInterface::setGroupRecordType(int newValue){
    click_chatter("\033[0;32mSetting the group record type of all reports in a client to %d\033[0m",newValue);
    groupRecordType=newValue;
}

void ClientInterface::setInvalidChecksum(bool newValue){
    if(newValue){
        click_chatter("\033[0;32mA client will now start sending packets with a wrong checksum\033[0m");
    }else{
        click_chatter("\033[0;32mA client will now stop sending packets with a wrong checksum\033[0m");
    }
    invalidChecksum=newValue;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ClientInterface)
