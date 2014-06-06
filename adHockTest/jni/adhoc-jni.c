
#include <string.h>
#include <jni.h>
#include <android/log.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

// TODO: Keep trying to restart network interface if it doesnt work


/*
 *  Network coding theory
 *
 *  Next Hop |1 Target |2 Source |3 Indx ; message
 * strstr(buf,"|1");
 *
 *  No packet drop
 *  No disconnections
 *  Static 3 nodes line network coding
 *  always between 96 and 207 , with 22 as relay
 */
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
#define MAX_MSGS_IN_BUF 20

#define RX_PORT 8888
#define TX_PORT 8888

/////
//DEFINE MEMBER IN NETWORK
////////
//GLOBALS
////////
static const char HELLO_MSG[] = "HELLO_MSG:";

////SINGLE NODE
typedef struct SourceList{
	char* ip;
	int   index;
	struct SourceList* next_source;
} SourceList;

typedef struct Buffer{

	int msg_indx;
	char msg[BUFLEN];

	int was_broadcast; 			// Flag - Set:This message was already sent as a broadcast clear: it was not
	//int msg_len; // xor might cause unexpected '\0'
	int should_xor;

} Buffer;


typedef struct MemberInNetwork {
	int cur_node_distance;
	char* node_ip;
	struct MemberInNetwork * NextNode;
	struct MemberInNetwork * PrevNode;
	struct NetworkMap * SubNetwork;
	int CountdownTimer;
	Buffer MemberBuffer[MAX_MSGS_IN_BUF];
	int buf_index;


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

int network_coding_on = FALSE;
/*
 *  FUNCTION DECLARATIONS
 */
void InitializeSockets();
void GenerateHelloMsg(char* base_string, NetworkMap* network_head);
jint Java_com_example_adhocktest_Routing_InitializeMap(JNIEnv* env1, jobject thiz,jstring ip_to_init);
MemberInNetwork* GetNode(char* node_ip_to_check, NetworkMap * Network_Head,int maximum_distance_from_originator);
int UpdateNodeTimer(MemberInNetwork* Node_to_update, int Countdown);
int AddToNetworkMap(char* node_ip,NetworkMap * Network_Head);
int RemoveFromNetworkMap(NetworkMap * network_to_remove_from, MemberInNetwork* member_to_remove, int network_is_members_list);
int DoesNodeExist(char* ip_to_check, NetworkMap* Network_to_check);
void Java_com_example_adhocktest_SenderUDP_SendUdpJNI( JNIEnv* env, jobject thiz, jstring ip,jint port, jstring message, jint is_broadcast);
void SendUdpJNI(const char* ip, int port, const char* message, int is_broadcast, int is_source, int msg_len);
jstring Java_com_example_adhocktest_ReceiverUDP_RecvUdpJNI(JNIEnv* env1, jobject thiz);
void ProcessHelloMsg(char* buf,int buf_length,NetworkMap* network_to_add_to);
jstring Java_com_example_adhocktest_Routing_RefreshNetworkMapJNI(JNIEnv* env1, jobject thiz);
void RefreshNetworkMap(NetworkMap* network_to_refresh);
MemberInNetwork* GetNextHop(NetworkMap* network_to_search ,MemberInNetwork* final_destination);
int IsNodeForbidden(char* ip_to_check);
//void AddMsgToBuffer(MemberInNetwork* MemberBuffer, const char* msg,int msg_indx,int msg_len, char* target_ip);
int IsHelloMsg(char* msg);
char* ExtractNextHopFromHeader(char *msg);
char* ExtractTargetFromHeader(char *msg);
//char* ExtractSourceFromHeader(char *msg); // TODO: Implement
int GetIntFromString(char* str_number);
SourceList* GetSourceFromString(char* msg);

/*
 * FUNCTION IMPLEMENTATION
 */

///////////////
// Global SendUdp Socket
///////////////
int sock_fd_tx;
struct sockaddr_in servaddr_tx;
int sock_fd_rx;
struct sockaddr_in servaddr_rx;

void InitializeSockets() {

	int is_broadcast = TRUE; // always broadcast
	int retval;
	////////////////
	/// create socket
	////////////////
	if (( sock_fd_tx = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		__android_log_print(ANDROID_LOG_INFO, "Error",  "SendUdpJNI(): Cannot create socket, error %d", errno);
		return;
	}

	/////////////////
	// set socket options
	/////////////////
	int ret=setsockopt(sock_fd_tx, SOL_SOCKET, SO_BROADCAST, &is_broadcast, sizeof(is_broadcast));
	if (ret) {
		close(sock_fd_tx);
		__android_log_print(ANDROID_LOG_INFO, "Error",  "SendUdpJNI(): Failed to set setsockopt(), error: %d", errno);
		return;
	}

	memset((char*)&servaddr_tx, 0, sizeof(servaddr_tx));
	servaddr_tx.sin_family = AF_INET;
	servaddr_tx.sin_port = htons(TX_PORT);

	__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "SendUdpJNI(): sock_fd_tx values = %d", sock_fd_tx);

//	if ((inet_aton("192.168.2.255",&servaddr_tx.sin_addr)) == 0) {
////		free(send_buf);
//		close(sock_fd_tx);
//		__android_log_print(ANDROID_LOG_INFO, "Error",  "SendUdpJNI():Cannot decode IP address");
//		return;
//	}
//	servaddr_tx.sin_addr.s_addr |= 0xff000000; // always broadcast!
	servaddr_tx.sin_addr.s_addr = htonl(INADDR_ANY);

		////////////// RX /////////////////////
		/////////
		// Create and bind socket
		/////////
		struct sockaddr_in servaddr_rx;

		if ((sock_fd_rx = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) {
			int temp = errno;
			__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): socket() call failed. errno : %d ",temp);
		}


		bzero(&servaddr_rx, sizeof(servaddr_rx));
		servaddr_rx.sin_family = AF_INET;
		servaddr_rx.sin_port = htons(RX_PORT);
		servaddr_rx.sin_addr.s_addr = htonl(INADDR_ANY);

		if (bind(sock_fd_rx, (struct sockaddr* ) &servaddr_rx, sizeof(servaddr_rx))==-1) {
			__android_log_print(ANDROID_LOG_INFO, "Error", "RecvUdpJNI() Failed to bind");
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

//	return FALSE;// TODO: Temporary debug , delete this line

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

	InitializeSockets();

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
		AddToNetworkMap("192.168.2.22",ForbiddenNodesToAdd);
		AddToNetworkMap("192.168.2.207",ForbiddenNodesToAdd);
	} else if (strcmp(MyNetworkMap->node_base_ip ,"192.168.2.96")==0) {
		AddToNetworkMap("192.168.2.207",ForbiddenNodesToAdd);
	} else if (strcmp(MyNetworkMap->node_base_ip ,"192.168.2.22")==0) {
		AddToNetworkMap("192.168.2.33",ForbiddenNodesToAdd);
	} else if (strcmp(MyNetworkMap->node_base_ip ,"192.168.2.207")==0) {
		AddToNetworkMap("192.168.2.33",ForbiddenNodesToAdd);
		AddToNetworkMap("192.168.2.96",ForbiddenNodesToAdd);
	}

	__android_log_print(ANDROID_LOG_INFO, "NetworkMap","InitializeMap(): completed successfully. My IP : [%s]", MyNetworkMap->node_base_ip);

	return 0;

	/*
	 *   33 <-> 96 <-> 22 <-> 207
	 */
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
	UpdateNodeTimer(temp,12);
	return 0;
}

/*
 * RemoveFromNetworkMap : receive MemberInNetwork to remove
 */
int RemoveFromNetworkMap(NetworkMap * network_to_remove_from, MemberInNetwork* member_to_remove, int network_is_members_list){

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
	free(member_to_remove->node_ip);
	free(member_to_remove->SubNetwork);
	free(member_to_remove);
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
                                                  jobject thiz, jstring ip,jint port, jstring j_message, jint is_broadcast)
{

	const char *_ip = (*env)->GetStringUTFChars(env, ip, 0);
	const char *message = (*env)->GetStringUTFChars(env, j_message, 0);   // Message to be sent

	SendUdpJNI(_ip,port,message,is_broadcast,TRUE,strlen(message));
	(*env)->ReleaseStringUTFChars(env,j_message,message);
	(*env)->ReleaseStringUTFChars(env,ip,_ip);
}

void SendUdpJNI(const char* _ip, int port, const char* message, int is_broadcast, int is_source, int msg_len) {

	int retval;
	char send_buf[BUFLEN];
	static int msg_index = 0;
	__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "SendUdpJNI(): Message is [%s]", message);

	char* next_hop_ip;
	char final_dest_ip[20];

//	 set ip to sub-net	```````````````````````````work broadcast
	if ((inet_aton(_ip,&servaddr_tx.sin_addr)) == 0) {
//		free(send_buf);
		close(sock_fd_tx);
		__android_log_print(ANDROID_LOG_INFO, "Error",  "SendUdpJNI():Cannot decode IP address");
		return;
	}
	int _is_broadcast;

	////////////////
	/// send
	////////////////
	char hello_message_string[HELLO_MSG_LEN];

	if (strcmp(message, "HELLO_MSG:")==0) {

		_is_broadcast = TRUE;
		servaddr_tx.sin_addr.s_addr |= 0xff000000; // always broadcast!
		setsockopt(sock_fd_tx, SOL_SOCKET, SO_BROADCAST, &_is_broadcast, sizeof(_is_broadcast));

		strcpy(hello_message_string,"HELLO_MSG:");
		strcat(hello_message_string,MyNetworkMap->node_base_ip);
		strcat(hello_message_string,"(");
		GenerateHelloMsg(hello_message_string,MyNetworkMap);
		strcat(hello_message_string,")");

		retval = sendto(sock_fd_tx, hello_message_string, strlen(hello_message_string), 0, (struct sockaddr*)&servaddr_tx, sizeof(servaddr_tx));
		if ( retval < 0) {
			__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "SendUdpJNI(): sendto failed with %d. message was [%s]", errno,hello_message_string);
		} else {
			__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "SendUdpJNI(): sendto() Success (retval=%d messge='%s' size=%d ip=%s", retval,hello_message_string,strlen(hello_message_string),_ip);
		}
	} else {
		/////////////////
		// Get next hop
		/////////////////




		strcpy(final_dest_ip,_ip);
		MemberInNetwork* temp_member = GetNode(final_dest_ip, AllNetworkMembersList, 1);
		if (temp_member != NULL) {
			temp_member = GetNextHop(MyNetworkMap,temp_member);
			if (temp_member != NULL) {
				next_hop_ip = temp_member->node_ip;

				// send directly to next hop
				if ((inet_aton(next_hop_ip,&servaddr_tx.sin_addr)) == 0) {
		//						free(send_buf);
					close(sock_fd_tx);
					__android_log_print(ANDROID_LOG_INFO, "Error",  "SendUdpJNI():Cannot decode IP address");
					return;
				}
				if (is_source) {
					_is_broadcast = FALSE;
				} else {
					_is_broadcast = TRUE;
					servaddr_tx.sin_addr.s_addr |= 0xff000000; // always broadcast!
				}
				setsockopt(sock_fd_tx, SOL_SOCKET, SO_BROADCAST, &_is_broadcast, sizeof(_is_broadcast));

				strcpy(send_buf,next_hop_ip);
				strcat(send_buf,"|");
				strcat(send_buf,_ip);
				strcat(send_buf,";XOR{");
				strcat(send_buf,MyNetworkMap->node_base_ip);
				strcat(send_buf,"-");
				sprintf(strstr(send_buf,"-"),"-%d",msg_index++);
				strcat(send_buf,"}");
				if (is_source == TRUE) {
						strcat(send_buf,"~");
				}
				strcat(send_buf,message);
			} else {
				__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "SendUdpJNI(): failed to find next hop. Returning");
			}
		}

		retval = sendto(sock_fd_tx, send_buf, strlen(send_buf)+1, 0, (struct sockaddr*)&servaddr_tx, sizeof(servaddr_tx));
		if ( retval < 0) {
			__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "SendUdpJNI(): sendto failed with %d. message was [%s]", errno,send_buf);
		} else {
			__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "SendUdpJNI(): sendto() Success (retval=%d messge='%s' size=%d ip=%s", retval,send_buf,strlen(send_buf),_ip);
		}
	}
//	close(sock_fd_tx); // no longer need to close socket since its global now
}

jstring
Java_com_example_adhocktest_ReceiverUDP_RecvUdpJNI(JNIEnv* env1,
        jobject thiz)
{
	int retVal=-1;
	int is_ignore_msg = 0;
	int is_forward_msg = 0;

	char return_str[BUFLEN+30];
	char* next_hop_from_header;
	char* target_from_header;


	///////////
	/// Receive UDP packets
	//////////
	//try to receive
	char buf[BUFLEN];
	char* strstrptr;

	struct sockaddr_in cli_addr;
	socklen_t slen=sizeof(cli_addr);
	int retval = recvfrom(sock_fd_rx, buf, BUFLEN, 0, (struct sockaddr*)&cli_addr, &slen);
	if (retval==-1) {
		__android_log_print(ANDROID_LOG_INFO, "Error",  "RecvUdpJNI() Failed to recvfrom() retval=%d",retval);
		exit(0); // TODO : Delete
		return (*env1)->NewStringUTF(env1, "ignore");
	} else {
		buf[retval] = '\0';
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

		target_from_header = ExtractTargetFromHeader(buf);
		next_hop_from_header = ExtractNextHopFromHeader(buf);
		if (target_from_header==NULL || next_hop_from_header == NULL){
			__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): Bad message, ignoring [%s]",buf);
			is_ignore_msg = TRUE;
		} else {
			if (strcmp(MyNetworkMap->node_base_ip,target_from_header) == 0) {        		// i am the target of the msg
				__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): I am the target of this message");
			} else if (strcmp(MyNetworkMap->node_base_ip,next_hop_from_header) == 0){ 		// i am the next hop
				is_forward_msg = TRUE;

				__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): I am the next hop of this message. forwarding [%s]",buf);
				strstrptr = strstr(buf,";");
				if (strstrptr == NULL) {
					__android_log_print(ANDROID_LOG_INFO, "Error",  "RecvUdpJNI(): Did not find ';', Ignoring message.");
					free(next_hop_from_header);
					free(target_from_header);
					return (*env1)->NewStringUTF(env1, "ignore"); // return ignore
				}

				__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): target_ip extracted before calling SendUdpJNI: <%s>",target_from_header);
				SendUdpJNI(target_from_header,TX_PORT,strstrptr+1,1,FALSE,strlen(strstrptr+1)); // TODO: When we add XOR we have to use the message length integer instead of strlen()
				__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): Back from forwarding1");
				sprintf(return_str, "forwarding message [%s]",buf); // TODO : Check if we can bring it back
				__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): Back from forwarding2");

			} else { 	 						// I am neither the next hop or target
				is_ignore_msg = TRUE;
			}
		}
		//////
		// Free memhttps://scontent-b-fra.xx.fbcdn.net/hphotos-prn2/t1.0-9/1174991_10202254138181241_545136069_n.jpg
		//////
		if (next_hop_from_header != NULL) {
			free(next_hop_from_header);
		}
		if (target_from_header != NULL) {
			free(target_from_header);
		}
	}


	__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): Raw1 string received: <%s>",buf);

	//if received successfully , close socket
	if (is_ignore_msg == TRUE) {
		return (*env1)->NewStringUTF(env1, "ignore"); // return ignore
	} else if (is_forward_msg) {
		return (*env1)->NewStringUTF(env1, "ignore"); // TODO: Make this return some forward feedback
	} else {
		strstrptr = strstr(buf,"}~"); 	// first try to check if this is an image
		if (strstrptr == NULL) {		// if its not image nor text just return something
			return (*env1)->NewStringUTF(env1, buf);
		} else {
			return (*env1)->NewStringUTF(env1, strstrptr+1); // return image or text leaving the ~ or ! char respectively
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

//void AddMsgToBuffer(MemberInNetwork* MemberBuffer, const char* msg,int msg_indx, int msg_len, char* target_ip){
//
//	__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c","AddMsgToBuffer(): Need to add msg : [%s] with index #%d to [%s]", msg,msg_indx,MemberBuffer->node_ip);
//
//	Buffer* temp;
//	Buffer* search_end;
//
//	temp = (Buffer*)malloc(sizeof(Buffer));
//	if (temp==NULL){
//		__android_log_print(ANDROID_LOG_INFO, "Error","AddMsgToBuffer(): Failed to allocate memory for Buffer... ;");
//		return;
//	}
//	temp->target_ip = (char*)malloc(sizeof(char)*20);
//	if (temp->target_ip==NULL){
//		__android_log_print(ANDROID_LOG_INFO, "Error","AddMsgToBuffer(): Failed to allocate memory for temp->target_ip... ;");
//		free(temp->target_ip);
//		return;
//	}
//
//	strcpy(temp->target_ip,target_ip);
//	temp->was_broadcast = FALSE;
//	temp->msg_indx = msg_indx;
//	temp->next_buf_msg = NULL;
//	temp->msg=(char*)malloc(sizeof(char)*(1+msg_len));
//	if (temp->msg==NULL){
//		__android_log_print(ANDROID_LOG_INFO, "Error_","AddMsgToBuffer(): Failed to allocate memory for msg in buffer... ;");
//		free(temp->target_ip);
//		free(temp);
//		return;
//	}
//	strcpy(temp->msg,msg);
//
//	search_end = MemberBuffer->msg_buffer;
//	if (search_end == NULL){
//		__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c","AddMsgToBuffer(): Buffer is empty");
//		MemberBuffer->msg_buffer = temp;
//	} else{
//		__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c","AddMsgToBuffer(): Buffer is not empty - reaching end of buffer list");
//		while (search_end->next_buf_msg != NULL){
//			search_end=search_end->next_buf_msg;
//		}
//		search_end->next_buf_msg = temp;
//	}
//
//	temp = MyMemberBuffer->msg_buffer;
//	while (temp!=NULL) {
//		__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c", "AddMsgToBuffer(): [%d:%s]",temp->msg_indx,temp->msg);
//		temp=temp->next_buf_msg;
//	}
//
//
//}

int IsHelloMsg(char* msg) {
	if (strstr(msg,HELLO_MSG)==NULL) {
		return FALSE;
	} else {
		return TRUE;
	}
}

char* ExtractNextHopFromHeader(char *msg) {
	char* temp = (char*)malloc(sizeof(char)*20);
	int i;

	for (i=0; msg[i] != '|'; i++) {
		temp[i] = msg[i];
		if (i>20) {
			free(temp);
			return NULL;
		}
	}
	temp[i] = '\0';
	return temp;
}

char* ExtractTargetFromHeader(char *msg) {
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

SourceList* GetSourceFromString(char* msg){

	char* strstrptr;
	char* temp_source_index_as_string = (char*)malloc(sizeof(char)* 20);
	if (temp_source_index_as_string == NULL) {
		return NULL;
	}
	SourceList* head_of_source_list = (SourceList*)malloc(sizeof(SourceList));
	head_of_source_list->ip = (char*)malloc(sizeof(char)* 20);
	head_of_source_list->next_source = NULL;
	SourceList* source_to_return = head_of_source_list;
	int i=0;
	strstrptr = strstr(msg, "{")+1;
	while (strstrptr[i] != '}') {					// while there are still XOR'd sources
		i = 0;
		while (strstrptr[i] != '-') {				// read the source ip, it ends with '-'
			source_to_return->ip[i] = strstrptr[i];
			i++;
		}
		source_to_return->ip[i++] = '\0';
		strstrptr = strstrptr + i; // Add offset to strstrptr

		i = 0;
		while (strstrptr[i] != ',' && strstrptr[i] != '}') {				// now read the source index, it ends with ',' or '}'
			temp_source_index_as_string[i] = strstrptr[i];
			i++;
		}
		temp_source_index_as_string[i] = '\0';
		source_to_return->index = GetIntFromString(temp_source_index_as_string);

		__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c", "GetSourceFromString(): Source found [%s]-[%d]",source_to_return->ip,source_to_return->index);
		if (strstrptr[i] != '}'){
			source_to_return->next_source = (SourceList*)malloc(sizeof(SourceList));
			source_to_return->next_source->ip = (char*)malloc(sizeof(char)* 20);
			source_to_return = source_to_return->next_source;
			i++;
			strstrptr = strstrptr + i;
		}
		else{
			source_to_return->next_source = NULL;
		}
	}
	free(temp_source_index_as_string);
	return head_of_source_list;
}

//char* xorString(char* str1, char* str2,int len1,int len2){
//
//	char* xor;
//	int i = 0;
//	int max,min;
//
//	max = len1 > len2 ? len1 : len2;
//	min = len1 > len2 ? len2 : len1;
//
//	xor = (char*)malloc(sizeof(char)*(max + 1));
//
//	printf("str1 strlen = %d , str2 strlen = %d ", len1, len2);
//
//	while (i < min){
//		xor[i] = str1[i] ^ str2[i];
//		i++;
//	}
//
//	if (len1>len2){
//		while (i < max){
//			xor[i] = str1[i] ^ ' ';
//			i++;
//		}
//	}
//	else{
//		while (i < max){
//			xor[i] = str2[i] ^ ' ';
//			i++;
//		}
//	}
//	xor[i] = '\0';
//
//	return xor;
//}

