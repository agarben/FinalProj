
#include <string.h>
#include <jni.h>
#include <android/log.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

// TODO: Keep trying to restart network interface if it doesnt work

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <jni.h>
#include <android/log.h>
#include <errno.h>

#define HEADER_SIZE_DWORD 3
#define ORIGINATOR_IP_OFFSET 4
#define DESTINATION_IP_OFFSET 8
#define DATA_OFFSET 12
#define DWORD_BIT 32
#define DWORD_BYTE 4
#define HELLO_MSG_LEN 1000
#define TRUE 1
#define FALSE 0
#define STR_EQUAL 0
#define STR_NOT_EQUAL 1
#define HEADER_LEN 200 // TODO: May need to be increased
#define BUFLEN 64000
#define MAX_MSGS_IN_BUF 120

#define DATA_PORT 1234
#define MNG_PORT 8888

#define DONTCARE_INT 0
#define DONTCARE_CHAR "dontcare"


/////
//DEFINE MEMBER IN NETWORK
////////
//GLOBALS
////////
static const char HELLO_MSG[] = "HELLO_MSG:";

////SINGLE NODE
typedef struct IpList{
	char 	ip[20];
	int   	index;
	int  	msg_len;
	int 	this_source_was_decoded_from_encoded_msg;
	struct IpList* next_source;
} IpList;
typedef struct Buffer{

	int is_source;
	int is_valid;
	char msg[BUFLEN];
	int msg_len;
	char target_ip[20];

	int was_sent; 			// Flag - Set:This message was already sent as a broadcast clear: it was not
	int should_xor;
} Buffer;

typedef struct MemberInNetwork {
	int cur_node_distance;
	char* node_ip;
	struct MemberInNetwork * NextNode;
	struct MemberInNetwork * PrevNode;
	struct NetworkMap * SubNetwork;
	int CountdownTimer;
	struct Buffer MemberBuffer[MAX_MSGS_IN_BUF];
	int current_index_to_send;
	//need to add more parameters such as last received etc
} MemberInNetwork;

////NETWORK MAP
typedef struct NetworkMap {
	int num_of_nodes;
	MemberInNetwork* FirstMember;
	MemberInNetwork* LastNetworkMember;
	char node_base_ip[16];
} NetworkMap;


/////
// GLOBALS
/////

NetworkMap* MyNetworkMap;
NetworkMap* AllNetworkMembersList;
NetworkMap* ForbiddenNodesToAdd;

MemberInNetwork* MyMemberInstance;

int network_coding_on = FALSE;
int pause_daemon = FALSE;
int daemon_paused = FALSE;

/********************************************************
 *  FUNCTION DECLARATIONS
 ********************************************************/
void InitializeSockets();
void GenerateHelloMsg(char* base_string, NetworkMap* network_head);
jint Java_com_example_adhocktest_Routing_InitializeMap(JNIEnv* env1, jobject thiz,jstring ip_to_init);
MemberInNetwork* GetNode(char* node_ip_to_check, NetworkMap * Network_Head,int maximum_distance_from_originator);
int UpdateNodeTimer(MemberInNetwork* Node_to_update, int Countdown);
int AddToNetworkMap(char* node_ip,NetworkMap * Network_Head);
int RemoveFromNetworkMap(NetworkMap * network_to_remove_from, MemberInNetwork* member_to_remove, int network_is_members_list);
int DoesNodeExist(char* ip_to_check, NetworkMap* Network_to_check);
void Java_com_example_adhocktest_SenderUDP_SendUdpJNI( JNIEnv* env, jobject thiz, jstring ip, jstring message, jint is_broadcast);
void SendUdpJNI(const char* _target_ip, const char* message, int is_broadcast, int is_source, int msg_len, char* source_ip, int source_msg_index);
jstring Java_com_example_adhocktest_ReceiverUDP_RecvUdpJNI(JNIEnv* env1, jobject thiz, jint is_mng);
void ProcessHelloMsg(char* buf,int buf_length,NetworkMap* network_to_add_to);
jstring Java_com_example_adhocktest_Routing_RefreshNetworkMapJNI(JNIEnv* env1, jobject thiz);
void RefreshNetworkMap(NetworkMap* network_to_refresh);
MemberInNetwork* GetNextHop(NetworkMap* network_to_search ,MemberInNetwork* final_destination);
int IsNodeForbidden(char* ip_to_check);
void AddMsgToBuffer(MemberInNetwork* Member, const char* msg,int msg_indx,int msg_len, const char* target_ip, int is_source);
int IsHelloMsg(char* msg);
IpList* ExtractNextHopFromHeader(char *msg);
IpList* ExtractTargetFromHeader(char *msg);

int GetIntFromString(char* str_number);
IpList* GetSourceFromString(char* msg);
void BuildHeader(char* send_buffer, MemberInNetwork* BufferOwnerMember1, MemberInNetwork* BufferOwner2);
void Java_com_example_adhocktest_BufferHandler_RunSendingDaemonJNI(JNIEnv* env1, jobject thiz);
int NeedToBroadcast(char* message);
void FlushBuffer(char * ip_of_member_to_flush);
int Max(int num1, int num2);
void CheckCharMalloc(char* char_to_check, char* function_name,  char* var_name);
void FreeIpList(IpList* list_to_free);

/////
// Netowrk coding utility functions
/////
char* Encode(char* msg_1, char* msg);
char* Decode(char* encoded_msg, int encoded_msg_len, IpList* Sources);
char* XorMessage(char* msg_1, char* msg_2,int len_1,int len_2, char* debug_message); // TODO: Remove char* debug_message
/*
 * FUNCTION IMPLEMENTATION
 */

///////////////
// Global SendUdp Socket
///////////////
int sock_uni_tx;
struct sockaddr_in servaddr_uni_tx;
int sock_broad_tx;
struct sockaddr_in servaddr_broad_tx;
int sock_fd_rx;
struct sockaddr_in servaddr_rx;

int sock_mng_tx;
struct sockaddr_in servaddr_mng_tx;
int sock_mng_rx;
struct sockaddr_in servaddr_mng_rx;


void InitializeSockets() {

	int is_broadcast = TRUE; // always broadcast
	int retval;

	//////////////////
	// TX_UCAST
	/////////////////

	/// create socket
	if (( sock_uni_tx = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		__android_log_print(ANDROID_LOG_INFO, "Error",  "InitializeSockets(): Cannot create socket, error %d", errno);
		return;
	}

	// set socket options
	retval=setsockopt(sock_uni_tx, SOL_SOCKET, SO_BROADCAST, &is_broadcast, sizeof(is_broadcast));
	if (retval) {
		close(sock_uni_tx);
		__android_log_print(ANDROID_LOG_INFO, "Error",  "InitializeSockets(): Failed to set setsockopt(), error: %d", errno);
		return;
	}

	memset((char*)&servaddr_uni_tx, 0, sizeof(servaddr_uni_tx));
	servaddr_uni_tx.sin_family = AF_INET;
	servaddr_uni_tx.sin_port = htons(DATA_PORT);

	__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "InitializeSockets(): sock_uni_tx values = %d", sock_uni_tx);

	servaddr_uni_tx.sin_addr.s_addr = htonl(INADDR_ANY);
	if ((inet_aton(MyNetworkMap->node_base_ip,&servaddr_uni_tx.sin_addr)) == 0) {
		close(sock_uni_tx);
		__android_log_print(ANDROID_LOG_INFO, "Error",  "SendUdpJNI():Cannot decode IP address");
		return;
	}

	is_broadcast = FALSE;
	if ((inet_aton(MyNetworkMap->node_base_ip,&servaddr_uni_tx.sin_addr)) == 0) {		// set socket target ip to next hop, use data tx socket
		close(sock_uni_tx);
		__android_log_print(ANDROID_LOG_INFO, "Error",  "SendUdpJNI():Cannot decode IP address");
		return;
	}

	setsockopt(sock_uni_tx, SOL_SOCKET, SO_BROADCAST, &is_broadcast, sizeof(is_broadcast));

	//////////////////
	// TX_BCAST
	/////////////////

	/// create socket
	if (( sock_broad_tx = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		__android_log_print(ANDROID_LOG_INFO, "Error",  "InitializeSockets(): Cannot create socket, error %d", errno);
		return;
	}

	// set socket options
	is_broadcast = TRUE;
	retval=setsockopt(sock_broad_tx, SOL_SOCKET, SO_BROADCAST, &is_broadcast, sizeof(is_broadcast));
	if (retval) {
		close(sock_broad_tx);
		__android_log_print(ANDROID_LOG_INFO, "Error",  "InitializeSockets(): Failed to set setsockopt(), error: %d", errno);
		return;
	}

	memset((char*)&servaddr_broad_tx, 0, sizeof(servaddr_broad_tx));
	servaddr_broad_tx.sin_family = AF_INET;
	servaddr_broad_tx.sin_port = htons(DATA_PORT);

	__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "InitializeSockets(): sock_broad_tx values = %d", sock_broad_tx);


	if ((inet_aton(MyNetworkMap->node_base_ip,&servaddr_broad_tx.sin_addr)) == 0) {
				close(sock_mng_tx);
				__android_log_print(ANDROID_LOG_INFO, "Error",  "SendUdpJNI():Cannot decode IP address");
				return;
	}
	servaddr_broad_tx.sin_addr.s_addr |= 0xff000000;
	////////////////////
	/// MNG TX
	/////////////////////

	/// create socket
	if (( sock_mng_tx = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		__android_log_print(ANDROID_LOG_INFO, "Error",  "InitializeSockets(): Cannot create management_tx socket, error %d", errno);
		return;
	}

	// set socket options
	is_broadcast = TRUE;
	retval=setsockopt(sock_mng_tx, SOL_SOCKET, SO_BROADCAST, &is_broadcast, sizeof(is_broadcast));
	if (retval) {
		close(sock_mng_tx);
		__android_log_print(ANDROID_LOG_INFO, "Error",  "InitializeSockets(): Failed to set management_tx setsockopt(), error: %d", errno);
		return;
	}

	memset((char*)&servaddr_mng_tx, 0, sizeof(servaddr_mng_tx));
	servaddr_mng_tx.sin_family = AF_INET;
	servaddr_mng_tx.sin_port = htons(MNG_PORT);

	__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "InitializeSockets(): sock_mng_tx values = %d", sock_mng_tx);

	servaddr_mng_tx.sin_addr.s_addr = htonl(INADDR_ANY);


	if ((inet_aton(MyNetworkMap->node_base_ip,&servaddr_mng_tx.sin_addr)) == 0) {
				close(sock_mng_tx);
				__android_log_print(ANDROID_LOG_INFO, "Error",  "SendUdpJNI():Cannot decode IP address");
				return;
	}
	servaddr_mng_tx.sin_addr.s_addr |= 0xff000000;

	///////////////////
	// MNG RX
	///////////////////

	// Create and bind socket
	struct sockaddr_in servaddr_mng_rx;

	if ((sock_mng_rx = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) {
		int temp = errno;
		__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "InitializeSockets(): socket() call failed. errno : %d ",temp);
	}


	bzero(&servaddr_mng_rx, sizeof(servaddr_mng_rx));
	servaddr_mng_rx.sin_family = AF_INET;
	servaddr_mng_rx.sin_port = htons(MNG_PORT);
	servaddr_mng_rx.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sock_mng_rx, (struct sockaddr* ) &servaddr_mng_rx, sizeof(servaddr_mng_rx))==-1) {
		__android_log_print(ANDROID_LOG_INFO, "Error", "InitializeSockets() Failed to bind");
	}

	///////////////////
	// RX
	///////////////////

	// Create and bind socket
	struct sockaddr_in servaddr_rx;

	if ((sock_fd_rx = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) {
		int temp = errno;
		__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "InitializeSockets(): socket() call failed. errno : %d ",temp);
	}


	bzero(&servaddr_rx, sizeof(servaddr_rx));
	servaddr_rx.sin_family = AF_INET;
	servaddr_rx.sin_port = htons(DATA_PORT);
	servaddr_rx.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sock_fd_rx, (struct sockaddr* ) &servaddr_rx, sizeof(servaddr_rx))==-1) {
		__android_log_print(ANDROID_LOG_INFO, "Error", "InitializeSockets() Failed to bind");
	}
}


void GenerateHelloMsg(char* base_string, NetworkMap* network_head) {


	if (network_head != NULL) {
		__android_log_print(ANDROID_LOG_INFO, "NetworkMap","GenerateHelloMessage(): base_ip [%s] base_str [%s]",network_head->node_base_ip, base_string);
		MemberInNetwork* temp = network_head->FirstMember;
		__android_log_print(ANDROID_LOG_INFO, "NetworkMap","GenerateHelloMessage(): The String so far is [%s]",base_string);
		while (temp!=NULL) {
			strcat(base_string,temp->node_ip);
			strcat(base_string,"(");
			GenerateHelloMsg(base_string, temp->SubNetwork);
			strcat(base_string,")");
			temp = temp->NextNode;
		}
	}
}

//return 1 if node is forbidden
int IsNodeForbidden(char* ip_to_check){


	MemberInNetwork* temp;
	temp = ForbiddenNodesToAdd->FirstMember;
	while (temp != NULL){
		if (strcmp(temp->node_ip,ip_to_check)==0){ //match
			__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c","IsNodeForbidden(): Node [%s] is in the forbidden list", temp->node_ip);
			return TRUE;
		}
		else{
			__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c","IsNodeForbidden(): Node [%s] is not the node we are looking for ([%s]) in the forbidden list", temp->node_ip, ip_to_check);
			temp=temp->NextNode;
		}
	}

	return FALSE;
}


/*
 * InitializeMap.
 */
jint
Java_com_example_adhocktest_Routing_InitializeMap(JNIEnv* env1,
        jobject thiz,jstring ip_to_init){


	__android_log_print(ANDROID_LOG_INFO, "NetworkMap","InitializeMap(): Version 30");
	MyNetworkMap = (NetworkMap*)malloc(sizeof(NetworkMap));
	if (MyNetworkMap==NULL){
		__android_log_print(ANDROID_LOG_INFO, "NetworkMap","InitializeMap(): failed. could not allocate memory for MyNetworkMap");
		return -1;
	}
	char const * get_my_ip = (*env1)->GetStringUTFChars(env1, ip_to_init, 0);
	strcpy(MyNetworkMap->node_base_ip,get_my_ip);
	MyNetworkMap->num_of_nodes=0;
	MyNetworkMap->FirstMember=NULL;
	MyNetworkMap->LastNetworkMember=NULL;

	AllNetworkMembersList = (NetworkMap*)malloc(sizeof(NetworkMap));
	if (AllNetworkMembersList==NULL){
			__android_log_print(ANDROID_LOG_INFO, "NetworkMap","InitializeMap(): failed. could not allocate memory for AllNetworkMembersList");
			free(MyNetworkMap);
			return -1;
	}
	//no allocation for base ip
	strcpy(AllNetworkMembersList->node_base_ip,"AllMembersList");
	AllNetworkMembersList->num_of_nodes=0;
	AllNetworkMembersList->FirstMember=NULL;
	AllNetworkMembersList->LastNetworkMember=NULL;

	/// Forbidden nodes to be first neighbors - this is used to create false "out of range" scenarios
	ForbiddenNodesToAdd = (NetworkMap*)malloc(sizeof(NetworkMap));
	if (ForbiddenNodesToAdd==NULL){
			__android_log_print(ANDROID_LOG_INFO, "NetworkMap","InitializeMap(): failed. could not allocate memory for ForbiddenNodesToAdd");
			free(AllNetworkMembersList);
			free(MyNetworkMap);
			return -1;
	}
	strcpy(ForbiddenNodesToAdd->node_base_ip,"ForbiddenNodesToAdd");
	ForbiddenNodesToAdd->num_of_nodes=0;
	ForbiddenNodesToAdd->FirstMember=NULL;
	ForbiddenNodesToAdd->LastNetworkMember=NULL;
	if (strcmp(MyNetworkMap->node_base_ip , "192.168.2.33")==0) {
//		AddToNetworkMap("192.168.2.22",ForbiddenNodesToAdd);
		AddToNetworkMap("192.168.2.207",ForbiddenNodesToAdd);
	} else if (strcmp(MyNetworkMap->node_base_ip ,"192.168.2.96")==0) {
		AddToNetworkMap("192.168.2.207",ForbiddenNodesToAdd);
	} else if (strcmp(MyNetworkMap->node_base_ip ,"192.168.2.22")==0) {
//		AddToNetworkMap("192.168.2.33",ForbiddenNodesToAdd);
	} else if (strcmp(MyNetworkMap->node_base_ip ,"192.168.2.207")==0) {
		AddToNetworkMap("192.168.2.33",ForbiddenNodesToAdd);
		AddToNetworkMap("192.168.2.96",ForbiddenNodesToAdd);
	}


//	33<->22<->207
	__android_log_print(ANDROID_LOG_INFO, "NetworkMap","InitializeMap(): completed successfully. My IP : [%s]", MyNetworkMap->node_base_ip);

	MyMemberInstance = (MemberInNetwork*)malloc(sizeof(MemberInNetwork));
	if (MyMemberInstance == NULL) {
		__android_log_print(ANDROID_LOG_INFO, "WARNING",  "InitializeMap(): MyMemberInstance == NULL");
	}
	MyMemberInstance->node_ip = (char*)malloc(sizeof(char)*20);
	if (MyMemberInstance->node_ip == NULL) {
		__android_log_print(ANDROID_LOG_INFO, "WARNING",  "InitializeMap(): MyMemberInstance->node_ip == NULL");
		}
	strcpy(MyMemberInstance->node_ip,MyNetworkMap->node_base_ip);
	int i;
	MyMemberInstance->current_index_to_send = 0;
	for (i=0; i<MAX_MSGS_IN_BUF; i++) {
		MyMemberInstance->MemberBuffer[i].is_valid = FALSE;
	}

	InitializeSockets();
	return 0;
}

/*
 * GetNode - gets node from the network map. return the node or NULL if not found
 */
// TODO: Need to check closeness of the node and perhaps change it, need to add DEBUG method.
// TODO: check mem allocations
MemberInNetwork* GetNode(char* node_ip_to_check, NetworkMap * Network_Head,int maximum_distance_from_originator){

	int i;

	MemberInNetwork * temp = Network_Head->FirstMember;
	MemberInNetwork * temp_in_subnetwork;

	__android_log_print(ANDROID_LOG_INFO, "NetworkMap","GetNode(): Check if [%s] has [%s] in the network map", Network_Head->node_base_ip,node_ip_to_check);

	if(maximum_distance_from_originator==0){
		__android_log_write(ANDROID_LOG_INFO,"NetworkMap","GetNode(): maximum distance allowed reached 0. returning");
		return NULL;
	}

	//check if exists in the network map
	while(temp!=NULL){
		if(strcmp(temp->node_ip,node_ip_to_check)==0){
			//node was found in the network map
			__android_log_print(ANDROID_LOG_INFO, "NetworkMap","GetNode(): The node with ip :[%s] was already in the network map [%s]", node_ip_to_check, Network_Head->node_base_ip);
			return temp;
		}
		else{
			//this node still isn't the node we received
			__android_log_print(ANDROID_LOG_INFO, "NetworkMap","GetNode(): Current node : [%s] is not the node we want to look for [%s]. checking subnetwork", temp->node_ip,node_ip_to_check);
			temp_in_subnetwork = GetNode(node_ip_to_check,temp->SubNetwork,maximum_distance_from_originator-1);
			if (temp_in_subnetwork != NULL)
			{
				__android_log_write(ANDROID_LOG_INFO,"NetworkMap","GetNode(): Node was found in the subnetwork network");
				return temp_in_subnetwork;
			}
			temp = temp->NextNode;
		}
	}

	__android_log_write(ANDROID_LOG_INFO,"NetworkMap","GetNode(): Node was not found in the network");
	return temp;

}

int UpdateNodeTimer(MemberInNetwork* Node_to_update, int Countdown) {
	Node_to_update->CountdownTimer = Countdown;
	return Node_to_update->CountdownTimer;
}

/*
 * AddToNetworkMap : receive node_ip, node_type - add the data to the network map
 */
int AddToNetworkMap(char* node_ip, NetworkMap * Network_Head){

	MemberInNetwork* temp = GetNode(node_ip, Network_Head, 1);
	MemberInNetwork* temp_for_network_list;
	int i;
	if (temp == NULL) {
		__android_log_print(ANDROID_LOG_INFO, "NetworkMap","AddToNetworkMap(): Need to add node with ip :[%s] to network map with base ip : [%s]", node_ip,Network_Head->node_base_ip);
		temp = (MemberInNetwork*)malloc(sizeof(MemberInNetwork));
		temp->NextNode = NULL;
		temp->cur_node_distance = 1;
		temp->node_ip = (char*)malloc(16*sizeof(char));
		temp->SubNetwork = (NetworkMap*)malloc(sizeof(NetworkMap));
		if (temp->SubNetwork == NULL)
		{
			free(temp->node_ip);
			__android_log_write(ANDROID_LOG_INFO, "NetworkMap","AddToNetworkMap(): FAILURE TO ALLOCATE SUBNETWORK");
			return 0;
		}
		temp->SubNetwork->FirstMember = NULL;
		temp->SubNetwork->LastNetworkMember = NULL;
		strcpy(temp->SubNetwork->node_base_ip,node_ip);
		temp->SubNetwork->num_of_nodes = 0;
		strcpy(temp->node_ip,node_ip);
		///////
		//buffers
		///////
		temp->current_index_to_send = 0;
		for (i=0; i<MAX_MSGS_IN_BUF; i++) {
			temp->MemberBuffer[i].is_valid = FALSE;
		}

		/////////

		Network_Head->num_of_nodes += 1;
		__android_log_print(ANDROID_LOG_INFO, "NetworkMap","AddToNetworkMap(): Network of base ip [%s] has [%d] nodes ",Network_Head->node_base_ip, Network_Head->num_of_nodes);
		if (Network_Head->num_of_nodes == 1){
			temp->PrevNode=NULL;
			Network_Head->FirstMember = temp;
		}
		else{
			Network_Head->LastNetworkMember->NextNode = temp;
			temp->PrevNode = Network_Head->LastNetworkMember;
		}
		Network_Head->LastNetworkMember = temp;
		__android_log_print(ANDROID_LOG_INFO, "NetworkMap","AddToNetworkMap(): Successfully added [%s] to network.",node_ip);

	} else {
		__android_log_print(ANDROID_LOG_INFO, "NetworkMap","AddToNetworkMap(): Node with ip :[%s] already exists in this level.", node_ip);
	}
	UpdateNodeTimer(temp,4);
	return 0;
}

/*
 * RemoveFromNetworkMap : receive MemberInNetwork to remove
 */
int RemoveFromNetworkMap(NetworkMap * network_to_remove_from, MemberInNetwork* member_to_remove, int network_is_members_list){

//	static int depth_in_recursion =0;
//	if (depth_in_recursion++ == 0) {
//		pause_daemon = TRUE;  					// TODO: We can do this only for allMembersInNetworkMap
//		while (daemon_paused == FALSE) {
//		}
//	}
	__android_log_print(ANDROID_LOG_INFO, "NetworkMap","RemoveFromNetworkMap(): Received a request to remove node: [%s]", member_to_remove->node_ip);

	// 3. remove ip from network and free memory
	MemberInNetwork* son_to_remove = member_to_remove->SubNetwork->FirstMember; // remove all sons of member_to_remove before deleting the member itself
	MemberInNetwork* temp_member;
	while (son_to_remove != NULL) {
		temp_member = son_to_remove->NextNode;
		RemoveFromNetworkMap(member_to_remove->SubNetwork,son_to_remove,FALSE);
		son_to_remove = temp_member;
		if (son_to_remove == NULL) {
		} else {
			son_to_remove->PrevNode = NULL; // pref is null but first in sub isn't touched anymore, [ not important ]
		}
	}

//	NEXT HOP   FINAL DEST     OTHER HEADER SHIT
//    IP1      | IP2         |       rsrvd         |   MESSAGE
	// After all sons have been deleted, remove member_to_remove
	if ((network_to_remove_from->FirstMember == network_to_remove_from->LastNetworkMember) && network_to_remove_from->FirstMember!=NULL){

		__android_log_write(ANDROID_LOG_INFO, "NetworkMap","RemoveFromNetworkMap(): There is only one node in the network (first = last)");

		network_to_remove_from->FirstMember = NULL;
		network_to_remove_from->LastNetworkMember = NULL;
	}
	else{
		/*
		 * the node that we want to remove-  need to check if its first, last or in the middle.
		 * first : point FirstMember to the second member.
		 * last : point LastMember to one before current last
		 * default : link previous and next nodes of the current node
		 */
		if (member_to_remove == network_to_remove_from->LastNetworkMember){

			member_to_remove->PrevNode->NextNode = NULL;
			network_to_remove_from->LastNetworkMember = member_to_remove->PrevNode;
			__android_log_print(ANDROID_LOG_INFO, "NetworkMap","RemoveFromNetworkMap(): Removing LastNetworkMember.New LastNetworkMember is: [%s]", network_to_remove_from->LastNetworkMember->node_ip);
		}
		else{
			if (member_to_remove == network_to_remove_from->FirstMember){
				member_to_remove->NextNode->PrevNode = NULL;
				network_to_remove_from->FirstMember = member_to_remove->NextNode;
				__android_log_print(ANDROID_LOG_INFO, "NetworkMap","RemoveFromNetworkMap(): Removing FirstMember. New FirstMember is: [%s]", network_to_remove_from->FirstMember->node_ip);
			}
			else{

				member_to_remove->NextNode->PrevNode = member_to_remove->PrevNode;
				member_to_remove->PrevNode->NextNode = member_to_remove->NextNode;
				__android_log_write(ANDROID_LOG_INFO, "NetworkMap","RemoveFromNetworkMap(): Removing node somewhere in the middle");
			}

		}

	}
	network_to_remove_from->num_of_nodes -= 1;
	__android_log_print(ANDROID_LOG_INFO, "NetworkMap","RemoveFromNetworkMap(): Removing member [%s] successfully from the network of [%s]. New amount of nodes [%d]", member_to_remove->node_ip,network_to_remove_from->node_base_ip,network_to_remove_from->num_of_nodes);

//	if (network_is_members_list == FALSE) {
//		__android_log_write(ANDROID_LOG_INFO, "NetworkMap","RemoveFromNetworkMap(): Entered first condition");
//		temp_member = GetNode(member_to_remove->node_ip, AllNetworkMembersList, 1);
//		__android_log_write(ANDROID_LOG_INFO, "NetworkMap","RemoveFromNetworkMap(): Entered second condition");
//		if (temp_member != NULL){;
//		__android_log_write(ANDROID_LOG_INFO, "NetworkMap","RemoveFromNetworkMap(): Entered third condition");
//		__android_log_print(ANDROID_LOG_INFO, "NetworkMap","RemoveFromNetworkMap(): AllNetworkMembersList->[%s] instance_count=[%d] ",temp_member->node_ip, temp_member->instance_count);
//			if (--temp_member->instance_count == 0) { // One less instance of node_ip exists in the network now.
//				__android_log_print(ANDROID_LOG_INFO, "NetworkMap","RemoveFromNetworkMap(): Going to remove [%s] from AllNetworkMembersList",temp_member->node_ip);
//				RemoveFromNetworkMap(AllNetworkMembersList,temp_member,TRUE);
//			}
//		}
//	}

	if (network_is_members_list == FALSE) {
		temp_member = GetNode(member_to_remove->node_ip, MyNetworkMap, 100); // TODO: Should be network depth
		if (temp_member == NULL) {
			temp_member = GetNode(member_to_remove->node_ip, AllNetworkMembersList,1);
			if (temp_member != NULL) {
				RemoveFromNetworkMap(AllNetworkMembersList,temp_member,TRUE);
			} else {
			}
		}
	}

	FlushBuffer(member_to_remove->node_ip);
	free(member_to_remove->node_ip);
	free(member_to_remove->SubNetwork);
	free(member_to_remove);
//	if (--depth_in_recursion == 0) {
//		pause_daemon = FALSE;
//	}
	return 0;

}

///*
// * CheckNodeExistence - checks if node exists in the network map, if it doesn't - adds it
// */
//// TODO: Need to check closeness of the node and perhaps change it, need to add DEBUG method.
//// TODO: check mem allocations
int DoesNodeExist(char* ip_to_check, NetworkMap* Network_to_check){

	int i;
	int retVal = FALSE;

	MemberInNetwork * temp = Network_to_check->FirstMember;

	//check if exists in the network map
	while(temp!=NULL){
		if(strcmp(temp->node_ip,ip_to_check) == STR_EQUAL){
			//node was found in the network map
			return TRUE;
		}
		else{
			//this node still isn't the node we received
			temp = temp->NextNode;
		}
	}
	return FALSE;
}

void
Java_com_example_adhocktest_SenderUDP_SendUdpJNI( JNIEnv* env,
                                                  jobject thiz, jstring ip, jstring j_message, jint is_broadcast)
{

	const char *_target_ip = (*env)->GetStringUTFChars(env, ip, 0);
	const char *message = (*env)->GetStringUTFChars(env, j_message, 0);   // Message to be sent

	SendUdpJNI(_target_ip, message, is_broadcast, TRUE, strlen(message), MyNetworkMap->node_base_ip, DONTCARE_INT);
	(*env)->ReleaseStringUTFChars(env,j_message,message);
	(*env)->ReleaseStringUTFChars(env,ip,_target_ip);
}

void SendUdpJNI(const char* _target_ip, const char* message, int is_broadcast, int is_source, int msg_len, char* source_ip, int source_msg_index) {

	__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): Entered SendUDPJni() with index:len [%d]:[%d]",source_msg_index,msg_len);
	int retval;
	char send_buf[BUFLEN];;
	static int msg_index = -1;

	MemberInNetwork* SourceMember;

	char* next_hop_ip;
	char final_dest_ip[20];

	int _is_broadcast;
	////////////////
	/// send
	////////////////
	char hello_message_string[HELLO_MSG_LEN];

	if (strcmp(message, "HELLO_MSG:")==0) {
		strcpy(hello_message_string,"HELLO_MSG:");
		strcat(hello_message_string,MyNetworkMap->node_base_ip);
		strcat(hello_message_string,"(");
		GenerateHelloMsg(hello_message_string,MyNetworkMap);
		strcat(hello_message_string,")");

		retval = sendto(sock_mng_tx, hello_message_string, strlen(hello_message_string), 0, (struct sockaddr*)&servaddr_mng_tx, sizeof(servaddr_mng_tx));
		if ( retval < 0) {
//			__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "SendUdpJNI(): sendto failed with %d. message was [%s]", errno,hello_message_string); // TODO: Uncomment
		} else {
//			__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "SendUdpJNI(): sendto() Success (retval=%d messge='%s' size=%d ip=%s", retval,hello_message_string,strlen(hello_message_string),_target_ip); // TODO: Uncomment
		}
	} else {
		if (is_source) {
			msg_index = (msg_index+1) % MAX_MSGS_IN_BUF;
			AddMsgToBuffer(MyMemberInstance, message, msg_index, msg_len,_target_ip, is_source); // TODO: If im source, else need to give the proper source member
		} else {
			AddMsgToBuffer(GetNode(source_ip,AllNetworkMembersList,1), message, source_msg_index, msg_len,_target_ip, is_source); // TODO: If im source, else need to give the proper source member
		}
	}
}

jstring
Java_com_example_adhocktest_ReceiverUDP_RecvUdpJNI(JNIEnv* env1, jobject thiz, jint is_mng)
{
	IpList* sources; // TODO: delete


	int retVal=-1;
	int is_ignore_msg = 0;
	int is_forward_msg = 0;

	char return_str[BUFLEN+30];
	IpList* target_list;
	IpList* next_hop_list;

	///////////

	/// Receive UDP packets
	//////////
	//try to receive
	char buf[BUFLEN];
	char* strstrptr;

	struct sockaddr_in cli_addr;
	socklen_t slen=sizeof(cli_addr);

	if (is_mng == FALSE) {
		retVal = recvfrom(sock_fd_rx, buf, BUFLEN, 0, (struct sockaddr*)&cli_addr, &slen);
		__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): (DATA) Raw string received: <%s>",buf);
	} else {
		retVal = recvfrom(sock_mng_rx, buf, BUFLEN, 0, (struct sockaddr*)&cli_addr, &slen);
		__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): (MNG)  Raw string received: <%s>",buf);
	}
	if (retVal==-1) {
		__android_log_print(ANDROID_LOG_INFO, "Error",  "RecvUdpJNI() Failed to recvfrom() retVal=%d",retVal);
		return (*env1)->NewStringUTF(env1, "ignore");
	} else {
		buf[retVal] = '\0';
	}

//	close(sock_fd_rx); // no need to close this global socket which is for the entire program

	if (strcmp(inet_ntoa(cli_addr.sin_addr),MyNetworkMap->node_base_ip)==0) {									// MSG is an echo from myself
		is_ignore_msg = TRUE;
		__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): Ignoring echo from myself [%s]",buf);
	} else if (IsNodeForbidden(inet_ntoa(cli_addr.sin_addr)) == TRUE) {
		is_ignore_msg = TRUE;
		__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): Ignoring message from forbidden node [%s]",buf);
	} else if (IsHelloMsg(buf) == TRUE) {																		// Hello msg
		is_ignore_msg = TRUE;
		if (IsNodeForbidden(inet_ntoa(cli_addr.sin_addr)) == TRUE){
			__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): Ignoring HELLO_MSG from  [%s]", inet_ntoa(cli_addr.sin_addr));
		} else {
			ProcessHelloMsg(strpbrk(buf,":")+1,strlen(strpbrk(buf,":")+1),MyNetworkMap);                        // WARNING This line will crash if hello msg doesnt contain ":"
		}
	} else {
		if (network_coding_on == TRUE) {
			__android_log_print(ANDROID_LOG_INFO, "WARNING",  "RecvUdpJNI(): Message decryption should occur here but its not implemented");
		}

		target_list = ExtractTargetFromHeader(buf);
		next_hop_list = ExtractNextHopFromHeader(buf);
		if (target_list==NULL || next_hop_list == NULL){
			__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): Bad message, ignoring [%s]",buf);
			is_ignore_msg = TRUE;
		} else {
			if (ImInTheIpList(target_list) == TRUE) {        		// i am on of the targets of the msg
				__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): I am the target of this message");
			} else if (ImInTheIpList(next_hop_list) == TRUE){ 		// i am on of the next hops
				is_forward_msg = TRUE;

				__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): I am the next hop of this message. forwarding [%s]",buf);
				strstrptr = strstr(buf,";");
				if (strstrptr == NULL) {
					__android_log_print(ANDROID_LOG_INFO, "Error",  "RecvUdpJNI(): Did not find ';', Ignoring message.");
					FreeIpList(target_list);
					FreeIpList(next_hop_list);
				}

				// TODO: Decode, and send only the message that is new

				sources = GetSourceFromString(buf);
				__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): Extracted sources: index:len [%s]-[%d]:[%d]", sources->ip, sources->index, sources->msg_len);
				SendUdpJNI(target_list->ip,strstrptr+1, TRUE, FALSE, sources->msg_len, sources->ip, sources->index);
				// TODO: Make more efficient
				// TODO: When we add XOR we have to use the message length integer instead of strlen()
				// TODO: When network coding this will have to happen after decode assuming 4+ line

				// end of TODO

			} else { 	 						// I am neither the next hop or target
				is_ignore_msg = TRUE;
			}
		}
		//////
		// Free mem
		//////
		FreeIpList(target_list);
		FreeIpList(next_hop_list);
	}

	__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): HOPA raw_msg [%s]",buf);
	//if received successfully , close socket
	if (is_ignore_msg == TRUE) {
		return (*env1)->NewStringUTF(env1, "ignore"); // return ignore
	} else if (is_forward_msg) {
		return (*env1)->NewStringUTF(env1, "ignore"); // TODO: Make this return some forward feedback
	} else {
		strstrptr = strstr(buf,"}~"); 	// first try to check if this is an image
		if (strstrptr == NULL) {		// if its not image nor text just return something
			__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): HOPA gonna return 1 [%s]",buf);
			return (*env1)->NewStringUTF(env1, buf);
		} else {


			if (network_coding_on == TRUE) {

				sources = GetSourceFromString(buf);
				if (sources != NULL) {
					if (sources->next_source != NULL) {
						__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): HOPA gonna return 2 [%s]",strstrptr);
						return Decode(buf, Max(sources->msg_len,sources->next_source->msg_len), sources); // Only if there are really 2 sources and decode is required
					}
				}

				__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): HOPA gonna return 3  [%s]",strstrptr);
				return (*env1)->NewStringUTF(env1, strstrptr+1); // even when network coding is ON sometimes we dont need to decode


			} else {
				__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): HOPA gonna return 4 [%s]",strstrptr);
				return (*env1)->NewStringUTF(env1, strstrptr+1); // return image or text leaving the ~ or ! char respectively
			}

		}
	}
}

void ProcessHelloMsg(char* buf,int buf_length,NetworkMap* network_to_add_to) {

	int i=-1;
	int start_index=0;
	int end_index;
	int cnt=-1;
	int retVal;
	char* ip_to_add = (char*)malloc(sizeof(char)*30);
	int dont_add_sons = FALSE;

	__android_log_print(ANDROID_LOG_INFO, "ProcessHelloMsg()",  "String:<%s>:%d",buf,buf_length);
	while (i++<buf_length) {
		if(start_index==0) {
			ip_to_add[i] = buf[i];
		}
		if (buf[i]=='(') {
			if (start_index==0) {
				start_index=i+1;			//First '(' in the hello
				cnt=0;						//Init count
				ip_to_add[i] = '\0';
				__android_log_print(ANDROID_LOG_INFO, "ProcessHelloMsg()",  "Ip_to_add : [%s] calling add to network",ip_to_add);
				if (strcmp(ip_to_add,MyNetworkMap->node_base_ip) == 0) {
					dont_add_sons = TRUE;
				} else {
					retVal = AddToNetworkMap(ip_to_add,network_to_add_to);
					if (retVal != 0) {
						//print ERROR log
					}
					__android_log_print(ANDROID_LOG_INFO, "ProcessHelloMsg()",  "AllNetworkMembersList->base_ip = [%s]",AllNetworkMembersList->node_base_ip);
					retVal = AddToNetworkMap(ip_to_add,AllNetworkMembersList); // Here we only add the ip as the member of the general list of network members.
					if (retVal != 0) {
						//print ERROR log
					}
				}
			}
			cnt++;							//Each '(' increases count which can only be decreased by ')'
		}
		if (buf[i]==')') {
			if (--cnt == 0) {			//Decrease count, if its 0 it means that the first '(' was met by its respective ')'
				end_index=i-1;			// in this case we are at the end index
				if (dont_add_sons != TRUE) {
					ProcessHelloMsg(buf+start_index, (end_index-start_index+1),GetNode(ip_to_add,network_to_add_to,1)->SubNetwork);   //GetNodeSubNetwork(network_to_add_to,ip_to_add));
				}
				if (i<buf_length-1) {
					ProcessHelloMsg(buf+i+1, buf_length-(i+1),network_to_add_to);
				}
				break;
			}
		}
	}
	//TODO: Free memory
}

/*
 * InitializeMap.
 */
jstring
Java_com_example_adhocktest_Routing_RefreshNetworkMapJNI(JNIEnv* env1, jobject thiz){

	RefreshNetworkMap(MyNetworkMap);

	MemberInNetwork* temp_node = AllNetworkMembersList->FirstMember;
	char* network_list_str = (char*)malloc(sizeof(char)*HELLO_MSG_LEN);
	network_list_str[0] = '\0';
	GenerateHelloMsg(network_list_str,AllNetworkMembersList);
	__android_log_print(ANDROID_LOG_INFO, "RefreshNetworkMapJNI","AllMembersList is currently [%s]",network_list_str);

	return (*env1)->NewStringUTF(env1, network_list_str);
}

void RefreshNetworkMap(NetworkMap* network_to_refresh) {

	MemberInNetwork* son_to_refresh = network_to_refresh->FirstMember;
	MemberInNetwork* temp_network_member;
	while (son_to_refresh != NULL) {
		if (son_to_refresh->CountdownTimer > 0) {
			UpdateNodeTimer(son_to_refresh, (son_to_refresh->CountdownTimer-1));

			__android_log_print(ANDROID_LOG_INFO, "RefreshNetworkMap","Network [%s] member [%s] countdown=[%d]",network_to_refresh->node_base_ip,son_to_refresh->node_ip,son_to_refresh->CountdownTimer);
			RefreshNetworkMap(son_to_refresh->SubNetwork);
			son_to_refresh = son_to_refresh->NextNode;
		} else {
			__android_log_print(ANDROID_LOG_INFO, "RefreshNetworkMap","Network [%s] member [%s] countdown=[%d] - REMOVE",network_to_refresh->node_base_ip,son_to_refresh->node_ip,son_to_refresh->CountdownTimer);
			temp_network_member = son_to_refresh;
			son_to_refresh = son_to_refresh->NextNode;

			RemoveFromNetworkMap(network_to_refresh, temp_network_member,FALSE);
		}
	}
}

MemberInNetwork* GetNextHop(NetworkMap* network_to_search ,MemberInNetwork* final_destination) {

	MemberInNetwork* temp_member;
	MemberInNetwork* temp_member_from_sub;
	__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c","GetNextHop(): Entered GetNextHop with Network of [%s]. looking for [%s]",network_to_search->node_base_ip, final_destination->node_ip);

	temp_member = network_to_search->FirstMember;
	while (temp_member != NULL){
		if (strcmp(temp_member->node_ip,final_destination->node_ip) == 0){
			__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c","GetNextHop(): Found member [%s] in Network of [%s]", temp_member->node_ip,network_to_search->node_base_ip);
			return temp_member;
		} else {
			__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c","GetNextHop(): Current member [%s] in Network of [%s] is not [%s]", temp_member->node_ip,network_to_search->node_base_ip, final_destination->node_ip);
			temp_member = temp_member->NextNode;
		}
	}
	__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c","GetNextHop(): Node [%s] was not a direct son of current node network [%s] , checking deeper", final_destination->node_ip,network_to_search->node_base_ip);

	temp_member = network_to_search->FirstMember;
	while (temp_member != NULL){

		temp_member_from_sub = GetNextHop(temp_member->SubNetwork,final_destination);
		if (temp_member_from_sub != NULL){
			__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c","GetNextHop(): Found member [%s] in SubNetwork of [%s]", temp_member_from_sub->node_ip,temp_member->node_ip);
			return temp_member;
		} else {
			__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c","GetNextHop(): Node [%s] was not in SubNetwork of [%s]", final_destination->node_ip,temp_member->node_ip);
			temp_member = temp_member->NextNode;
		}

	}

	__android_log_print(ANDROID_LOG_INFO, "adhoc-.c","GetNextHop(): Did not find [%s] in [%s]", final_destination->node_ip,network_to_search->node_base_ip);
	return NULL;

}

void AddMsgToBuffer(MemberInNetwork* Member, const char* msg,int msg_indx,int msg_len, const char* target_ip, int is_source) {
	Member->MemberBuffer[msg_indx].is_valid = FALSE;
	Member->MemberBuffer[msg_indx].is_source = is_source;
	Member->MemberBuffer[msg_indx].was_sent = FALSE;
	Member->MemberBuffer[msg_indx].msg_len = msg_len;
	Member->MemberBuffer[msg_indx].should_xor = msg_indx;
	// TODO: Handle a missed packet scenario (received 4-6 or 4-6-5)

	strcpy(Member->MemberBuffer[msg_indx].msg, msg);
	strcpy(Member->MemberBuffer[msg_indx].target_ip, target_ip);


	Member->MemberBuffer[msg_indx].is_valid = TRUE;

	/////////////// END OF FUNCTION - ONLY LOGS FROM NOW ///////////////////

	int i;
	for (i=0; i<3; i++) {
//		if (Member->MemberBuffer[i].is_valid == TRUE && Member->MemberBuffer[i].was_sent == FALSE) {
		if (1) {
//			__android_log_print(ANDROID_LOG_INFO, "Buffers","%d. msg=[%s]", i, Member->MemberBuffer[i].msg);
			__android_log_print(ANDROID_LOG_INFO, "Buffers","%d.	member_ip=[%s] target_ip=[%s] is_valid=[%d] was_sent=[%d] msg_len=[%d] should_xor=[%d]", i, Member->node_ip, Member->MemberBuffer[i].target_ip, Member->MemberBuffer[i].is_valid, Member->MemberBuffer[i].was_sent, Member->MemberBuffer[i].msg_len, Member->MemberBuffer[i].should_xor);
		}
	}

}
int IsHelloMsg(char* msg) {
	if (strstr(msg,HELLO_MSG)==NULL) {
		return FALSE;
	} else {
		return TRUE;
	}
}

IpList* ExtractNextHopFromHeader(char *msg) {

	IpList* temp = (IpList*)malloc(sizeof(IpList));
	temp->next_source = NULL;

	int i;
	int offset;

	for (i=0; (msg[i] != '|' && msg[i] != ','); i++) {
		temp->ip[i] = msg[i];
	}

	temp->ip[i] = '\0';

	offset = i;
	i = 0;
	if (msg[offset++] == ',') {
		temp->next_source = (IpList*)malloc(sizeof(IpList));
		while (msg[offset+i] != '|') {
			sleep(1);
			temp->next_source->ip[i] = msg[offset+i];
			i++;
		}
		temp->next_source->ip[i] = '\0';
	}
	return temp;
}


//TODO: Add malloc error messages to entire project

IpList* ExtractTargetFromHeader(char *msg) {
	IpList* temp = (IpList*)malloc(sizeof(IpList));
	temp->next_source = NULL;
	int i;
	int offset;

	msg = strstr(msg,"|");
	msg++;
	if (msg == NULL) {
		__android_log_print(ANDROID_LOG_INFO, "Error",  "ExtractTargetFromHeader():strstr fail");
	}

	for (i=0; (msg[i] != ';' && msg[i] != ','); i++) {
		temp->ip[i] = msg[i];
	}


	temp->ip[i] = '\0';

	offset = i;
	i = 0;
	if (msg[offset++] == ',') {
		temp->next_source = (IpList*)malloc(sizeof(IpList));
		while (msg[offset+i] != ';') {
			sleep(1);
			temp->next_source->ip[i] = msg[offset+i];
			i++;
		}
		temp->next_source->ip[i] = '\0';
	}
	return temp;
}

char* ExtractTarget2nFromHeader(char *msg) {				// TODO: Check if we can unite this and ExtractTargetFromHeader
	char* temp = (char*)malloc(sizeof(char)*20);
	int i;

	char* strstrptr = strstr(msg,"|");
	if (strstrptr == NULL) {
		__android_log_print(ANDROID_LOG_INFO, "Error", "ExtractTargetFromHeader(): Did not find '|' in the message [%s]",msg);
		return NULL;
	}
	strstrptr++;
	for (i=0; strstrptr[i] != ';'; i++) {
		temp[i] = strstrptr[i];
	}
	temp[i] = '\0';
	return temp;
}

int GetIntFromString(char* str_number) {
	int i;
	int factor = 1;
	int digit;
	int number_to_return = 0;

	for (i=strlen(str_number)-1; i>=0; i--) {
		digit = str_number[i] - '0';
		if (digit > 9 || digit < 0) {
			__android_log_print(ANDROID_LOG_INFO, "Error", "GetIntFromString(): Invalid number represented by string [%s]",str_number);
			return -1;
		}
		number_to_return += digit*factor;
		factor = factor*10;
	}
	return number_to_return;
}

IpList* GetSourceFromString(char* msg){

	char* strstrptr;
	char* int_as_string = (char*)malloc(sizeof(char)* 20);
	if (int_as_string == NULL) {
		return NULL;
	}
	IpList* head_of_source_list = (IpList*)malloc(sizeof(IpList));
	head_of_source_list->next_source = NULL;
	IpList* source_to_return = head_of_source_list;
	int i=0;
	strstrptr = strstr(msg, "{")+1;
	while (strstrptr[i] != '}') {					// while there are still XOR'd sources
		i = 0;

		source_to_return->this_source_was_decoded_from_encoded_msg = FALSE; //init as false, only Decode() should change this to true
		///////////////////// PARSE SOURCE IP/////////////////////////////
		while (strstrptr[i] != '-') {				// read the source ip, it ends with '-'
			source_to_return->ip[i] = strstrptr[i];
			i++;
		}
		source_to_return->ip[i++] = '\0';

		///////////////////// PARSE SOURCE MSG INDEX/////////////////////////////
		strstrptr = strstrptr + i; // Add offset to strstrptr
		i = 0;
		while (strstrptr[i] != ':' && strstrptr[i] != '}') {				// now read the source index, it ends with ':' or '}'
			int_as_string[i] = strstrptr[i];
			i++;
		}
		int_as_string[i++] = '\0';
		source_to_return->index = GetIntFromString(int_as_string);

		///////////////////// PARSE SOURCE MSG LEN/////////////////////////////
		strstrptr = strstrptr + i; // Add offset to strstrptr
		i = 0;
		while (strstrptr[i] != ',' && strstrptr[i] != '}') {				// now read the source len, it ends with ',' or '}'
			int_as_string[i] = strstrptr[i];
			i++;
		}
		int_as_string[i] = '\0';
		source_to_return->msg_len = GetIntFromString(int_as_string);

//		__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c", "GetSourceFromString(): Source found [%s]-[%d]",source_to_return->ip,source_to_return->index);
		if (strstrptr[i] == ','){		// if there's a ','
			source_to_return->next_source = (IpList*)malloc(sizeof(IpList));
			source_to_return = source_to_return->next_source;
			i++;
			strstrptr = strstrptr + i;
		} else {
			source_to_return->next_source = NULL;
		}
	}
	free(int_as_string);
	return head_of_source_list;
}


void Java_com_example_adhocktest_BufferHandler_RunSendingDaemonJNI(JNIEnv* env1, jobject thiz) {

	/// TODO: We do not support relays that are also sources, and vice-versa in network coding yet

	__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c", "RunSendingDaemonJNI(): Started.");
	MemberInNetwork* Member;
	MemberInNetwork* ReadyMember1 = NULL;
	MemberInNetwork* ReadyMember2 = NULL;

	IpList* sources;
	int ready_to_send = FALSE;
	int ready_to_send_prev_state = FALSE;
	char encoded_msg[BUFLEN];

	//// debug and such, delete variables and all logic related
	int _is_broadcast = FALSE;								//
	int entered_nw_coding = FALSE;							//
	int entered_sect_1 = FALSE;								//
	//////////////////////////////////////////////////////////

	int my_turn = FALSE;
	int retval;

	char send_buf[BUFLEN]; // TODO: Consider changing to BUFLEN+HEADER_LEN
	while (1) { // TODO: Check this double while(1) loop

//		while (pause_daemon) { // TODO: Add a daemon paused/unpaused print
//			daemon_paused = TRUE;
//		}
//		daemon_paused = FALSE;

		Member = AllNetworkMembersList->FirstMember;

		while (Member != NULL) {

			entered_sect_1 = FALSE; // todo: delete
			if (Member->MemberBuffer[Member->current_index_to_send].is_valid == TRUE && Member->MemberBuffer[Member->current_index_to_send].was_sent == FALSE) 	{ // TODO: This is a temporary condition for NW_coding, change it

				// sect1
				entered_sect_1 = TRUE; // todo:delete
				entered_nw_coding = FALSE; // TODO: Delete
				if (network_coding_on && Member->MemberBuffer[Member->current_index_to_send].is_source == FALSE) {
					entered_nw_coding = TRUE;

					if (ReadyMember1 == NULL) {
						ReadyMember1 = Member;
					} else if (ReadyMember2 == NULL ) {
						if (strcmp(Member->node_ip,ReadyMember1->node_ip)!=0) { // Only if we found another member with a message to send who is not identical to the previous one
							ReadyMember2 = Member;

							__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RunSendingDaemonJNI(): Going to send [%s-%d] source?[%d] XOR [%s-%d] source?[%d]",
									ReadyMember1->node_ip,
									ReadyMember1->current_index_to_send,
									ReadyMember1->MemberBuffer[ReadyMember1->current_index_to_send].is_source,
									ReadyMember2->node_ip,
									ReadyMember2->current_index_to_send,
									ReadyMember2->MemberBuffer[ReadyMember2->current_index_to_send].is_source);
							BuildHeader(send_buf, ReadyMember1, ReadyMember2);
							ready_to_send_prev_state = ready_to_send;
							ready_to_send = TRUE;
						}
					}
				} else {
					BuildHeader(send_buf, Member, Member); // TODO: Second member will not be used, in the future it will be for network coding second msg
					ready_to_send_prev_state = ready_to_send;
					ready_to_send = TRUE;

					entered_nw_coding = FALSE;
				}



				if (ready_to_send) {
					//// sending block
					/////////////////////////////////////

					sources = GetSourceFromString(send_buf);


					///////////////////////////////////////////////////////////////////////////////////////////////////////////////////// // TODO: Delete
					__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RunSendingDaemonJNI(): ready to send"); // TODO: Delete
					__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RunSendingDaemonJNI(): send_buf=[%s]",send_buf);

					if (sources != NULL) {
						__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RunSendingDaemonJNI(): IpList_1=[%s]",sources->ip);
						if (sources->next_source != NULL) {
							__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RunSendingDaemonJNI(): IpList_2=[%s]",sources->next_source->ip);

						}
					}
					/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
					if (NeedToBroadcast(send_buf) == FALSE) {
						retval = sendto(sock_uni_tx, send_buf, strlen(send_buf)+1, 0, (struct sockaddr*)&servaddr_uni_tx, sizeof(servaddr_uni_tx));
					} else {
						__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RunSendingDaemonJNI(): send_buf=[%s]",send_buf);
						retval = sendto(sock_broad_tx, send_buf, Max(sources->msg_len,sources->next_source->msg_len), 0, (struct sockaddr*)&servaddr_broad_tx, sizeof(servaddr_broad_tx));
					}
					if ( retval < 0) {
						__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RunSendingDaemonJNI(): sendto failed with %d. message was [%20s]", errno,send_buf);
					} else {
						__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RunSendingDaemonJNI(): sendto() Success (retval=%d member=[%s] messge=[%20s] size=%d ", retval, Member->node_ip, send_buf,strlen(send_buf));
					}

					///// prepare for next iteration
					//////////////////////////////////
					if (network_coding_on && Member->MemberBuffer[Member->current_index_to_send].is_source == FALSE) { // TODO: Temporary workaround
						if (ReadyMember1 != NULL) {
							ReadyMember1->MemberBuffer[ReadyMember1->current_index_to_send].was_sent = TRUE;
							ReadyMember1->current_index_to_send = (ReadyMember1->current_index_to_send+1) % MAX_MSGS_IN_BUF;
						}
						if (ReadyMember2 != NULL) {
							ReadyMember2->MemberBuffer[ReadyMember2->current_index_to_send].was_sent = TRUE;
							ReadyMember2->current_index_to_send = (ReadyMember2->current_index_to_send+1) % MAX_MSGS_IN_BUF;
						}
					} else {
						Member->MemberBuffer[Member->current_index_to_send].was_sent = TRUE;
						Member->current_index_to_send = (Member->current_index_to_send+1) % MAX_MSGS_IN_BUF;
					}

					ReadyMember1 = NULL;
					ReadyMember2 = NULL;
					ready_to_send_prev_state = ready_to_send;
					ready_to_send = FALSE;
					///// INFO
					////////////////////////////////////////
					if (Member->current_index_to_send == MAX_MSGS_IN_BUF) {
						__android_log_print(ANDROID_LOG_INFO, "Buffers","RunSendingDaemonJNI(): Member=[%s] buffer going to wrap-around", Member->node_ip);
					}
				}
			}

//			if (ReadyMember1 != NULL && ReadyMember2 != NULL) {
//				__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RunSendingDaemonJNI(): Pre ready_to_send. entered_nwc:[%d] entered_sect1:[%d] ready_to_send:[%d], ReadyMember1 [%s] ReadyMember2[%s]", entered_nw_coding, entered_sect_1, ready_to_send_prev_state,ReadyMember1->node_ip,ReadyMember2->node_ip);
//			} else if (ReadyMember1 != NULL) {
//				__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RunSendingDaemonJNI(): Pre ready_to_send. entered_nw_coding:[%d] entered_sect1:[%d] ready_to_send:[%d], ReadyMember1 [%s] ReadyMember2[NULL] Member [%s] Member Cur Indx [%d] Member valid [%d] Member Sent [%d]", entered_nw_coding, entered_sect_1, ready_to_send_prev_state,ReadyMember1->node_ip, Member->node_ip,Member->current_index_to_send,Member->MemberBuffer[Member->current_index_to_send].is_valid,Member->MemberBuffer[Member->current_index_to_send].was_sent);
//			} else {
//				__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RunSendingDaemonJNI(): Pre ready_to_send. entered_nw_coding:[%d] entered_sect1:[%d] ready_to_send:[%d], Member [%s]", entered_nw_coding, entered_sect_1, ready_to_send_prev_state,Member->node_ip);
//			}



			usleep(5000);
			///////  Mary go round?
			////////////////////////////////////
			if (my_turn == TRUE) { 												// if i had my turn and now its over, go over network member buffers
				my_turn = FALSE;
				Member = AllNetworkMembersList->FirstMember;
			} else if (Member == AllNetworkMembersList->LastNetworkMember) {  	// if reached the end of members' buffers, it is my buffer's turn to send now
				my_turn = TRUE;
				Member = MyMemberInstance;
			} else {			// if going through the buffers of members list, go to the next node
				Member = Member->NextNode;
			}
		}
	}
}

int NeedToBroadcast(char* message) {

	IpList* list_of_next_hops = ExtractNextHopFromHeader(message);

	__android_log_print(ANDROID_LOG_INFO, "DEBUG_1",  "NeedToBroadcast(): Message [%s]", message);
	if (list_of_next_hops != NULL) {
		if (list_of_next_hops->next_source != NULL) {
			__android_log_print(ANDROID_LOG_INFO, "DEBUG_1",  "NeedToBroadcast(): returning TRUE");
			return TRUE;
		}
	}

	__android_log_print(ANDROID_LOG_INFO, "DEBUG_1",  "NeedToBroadcast(): returning FALSE");
	return FALSE;
}

void BuildHeader(char* send_buf, MemberInNetwork* BufferOwnerMember1, MemberInNetwork* BufferOwnerMember2) {

	MemberInNetwork* next_hop_ptr_1;
	MemberInNetwork* next_hop_ptr_2;
	MemberInNetwork* target_member_ptr_1;
	MemberInNetwork* target_member_ptr_2;
	int two_different_packets = FALSE;

	if (strcmp(BufferOwnerMember1->node_ip,BufferOwnerMember2->node_ip) != 0) {    // check if there's only 1 packet or 2 to be sent
		two_different_packets = TRUE;
	}

	/////////////////////////////////
	// Get both targets and next hops
	/////////////////////////////////
	target_member_ptr_1 = GetNode(BufferOwnerMember1->MemberBuffer[BufferOwnerMember1->current_index_to_send].target_ip, AllNetworkMembersList, 1);
	if (target_member_ptr_1 == NULL) {
		__android_log_print(ANDROID_LOG_INFO, "ERROR",  "RunSendingDaemonJNI(): Could not find target_member_ptr in MyNetworkMap");
	}
	next_hop_ptr_1 = GetNextHop(MyNetworkMap, target_member_ptr_1);
	if (next_hop_ptr_1 == NULL) {
		__android_log_print(ANDROID_LOG_INFO, "ERROR",  "RunSendingDaemonJNI(): Could not find next_hop_ptr in MyNetworkMap");
	}
	next_hop_ptr_1 = GetNextHop(MyNetworkMap, target_member_ptr_1);

	if (two_different_packets) {												// if there's another destination
		target_member_ptr_2 = GetNode(BufferOwnerMember2->MemberBuffer[BufferOwnerMember2->current_index_to_send].target_ip, AllNetworkMembersList, 1);
		if (target_member_ptr_2 == NULL) {
			__android_log_print(ANDROID_LOG_INFO, "ERROR",  "RunSendingDaemonJNI(): Could not find target_member_ptr in MyNetworkMap");
		}
		next_hop_ptr_2 = GetNextHop(MyNetworkMap, target_member_ptr_2);
		if (next_hop_ptr_2 == NULL) {
			__android_log_print(ANDROID_LOG_INFO, "ERROR",  "RunSendingDaemonJNI(): Could not find next_hop_ptr in MyNetworkMap");
		}
		next_hop_ptr_2 = GetNextHop(MyNetworkMap, target_member_ptr_2);
	}


	////////////////////////////////
	// Build header string
	////////////////////////////////
	char * strstrptr;

	// copied to BuildHeader

	/////////////////////////////////////////////////////////// NXT_HOP ////////////////////////////////////////////////////////////////////////
			////////////// NextHop towards target1 //////////
	strcpy(send_buf,next_hop_ptr_1->node_ip);
			////////////// NextHop towards target2 //////////
	if (two_different_packets == TRUE) {
		strcat(send_buf,",");
		strcat(send_buf,next_hop_ptr_2->node_ip);
	}
	strcat(send_buf,"|");
	/////////////////////////////////////////////////////////// TARGETS /////////////////////////////////////////////////////////////////////////
			////////////// First target ///////////////////
	strcat(send_buf,target_member_ptr_1->node_ip);
			////////////// Second Target ///////////////////
	if (two_different_packets == TRUE) {
		strcat(send_buf,",");
		strcat(send_buf,target_member_ptr_2->node_ip);
	}
	strcat(send_buf,";");
	/////////////////////////////////////////////////////////// SOURCES ////////////////////////////////////////////////////////////////////////
	strcat(send_buf,"XOR{");
			///////////// First source ///////////////
	strcat(send_buf,BufferOwnerMember1->node_ip);
	strcat(send_buf,"-");
	sprintf(strstr(send_buf,"-"),"-%d",BufferOwnerMember1->current_index_to_send);						// WARNING - No verification that strstrptr!=NULL
	strcat(send_buf,":");
	sprintf(strstr(send_buf,":"),":%d",BufferOwnerMember1->MemberBuffer[BufferOwnerMember1->current_index_to_send].msg_len);
	__android_log_print(ANDROID_LOG_INFO, "DEBUG_1",  "BuildHeader(): stage1 [%s]", send_buf);
			///////////// Second source ///////////////
	if (two_different_packets == TRUE) {
		strcat(send_buf,",");
		strcat(send_buf,BufferOwnerMember2->node_ip);
		strcat(send_buf,"-");
		strstrptr = strstr(send_buf,"-"); // find the '-' of the first source
		strstrptr = strstr(++strstrptr,"-"); // find the '-' of the second source (the one we're currently adding     	// WARNING - No verification that strstrptr!=NULL
		sprintf(strstrptr,"-%d",BufferOwnerMember2->current_index_to_send);
		strcat(send_buf,":");
		strstrptr = strstr(send_buf,":"); // find the ':' of the first source
		strstrptr = strstr(++strstrptr,":"); // find the ':' of the second source (the one we're currently adding     	// WARNING - No verification that strstrptr!=NULL
		sprintf(strstrptr,":%d",BufferOwnerMember2->MemberBuffer[BufferOwnerMember2->current_index_to_send].msg_len);
	}

	strcat(send_buf,"}");
	// /copied to buildHeader

	if (!two_different_packets) {
//	if (!NeedToBroadcast(send_buf)) {
		if ((inet_aton(next_hop_ptr_1->node_ip,&servaddr_uni_tx.sin_addr)) == 0) {
				close(sock_uni_tx);
				__android_log_print(ANDROID_LOG_INFO, "Error",  "SendUdpJNI():Cannot decode IP address");
				return;
		}
	}

	__android_log_print(ANDROID_LOG_INFO, "DEBUG_1",  "BuildHeader(): Final Header is [%s]", send_buf);
	strcat(send_buf,"~");


	char * only_data_part_of_msg;
	// 1. only_data_part_of_msg = Encode (msg1,msg2)
	if (two_different_packets) {
		only_data_part_of_msg = XorMessage(
				strstr(BufferOwnerMember1->MemberBuffer[BufferOwnerMember1->current_index_to_send].msg,"~")+1,
				strstr(BufferOwnerMember2->MemberBuffer[BufferOwnerMember2->current_index_to_send].msg,"~")+1,
				BufferOwnerMember1->MemberBuffer[BufferOwnerMember1->current_index_to_send].msg_len, // TODO: check that this len is true
				BufferOwnerMember2->MemberBuffer[BufferOwnerMember2->current_index_to_send].msg_len, "Encoding");

	} else {
		only_data_part_of_msg = strstr(BufferOwnerMember1->MemberBuffer[BufferOwnerMember1->current_index_to_send].msg,"~"); // TODO: When network coding, the msg should be the encoded one and not just of Member1
		if (only_data_part_of_msg != NULL) {
			only_data_part_of_msg++;
		} else {
			only_data_part_of_msg = BufferOwnerMember1->MemberBuffer[BufferOwnerMember1->current_index_to_send].msg;
		}


	}

	// 2. decode
	// 3. strcmp

//	char* decoded_msg;
//	IpList* sources_of_msg = GetSourceFromString(send_buf);
//	int compare1,compare2;
//	if (two_different_packets) {
//		strcat(send_buf,only_data_part_of_msg);
//		decoded_msg = Decode(only_data_part_of_msg, Max(BufferOwnerMember1->MemberBuffer[BufferOwnerMember1->current_index_to_send].msg_len,BufferOwnerMember2->MemberBuffer[BufferOwnerMember2->current_index_to_send].msg_len), sources_of_msg);
//		compare1 = strcmp(decoded_msg,strstr(BufferOwnerMember1->MemberBuffer[BufferOwnerMember1->current_index_to_send].msg,"~")+1);
//		compare2 = strcmp(decoded_msg,strstr(BufferOwnerMember2->MemberBuffer[BufferOwnerMember2->current_index_to_send].msg,"~")+1);
//
//		__android_log_print(ANDROID_LOG_INFO, "God",  "BuildHeader(): strcmp1=[%d] strcmp2[%d] ",compare1,compare2);
//		sleep(1);
//	}

	strcat(send_buf,only_data_part_of_msg);
	if (two_different_packets) {
		free(only_data_part_of_msg);
	}
}

//                                                         XOR{}~DATA
/////////////////////////////////////////
//// Network coding utility
/////////////////////////////////////////



char* Encode(char* msg_1, char* msg) {
	return "HOPA";
}

char* Decode(char* encoded_msg, int encoded_msg_len, IpList* Sources) {
	//1. perhaps need to validate message in my own buffer
	//2. encoded_msg contains only the ACTUAL message
	//3. i dont support having BOTH of the messages..
	//4. need to free the returned decoded_msg..

	char* decoded_msg;
	int am_i_a_source=FALSE;
	MemberInNetwork* temp_mem;
	IpList* source_alrdy_in_my_buffer;
	/// run on message check which index you need for other than yourself
	//check first source
	if (strcmp(Sources->ip,MyNetworkMap->node_base_ip)==0){ // i'm dest source1
		am_i_a_source=TRUE;
		__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "Decode(): Source1 ip [%s] is myself", Sources->ip);
		source_alrdy_in_my_buffer=Sources;
	} else {
		temp_mem = GetNode(Sources->ip,AllNetworkMembersList,1);
		if (temp_mem==NULL){
			__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "Decode(): Source1 [%s] is not in allmemberlist", Sources->ip);
			return NULL;
		} else if (temp_mem->MemberBuffer[Sources->next_source->index].is_valid==TRUE){
			//i have the message
			__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "Decode(): I have Source1 [%s] msg #%d in my buffer", Sources->ip,Sources->index);
			source_alrdy_in_my_buffer=Sources;
		}
	}

	//check second source
	if (strcmp(Sources->next_source->ip,MyNetworkMap->node_base_ip)==0){ // i'm dest source2
		am_i_a_source=TRUE;
		__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "Decode(): Source2 ip [%s] is myself", Sources->next_source->ip);
		source_alrdy_in_my_buffer=Sources->next_source;
	} else {
		temp_mem = GetNode(Sources->next_source->ip,AllNetworkMembersList,1);
		if (temp_mem==NULL){
			__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "Decode(): Source2 [%s] is not in allmemberlist", Sources->next_source->ip);
			return NULL;
		} else if (temp_mem->MemberBuffer[Sources->next_source->index].is_valid==TRUE){
			//i have the message
			__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "Decode(): I have Source2 [%s] msg #%d in my buffer", Sources->next_source->ip,Sources->next_source->index);
			source_alrdy_in_my_buffer=Sources->next_source;
		}
	}

	/// xor
	if (am_i_a_source==TRUE){
		decoded_msg = XorMessage(encoded_msg,  strstr(MyMemberInstance->MemberBuffer[source_alrdy_in_my_buffer->index].msg,"~")+1 , encoded_msg_len, source_alrdy_in_my_buffer->msg_len, "Decoding as source" );
	} else {		decoded_msg = XorMessage(encoded_msg,  strstr(temp_mem->MemberBuffer[source_alrdy_in_my_buffer->index].msg,"~")+1 , encoded_msg_len, source_alrdy_in_my_buffer->msg_len, "Decoding as relay");
	}
	//maybe will allocate memory for mymessage and free here

	/// return result

	return decoded_msg;
}

char* XorMessage(char* msg_1, char* msg_2,int len_1,int len_2, char* debug_message) {

	char* xor_result;
	int i = 0;
	int max,min;

	max = len_1 > len_2 ? len_1 : len_2;
	min = len_1 > len_2 ? len_2 : len_1;

	__android_log_print(ANDROID_LOG_INFO, "God",  "XorMessage(): Entered function for %s",debug_message);

	xor_result = (char*)malloc(sizeof(char)*(max + 1));
	CheckCharMalloc(xor_result, "XorMessage", "xor_result");
//	printf("msg_1 strlen = %d , msg_2 strlen = %d ", len_1, len_2);


//	if (msg_1 != NULL) {
//		__android_log_print(ANDROID_LOG_INFO, "God",  "XorMessage(): msg1=[%s]",msg_1);
//	} else {
//		__android_log_print(ANDROID_LOG_INFO, "God",  "XorMessage(): msg1=[NULL]");
//	}
//
//	if (msg_2 != NULL) {
//		__android_log_print(ANDROID_LOG_INFO, "God",  "XorMessage(): msg2=[%s]",msg_2);
//	} else {
//		__android_log_print(ANDROID_LOG_INFO, "God",  "XorMessage(): msg2=[NULL]");
//	}
	while (i < min){
		xor_result[i] = msg_1[i] ^ msg_2[i];
		i++;
	}

	if (len_1>len_2){
		while (i < max){
			xor_result[i] = msg_1[i] ^ ' ';
			i++;
		}
	} else {
		while (i < max){
			xor_result[i] = msg_2[i] ^ ' ';
			i++;
		}
	}
	xor_result[i] = '\0';

	return xor_result;
}

void FlushBuffer(char * ip_of_member_to_flush) {
	int i;
	MemberInNetwork * member_to_flush = GetNode(ip_of_member_to_flush, AllNetworkMembersList, 1);
	if (member_to_flush != NULL) {
		for (i=0; i<MAX_MSGS_IN_BUF; i++) {
			member_to_flush->MemberBuffer[i].is_valid = FALSE;
		}
	}
}

int Max(int num1, int num2) {
	return (num1 > num2 ? num1 : num2);
}

void CheckCharMalloc(char* char_to_check, char* function_name,  char* var_name) {
	if (char_to_check == NULL) {
		__android_log_print(ANDROID_LOG_INFO, "Error",  "%s(): Failed to allocate %s",function_name,var_name);
	}
}

int ImInTheIpList(IpList* list_to_check) {
	if (strcmp(MyNetworkMap->node_base_ip,list_to_check->ip) == 0) {
		return TRUE;
	}

	if (list_to_check->next_source != NULL) {
		if (strcmp(MyNetworkMap->node_base_ip,list_to_check->next_source->ip) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

void FreeIpList(IpList* list_to_free) {
	if (list_to_free == NULL) {
		return;
	}

	while (list_to_free->next_source != NULL) {
		FreeIpList(list_to_free->next_source);
	}

//	free(list_to_free);
}
