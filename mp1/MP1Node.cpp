/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
	/*
	 * This function is partially implemented and may require changes
	 */
	int id = *(int*)(&memberNode->addr.addr);
	int port = *(short*)(&memberNode->addr.addr[4]);

	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;
    // node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode);

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
	MessageHdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if ( 0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
    }
    else {
        msg=new MessageHdr();
        msg->msgType=JOINREQ;
        msg->addr=&memberNode->addr;
        //msg copy all the memberNode info
        msg->member_vector=memberNode->memberList;
#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, sizeof(MessageHdr));
        free(msg);
    }

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode(){
   /*
    * Your code goes here
    */
    
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
    	return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
    	return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;
    	size = memberNode->mp1q.front().size;
    	memberNode->mp1q.pop(); 
    	recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}

/**
 * @brief 遍历判断Memberlist内是否有对应的id:port
 * 
 * @param id 
 * @param port 
 */
MemberListEntry* MP1Node::checkMemberList(int id,int port){
    for(int i=0;i<memberNode->memberList.size();++i){
        MemberListEntry item=memberNode->memberList[i];
        if(item.id==id&&item.port==port){
            return &memberNode->memberList[i];
        }
    }
    return nullptr;
}


/**
 * @brief search for address 
 * 
 * @param id 
 * @param port 
 */
MemberListEntry* MP1Node::checkMemberList(Address* addr){
    int id=0;
    short port;
    memcpy(&id,&(addr->addr[0]),sizeof(int));
    memcpy(&port,&(addr->addr[4]),sizeof(short));
    for(int i=0;i<memberNode->memberList.size();++i){
        MemberListEntry item=memberNode->memberList[i];
        if(item.id==id&&item.port==port){
            return &memberNode->memberList[i];
        }
    }
    return nullptr;
}

void MP1Node::insertMemberList(MessageHdr *msg){
    int id=0;
    short port;
    memcpy(&id,&(msg->addr->addr[0]),sizeof(int));
    memcpy(&port,&(msg->addr->addr[4]),sizeof(short));
    long hearbeat=1;
    long timestamp=this->par->getcurrtime();
    if(nullptr!=checkMemberList(id,port)) return;
    memberNode->memberList.emplace_back(id,port,hearbeat,timestamp);
    Address* addr=getAddress(id,port);
    log->logNodeAdd(&memberNode->addr,addr);
    delete addr;
}


void MP1Node::insertMemberList(MemberListEntry* node){
    Address *addr=getAddress(node->id,node->port);
    if(*addr==memberNode->addr){
        delete addr;
        return;
    }
    if(par->getcurrtime()-node->gettimestamp()<TREMOVE){
        log->logNodeAdd(&memberNode->addr,addr);
        MemberListEntry new_entry = *node;
        memberNode->memberList.push_back(new_entry);
    }
    delete addr;
}

/**
 * @brief 
 * 
 * @param addr dst_addr
 * @param msgType 
 */
void MP1Node::send_message(Address *addr,MsgTypes msgType){
    MessageHdr *reply_msg=new MessageHdr();
    reply_msg->msgType=msgType;
    reply_msg->addr=&memberNode->addr;
    // memcpy(&(reply_msg->addr),&(addr),sizeof(reply_msg->addr));
    // vector copy
    reply_msg->member_vector = memberNode->memberList;
    emulNet->ENsend(&(memberNode->addr),addr,(char*) reply_msg,sizeof(MessageHdr));
}


/**
 * @brief update the SrcMember heartbeat 
 * 
 * @param msg 
 */
void MP1Node::updateSrcMember(MessageHdr *msg){
    int id=0;
    short port;
    MemberListEntry *node=checkMemberList(msg->addr);
    if(nullptr!=node){
    ++(node->heartbeat);
    node->timestamp=par->getcurrtime();
    }else{
        insertMemberList(msg);
    }
    // delete node;
}

/**
 * @brief handle the broadcast ping message and update the heartbeat info list. 
 * 
 * @param msg 
 */
void MP1Node::pingHandler(MessageHdr *msg){
    updateSrcMember(msg);
    for(auto m:msg->member_vector){
        if(m.id>10||m.id<0) assert(1!=1);
        // this is local node info.
        MemberListEntry* node= checkMemberList(m.id,m.port);
        if(nullptr!=node&&m.heartbeat>node->heartbeat){
            node->heartbeat=m.heartbeat;     
            node->timestamp=par->getcurrtime();       
        }else if(nullptr==node){
            insertMemberList(&m);
        }
    }
}


/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size ) {
	/*
	 * Your code goes here
	 */
    MessageHdr *msg=(MessageHdr*) data;
    if(msg->msgType==JOINREQ){
        //introducer
        this->insertMemberList(msg);
        send_message(msg->addr,JOINREP);
    }else if(msg->msgType==JOINREP){
        this->insertMemberList(msg);
        memberNode->inGroup=true;
    }
    else if(msg->msgType==PING){
        this->pingHandler(msg);
    }
    delete msg;
    return true;
}

/**
 * @brief 
 * 
 * @param id 
 * @param port 
 */
Address* MP1Node::getAddress(int id,short port){
    Address *addr=new Address();
    memcpy(&addr->addr[0],&id,sizeof(int));
    memcpy(&addr->addr[4],&port,sizeof(short));
    return addr;
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {

	/*
	 * Your code goes here
	 */
    memberNode->heartbeat ++;

    // from tail to head:a trick that iter a change vector
    for(int i=memberNode->memberList.size()-1;i>=0;--i){
        Address *addr=getAddress(memberNode->memberList[i].getid(),memberNode->memberList[i].getport());
        if(this->par->getcurrtime()-memberNode->memberList[i].gettimestamp()>=TREMOVE){
            memberNode->memberList.erase(memberNode->memberList.begin()+i);
            log->logNodeRemove(&memberNode->addr,addr);
            continue;
        }
        this->send_message(addr,PING);
        delete addr;
    }
    return;
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
	memberNode->memberList.clear();
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    
}
