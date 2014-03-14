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

/////
//DEFINE MEMBER IN NETWORK
/////
typedef enum {DIRECT_NEIGHBOUR, NEIGHBOUR_OF_NEIGHBOUR} node_closeness;
////////
//GLOBALS
////////
static const char HELLO_MSG[] = "HELLO_FROM";

////SINGLE NODE
typedef struct MemberInNetwork {
	node_closeness cur_node_closeness;
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
//MemberInNetwork* LastNetworkMember;
int tempGlobal;

/*
 * InitializeMap.
 */
jint
Java_com_example_adhocktest_Routing_InitializeMap(JNIEnv* env1,
        jobject thiz,jstring ip_to_init){

	MyNetworkMap = (NetworkMap*)malloc(sizeof(NetworkMap));
	if (MyNetworkMap==NULL){
		__android_log_print(ANDROID_LOG_INFO, "NETWORKMAP","InitializeMap failed. could not allocate memory for MyNetworkMap");
		return -1;
	}
	char const * get_my_ip = (*env1)->GetStringUTFChars(env1, ip_to_init, 0);
	strcpy(MyNetworkMap->node_base_ip,get_my_ip);
	MyNetworkMap->num_of_nodes=0;
	MyNetworkMap->FirstMember=NULL;

	MyNetworkMap->LastNetworkMember=NULL;
	__android_log_print(ANDROID_LOG_INFO, "NETWORKMAP","InitializeMap completed successfully. My IP : [%s]", MyNetworkMap->node_base_ip);
	return 0;

}

/*
 * AddToNetworkMap : receive node_ip, node_type - add the data to the network map
 */
int AddToNetworkMap(char* node_ip, node_closeness node_type, char* buf,NetworkMap * Network_Head){

	__android_log_print(ANDROID_LOG_INFO, "NETWORKMAP","Need to add node with ip :[%s] to network map with closeness value of %d", node_ip,node_type);

	MemberInNetwork * temp = (MemberInNetwork*)malloc(sizeof(MemberInNetwork));
	temp->NextNode = NULL;
	temp->cur_node_closeness = node_type;
	temp->node_ip = (char*)malloc(16*sizeof(char));
	__android_log_write(ANDROID_LOG_INFO, "NETWORKMAP","BEFORE MALLOC SUBNETWORK");
	temp->SubNetwork = (NetworkMap*)malloc(sizeof(NetworkMap));
	if (temp->SubNetwork == NULL)
	{
		__android_log_write(ANDROID_LOG_INFO, "NETWORKMAP","FAILURE TO ALLOCATE SUBNETWORK");
		return 0;
	}
	__android_log_write(ANDROID_LOG_INFO, "NETWORKMAP","AFTER MALLOC SUBNETWORK SUCCESS");
	temp->SubNetwork->FirstMember = NULL;
	temp->SubNetwork->LastNetworkMember = NULL;
	__android_log_write(ANDROID_LOG_INFO, "NETWORKMAP","AFTER NULL POINTER TO SUBNET FIRST AND LAST");
	strcpy(temp->SubNetwork->node_base_ip,node_ip);
	temp->SubNetwork->num_of_nodes = 0;
	strcpy(temp->node_ip,node_ip);
	__android_log_print(ANDROID_LOG_INFO, "NETWORKMAP","ip to add :[%s]. the ip that was added: [%s] ", node_ip,temp->node_ip);

	Network_Head->num_of_nodes += 1;
	__android_log_print(ANDROID_LOG_INFO, "NETWORKMAP","OK2 - num of nodes in base network of [%s] : %d ",Network_Head->node_base_ip, Network_Head->num_of_nodes);
	if (Network_Head->num_of_nodes == 1){
		__android_log_write(ANDROID_LOG_INFO, "NETWORKMAP","NETWORKHEAD -> NUM OF NODES == 1");
		temp->PrevNode=NULL;
		Network_Head->FirstMember = temp;
	}
	else{
		__android_log_write(ANDROID_LOG_INFO, "NETWORKMAP","NETWORKHEAD -> NUM OF NODES != 1");
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
	tempGlobal++;
	if (tempGlobal < 2)
		CheckNodeExistence("192.168.2.222",NULL,temp->SubNetwork,NEIGHBOUR_OF_NEIGHBOUR);

	return 0;

}




/*
 * RemoveFromNetworkMap : receive MemberInNetwork to remove
 */
int RemoveFromNetworkMap(MemberInNetwork * member_to_remove){

	__android_log_print(ANDROID_LOG_INFO, "NETWORKMAP","RemoveFromNetworkMap: Received a request to remove node: [%s]", member_to_remove->node_ip);

	//check if there is only one member in network
	if (MyNetworkMap->FirstMember == MyNetworkMap->LastNetworkMember){

		__android_log_write(ANDROID_LOG_INFO, "NETWORKMAP","RemoveFromNetworkMap: There is only one node in the network (first = last)");

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
			__android_log_print(ANDROID_LOG_INFO, "NETWORKMAP","RemoveFromNetworkMap: Removing LastNetworkMember.New LastNetworkMember is: [%s]", MyNetworkMap->LastNetworkMember->node_ip);
		}
		else{
			if (member_to_remove == MyNetworkMap->FirstMember){
				member_to_remove->NextNode->PrevNode = NULL;
				MyNetworkMap->FirstMember = member_to_remove->NextNode;
				__android_log_print(ANDROID_LOG_INFO, "NETWORKMAP","RemoveFromNetworkMap: Removing FirstMember. New FirstMember is: [%s]", MyNetworkMap->FirstMember->node_ip);
			}
			else{

				member_to_remove->NextNode->PrevNode = member_to_remove->PrevNode;
				member_to_remove->PrevNode->NextNode = member_to_remove->NextNode;
				__android_log_write(ANDROID_LOG_INFO, "NETWORKMAP","RemoveFromNetworkMap: Removing node somewhere in the middle");
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
//int CheckNodeExistence(struct sockaddr_in cli_addr,char* buf){
//
//	int i;
//	int retVal = -1;
//	char node_ip[16];
//
//	MemberInNetwork * temp = MyNetworkMap->FirstMember;
//
//	//extract node-ip
//	strcpy(node_ip,inet_ntoa(cli_addr.sin_addr));
//
//	//check if exists in the network map
//	while(temp!=NULL){
//		if(strcmp(temp->node_ip,node_ip)==0){
//			//node was found in the network map
//			__android_log_print(ANDROID_LOG_INFO, "NETWORKMAP","The node with ip :[%s] is already in the network map", node_ip);
//			return 0;
//		}
//		else{
//			//this node still isn't the node we received
//			__android_log_print(ANDROID_LOG_INFO, "NETWORKMAP","Current node : [%s] is not the node we want to look for [%s]", temp->node_ip,node_ip);
//			temp = temp->NextNode;
//		}
//	}
//
//	//add node to network map.
//	retVal = AddToNetworkMap(node_ip,DIRECT_NEIGHBOUR);
//
//
//	return retVal;
//
//}

/*
 * CheckNodeExistence - checks if node exists in the network map, if it doesn't - adds it
 */
// TODO: Need to check closeness of the node and perhaps change it, need to add DEBUG method.
// TODO: check mem allocations
int CheckNodeExistence(char node_ip_to_check[16],char* buf, NetworkMap * Network_Head,node_closeness closeness_to_add){

	int i;
	int retVal = -1;

	MemberInNetwork * temp = Network_Head->FirstMember;

	__android_log_print(ANDROID_LOG_INFO, "NETWORKMAP","[CheckNodeExistence]: Check if [%s] has [%s] in the network map", Network_Head->node_base_ip,node_ip_to_check);

	//check if exists in the network map
	while(temp!=NULL){
		if(strcmp(temp->node_ip,node_ip_to_check)==0){
			//node was found in the network map
			__android_log_print(ANDROID_LOG_INFO, "NETWORKMAP","The node with ip :[%s] is already in the network map", node_ip_to_check);
			return 1;
		}
		else{
			//this node still isn't the node we received
			__android_log_print(ANDROID_LOG_INFO, "NETWORKMAP","Current node : [%s] is not the node we want to look for [%s]", temp->node_ip,node_ip_to_check);
			temp = temp->NextNode;
		}
	}

	//add node to network map.
	retVal = AddToNetworkMap(node_ip_to_check,closeness_to_add,buf,Network_Head);


	return  retVal;

}




jstring
Java_com_example_adhocktest_SenderUDP_SendUdpJNI( JNIEnv* env,
                                                  jobject thiz, jstring ip,jint port, jstring message, jint is_broadcast)
{
	int sock_fd;
	static int hey = 0;
	__android_log_print(ANDROID_LOG_INFO, "Send",  "HEY %d",hey++);
	const char *_ip = (*env)->GetStringUTFChars(env, ip, 0);
	const char *data = (*env)->GetStringUTFChars(env, message, 0);   // Message to be sent
	unsigned char *header = (char*)malloc((300)*sizeof(char));			// A customized header to be attached to the data
	////////////////
	// Create header and attach data to it
	///////////////
	int k;

	struct in_addr in_addr_holder;
	inet_pton(AF_INET, "192.168.2.12", header+ORIGINATOR_IP_OFFSET);
	inet_pton(AF_INET, "1.2.3.4",header+DESTINATION_IP_OFFSET);

	__android_log_print(ANDROID_LOG_INFO, "Send",  " ");
	__android_log_print(ANDROID_LOG_INFO, "Send",  "Header DWORD0:  [%d %d %d %d]", header[0],header[1],header[2],header[3]);
	__android_log_print(ANDROID_LOG_INFO, "Send",  "Header DWORD1:  [%d %d %d %d]", header[ORIGINATOR_IP_OFFSET],header[ORIGINATOR_IP_OFFSET+1],header[ORIGINATOR_IP_OFFSET+2],header[ORIGINATOR_IP_OFFSET+3]);
	__android_log_print(ANDROID_LOG_INFO, "Send",  "Header DWORD2:  [%d %d %d %d]", header[DESTINATION_IP_OFFSET],header[DESTINATION_IP_OFFSET+1],header[DESTINATION_IP_OFFSET+2],header[DESTINATION_IP_OFFSET+3]);

	__android_log_print(ANDROID_LOG_INFO, "Send",  "STACK 1");

	unsigned char *send_buf = (char*)malloc((1000+strlen(data))*sizeof(char));
	for (k=0; k<(96+strlen(data)); k++) { // TODO: Check if there's a better way of doing this (without copying byte after byte)
		if (k < 96) {
			send_buf[k] = header[k];
		} else
			send_buf[k] = data[k-96];
	}
	send_buf[k] = '\0';
	__android_log_print(ANDROID_LOG_INFO, "Send",  "k = %d",k);
	__android_log_print(ANDROID_LOG_INFO, "Send",  "Final send_buf DWORD0:  [%d %d %d %d]", send_buf[0],send_buf[1],send_buf[2],send_buf[3]);
	__android_log_print(ANDROID_LOG_INFO, "Send",  "Final send_buf DWORD1:  [%d %d %d %d]", send_buf[ORIGINATOR_IP_OFFSET],send_buf[ORIGINATOR_IP_OFFSET+1],send_buf[ORIGINATOR_IP_OFFSET+2],send_buf[ORIGINATOR_IP_OFFSET+3]);
	__android_log_print(ANDROID_LOG_INFO, "Send",  "Final send_buf DWORD2:  [%d %d %d %d]", send_buf[DESTINATION_IP_OFFSET],send_buf[DESTINATION_IP_OFFSET+1],send_buf[DESTINATION_IP_OFFSET+2],send_buf[DESTINATION_IP_OFFSET+3]);
	__android_log_print(ANDROID_LOG_INFO, "Send",  "Sendbuf DATA:  [%s]", send_buf+96);
	__android_log_print(ANDROID_LOG_INFO, "Send",  " ");
	__android_log_print(ANDROID_LOG_INFO, "Send",  "~~Final Header destination ip   = [%d %d %d %d]", send_buf[ORIGINATOR_IP_OFFSET],send_buf[ORIGINATOR_IP_OFFSET+1],send_buf[ORIGINATOR_IP_OFFSET+2],send_buf[ORIGINATOR_IP_OFFSET+3]);
 	__android_log_print(ANDROID_LOG_INFO, "Send",  "~~Final Header destination ip   = [%d %d %d %d]", send_buf[DESTINATION_IP_OFFSET],send_buf[DESTINATION_IP_OFFSET+1],send_buf[DESTINATION_IP_OFFSET+2],send_buf[DESTINATION_IP_OFFSET+3]);
	__android_log_print(ANDROID_LOG_INFO, "Send",  " ");
//	free(header);
//	free(data);
	////////////////
	/// create socket
	////////////////
	if (( sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {

//		free(send_buf);
		return (*env)->NewStringUTF(env,"Cannot create socket");
	}

	__android_log_write(ANDROID_LOG_INFO, "GALPA",  "sock_fd values");
	__android_log_print(ANDROID_LOG_INFO, "GALPA",  "sock_fd values = %d", sock_fd);

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

	int retval = sendto(sock_fd, send_buf, k, 0, (struct sockaddr*)&servaddr, sizeof(servaddr));

	char str[100];
	if ( retval < 0) {
		sprintf(str, "sendto failed with %d", retval);
	} else {
		sprintf(str, "sendto() Success (retval=%d messge='%s' size=%d ip=%s.", retval,send_buf,strlen(send_buf),_ip);
	}
//	free(send_buf);

	close(sock_fd);
	__android_log_print(ANDROID_LOG_INFO, "Send",  "Gonna return");
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
	if ((strstr(buf+96,HELLO_MSG)!=NULL) && (strstr(buf+96,MyNetworkMap->node_base_ip)==NULL))
	{
		CheckNodeExistence(inet_ntoa(cli_addr.sin_addr),buf,MyNetworkMap,DIRECT_NEIGHBOUR);
	}

	__android_log_print(ANDROID_LOG_INFO, "Recv","buf0 [%d %d %d %d]",buf[0],buf[1],buf[2],buf[3]);
	__android_log_print(ANDROID_LOG_INFO, "Recv","buf1 [%d %d %d %d]",buf[4],buf[5],buf[6],buf[7]);
	__android_log_print(ANDROID_LOG_INFO, "Recv","buf2 [%d %d %d %d]",buf[8],buf[9],buf[10],buf[11]);
	__android_log_print(ANDROID_LOG_INFO, "Recv","MSG [%s]", buf+96);
	//if received successfully , close socket
	close(sockfd);

	return (*env1)->NewStringUTF(env1, buf+96); // only return the MSG part

}






