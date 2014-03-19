/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
/*
#include <string.h>
#include <jni.h>
#include <android/log.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
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

/////
//DEFINE MEMBER IN NETWORK
////////
//GLOBALS
////////
static const char HELLO_MSG[] = "HELLO_MSG:";

////SINGLE NODE
typedef struct MemberInNetwork {
	int cur_node_distance;
//	int time_stamp; TODO: Make this part of the C struct
	char* node_ip;
	struct MemberInNetwork * NextNode;
	struct MemberInNetwork * PrevNode;
	struct NetworkMap * SubNetwork;
	//need to add more parameters such as last received etc
} MemberInNetwork;

////NETWORK MAP
typedef struct NetworkMap {
	int num_of_nodes;
	MemberInNetwork* FirstMember;
	MemberInNetwork* LastNetworkMember;
	char node_base_ip[16];
} NetworkMap;

NetworkMap* MyNetworkMap;
NetworkMap* AllNetworkMembersList;
int tempGlobal;




void GenerateHelloMsg(char* base_string, NetworkMap* network_head) {

	MemberInNetwork* temp = network_head->FirstMember;
	__android_log_print(ANDROID_LOG_INFO, "NetworkMap","The String so far is [%s]",base_string);
	while (temp!=NULL) {
		strcat(base_string,temp->node_ip);
		strcat(base_string,"(");
		GenerateHelloMsg(base_string, temp->SubNetwork);
		strcat(base_string,")");
		temp = temp->NextNode;
	}
}

/*
 * InitializeMap.
 */
jint
Java_com_example_adhocktest_Routing_InitializeMap(JNIEnv* env1,
        jobject thiz,jstring ip_to_init){

	char* buffer = "gal(ben(lior()))leslie(dudu())";

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
			return -1;
	}
	//no allocation for base ip
	AllNetworkMembersList->num_of_nodes=0;
	AllNetworkMembersList->FirstMember=NULL;
	AllNetworkMembersList->LastNetworkMember=NULL;

	__android_log_print(ANDROID_LOG_INFO, "NetworkMap","InitializeMap(): completed successfully. My IP : [%s]", MyNetworkMap->node_base_ip);

	return 0;

}

/*
 * AddToNetworkMap : receive node_ip, node_type - add the data to the network map
 */
int AddToNetworkMap(char* node_ip,NetworkMap * Network_Head){

	if (DoesNodeExist(node_ip, Network_Head) == FALSE) {
		__android_log_print(ANDROID_LOG_INFO, "NetworkMap","AddToNetworkMap(): Need to add node with ip :[%s] to network map with base ip : [%s]", node_ip,Network_Head->node_base_ip);

		MemberInNetwork * temp = (MemberInNetwork*)malloc(sizeof(MemberInNetwork));
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
		/*
		 * add subnetwork .
		 * allocate memory
		 * run over buf and get IPs
		 * call AddToNetworkMap
		 */
	//	tempGlobal++;
	//	if (tempGlobal < 2)
	//		CheckNodeExistence("192.168.2.222",NULL,temp->SubNetwork,NEIGHBOUR_OF_NEIGHBOUR);
		__android_log_write(ANDROID_LOG_INFO, "NetworkMap","AddToNetworkMap(): Success.");
	} else {
		__android_log_print(ANDROID_LOG_INFO, "NetworkMap","AddToNetworkMap(): Node with ip :[%s] already exists in this level.", node_ip);
	}
	return 0;

}




/*
 * RemoveFromNetworkMap : receive MemberInNetwork to remove
 */
int RemoveFromNetworkMap(MemberInNetwork * member_to_remove){

	__android_log_print(ANDROID_LOG_INFO, "NetworkMap","RemoveFromNetworkMap(): Received a request to remove node: [%s]", member_to_remove->node_ip);

	//check if there is only one member in network
	if (MyNetworkMap->FirstMember == MyNetworkMap->LastNetworkMember){

		__android_log_write(ANDROID_LOG_INFO, "NetworkMap","RemoveFromNetworkMap(): There is only one node in the network (first = last)");

		MyNetworkMap->FirstMember = NULL;
		MyNetworkMap->LastNetworkMember = NULL;
		free(member_to_remove->node_ip);
		free(member_to_remove);

	}
	else{
		/*
		 * the node that we want to remove-  need to check if its first, last or in the middle.
		 * first : point FirstMember to the second member.
		 * last : point LastMember to one before current last
		 * default : link previous and next nodes of the current node
		 */
		if (member_to_remove == MyNetworkMap->LastNetworkMember){

			member_to_remove->PrevNode->NextNode = NULL;
			MyNetworkMap->LastNetworkMember = member_to_remove->PrevNode;
			__android_log_print(ANDROID_LOG_INFO, "NetworkMap","RemoveFromNetworkMap(): Removing LastNetworkMember.New LastNetworkMember is: [%s]", MyNetworkMap->LastNetworkMember->node_ip);
		}
		else{
			if (member_to_remove == MyNetworkMap->FirstMember){
				member_to_remove->NextNode->PrevNode = NULL;
				MyNetworkMap->FirstMember = member_to_remove->NextNode;
				__android_log_print(ANDROID_LOG_INFO, "NetworkMap","RemoveFromNetworkMap(): Removing FirstMember. New FirstMember is: [%s]", MyNetworkMap->FirstMember->node_ip);
			}
			else{

				member_to_remove->NextNode->PrevNode = member_to_remove->PrevNode;
				member_to_remove->PrevNode->NextNode = member_to_remove->NextNode;
				__android_log_write(ANDROID_LOG_INFO, "NetworkMap","RemoveFromNetworkMap(): Removing node somewhere in the middle");
			}

		}

		free(member_to_remove->node_ip);
		free(member_to_remove);
	}

	MyNetworkMap->num_of_nodes -= 1;
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
			__android_log_print(ANDROID_LOG_INFO, "NetworkMap","The node with ip :[%s] is already in the network map", ip_to_check);
			return TRUE;
		}
		else{
			//this node still isn't the node we received
			__android_log_print(ANDROID_LOG_INFO, "NetworkMap","Current node : [%s] is not the node we want to look for [%s]", temp->node_ip,ip_to_check);
			temp = temp->NextNode;
		}
	}
	return FALSE;
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

	__android_log_print(ANDROID_LOG_INFO, "NetworkMap","CheckNodeExistence(): Check if [%s] has [%s] in the network map", Network_Head->node_base_ip,node_ip_to_check);

	if(maximum_distance_from_originator==0){
		__android_log_write(ANDROID_LOG_INFO,"NetworkMap","CheckNodeExistence(): maximum distance allowed reached 0. returning");
		return NULL;
	}

	//check if exists in the network map
	while(temp!=NULL){
		if(strcmp(temp->node_ip,node_ip_to_check)==0){
			//node was found in the network map
			__android_log_print(ANDROID_LOG_INFO, "NetworkMap","CheckNodeExistence(): The node with ip :[%s] was already in the network map", node_ip_to_check);
			return temp;
		}
		else{
			//this node still isn't the node we received
			__android_log_print(ANDROID_LOG_INFO, "NetworkMap","CheckNodeExistence(): Current node : [%s] is not the node we want to look for [%s]. checking subnetwork", temp->node_ip,node_ip_to_check);
			temp_in_subnetwork = GetNode(node_ip_to_check,temp->SubNetwork,maximum_distance_from_originator-1);
			if (temp_in_subnetwork != NULL)
			{
				__android_log_write(ANDROID_LOG_INFO,"NetworkMap","CheckNodeExistence(): Node was found in the subnetwork network");
				return temp_in_subnetwork;
			}
			temp = temp->NextNode;
		}
	}

	__android_log_write(ANDROID_LOG_INFO,"NetworkMap","CheckNodeExistence(): Node was not found in the network");
	return  temp;

}




jstring
Java_com_example_adhocktest_SenderUDP_SendUdpJNI( JNIEnv* env,
                                                  jobject thiz, jstring ip,jint port, jstring message, jint is_broadcast)
{
	int sock_fd;
	static int hey = 0;
	__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "SendUdpJNI(): HEY %d",hey++);
	const char *_ip = (*env)->GetStringUTFChars(env, ip, 0);
	const char *send_buf = (*env)->GetStringUTFChars(env, message, 0);   // Message to be sent

	////////////////
	/// create socket
	////////////////
	if (( sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
//		free(send_buf);
		return (*env)->NewStringUTF(env,"Cannot create socket");
	}

	__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "SendUdpJNI(): sock_fd values = %d", sock_fd);

	/////////////////
	// set socket options
	/////////////////
	int ret=setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, &is_broadcast, sizeof(is_broadcast));
	if (ret) {
//		free(send_buf);
		close(sock_fd);
		return (*env)->NewStringUTF(env,"Failed to set setsockopt()");
	}


	////////////////
	/// send
	////////////////
	struct sockaddr_in servaddr;

	memset((char*)&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);

	if ((inet_aton(_ip,&servaddr.sin_addr)) == 0) {
//		free(send_buf);
		close(sock_fd);
		return (*env)->NewStringUTF(env,"Cannot decode IP address");
	}

	char* hello_message_string;
	int retval;

	if (strcmp(send_buf, "HELLO_MSG:")==0) {
		hello_message_string = (char*)malloc(sizeof(char)*HELLO_MSG_LEN);
		strcpy(hello_message_string,"HELLO_MSG:");
		strcat(hello_message_string,MyNetworkMap->node_base_ip);
		strcat(hello_message_string,"(");
		GenerateHelloMsg(hello_message_string,MyNetworkMap);
		strcat(hello_message_string,")");
		retval = sendto(sock_fd, hello_message_string, strlen(hello_message_string), 0, (struct sockaddr*)&servaddr, sizeof(servaddr));
		free(hello_message_string);
	} else {
		retval = sendto(sock_fd, send_buf, strlen(send_buf), 0, (struct sockaddr*)&servaddr, sizeof(servaddr));
	}

	char str[100];
	if ( retval < 0) {
		sprintf(str, "sendto failed with %d", retval);
	} else {
		sprintf(str, "sendto() Success (retval=%d messge='%s' size=%d ip=%s.", retval,send_buf,strlen(send_buf),_ip);
	}
//	free(send_buf);

	close(sock_fd);
	__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "SendUdpJNI(): Gonna return");
	return (*env)->NewStringUTF(env,str);

}

jstring
Java_com_example_adhocktest_ReceiverUDP_RecvUdpJNI(JNIEnv* env1,
        jobject thiz)
{
	int PORT = 8888;
	int BUFLEN = 512;
	int retVal=-1;


	struct sockaddr_in my_addr, cli_addr;
	int sockfd, i;
	socklen_t slen=sizeof(cli_addr);
	char* buf;
	buf = (char*)malloc(BUFLEN*sizeof(char));
	for (i=0; i<BUFLEN; i++) {
		buf[i] = '\0';
	}

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) {
		free(buf);
		return (*env1)->NewStringUTF(env1, "socket");
	}


	bzero(&my_addr, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(PORT);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sockfd, (struct sockaddr* ) &my_addr, sizeof(my_addr))==-1) {
		free(buf);
		return (*env1)->NewStringUTF(env1, "bind");
	}

	//try to receive
	if (recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr*)&cli_addr, &slen)==-1)
		return (*env1)->NewStringUTF(env1, "errRecv");
//	//check if hello message - try to update network map if it isn't my own broadcast
	//if ((strstr(buf,HELLO_MSG)!=NULL) && (strstr(buf,MyNetworkMap->node_base_ip)==NULL))
	if (strstr(buf,HELLO_MSG)!=NULL && strcmp(inet_ntoa(cli_addr.sin_addr),MyNetworkMap->node_base_ip)!=0) // TODO: Check if the network pointer is correct
	{
		ProcessHelloMsg(strpbrk(buf,":")+1,strlen(strpbrk(buf,":")+1),MyNetworkMap);
//		CheckNodeExistence(inet_ntoa(cli_addr.sin_addr),buf,MyNetworkMap,DIRECT_NEIGHBOUR);
	}

	__android_log_print(ANDROID_LOG_INFO, "adhoc-jni.c",  "RecvUdpJNI(): Raw string: <%s>",buf);
	//if received successfully , close socket
	close(sockfd);

	return (*env1)->NewStringUTF(env1, buf); // only return the MSG part

}

int ProcessHelloMsg(char* buf,int buf_length,NetworkMap* network_to_add_to) {

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
					__android_log_write(ANDROID_LOG_INFO, "ProcessHelloMsg()",  "~~DETECTED ATTEMP TO ADD SELF~~");
					dont_add_sons = TRUE;
				} else {
					retVal = AddToNetworkMap(ip_to_add,network_to_add_to);
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






