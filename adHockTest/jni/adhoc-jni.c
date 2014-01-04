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


/////
//DEFINE MEMBER IN NETWORK
/////
typedef enum {DIRECT_NEIGHBOUR, NEIGHBOUR_OF_NEIGHBOUR} node_closeness;
////////
//GLOBALS
////////

////SINGLE NODE
typedef struct MemberInNetwork {
	node_closeness cur_node_closeness;
	char* node_ip;
	//need to add more parameters such as last received etc
} MemberInNetwork;

////NETWORK MAP
typedef struct NetworkMap {
	int num_of_nodes;
	MemberInNetwork* FirstMember;
} NetworkMap;

NetworkMap* MyNetworkMap;



/*
 * InitializeMap.
 */
jint
Java_com_example_adhocktest_Routing_InitializeMap(JNIEnv* env1,
        jobject thiz){

	MyNetworkMap = (NetworkMap*)malloc(sizeof(NetworkMap));
	if (MyNetworkMap==NULL)
		return -1;
	MyNetworkMap->num_of_nodes=0;
	MyNetworkMap->FirstMember=NULL;
	return 0;

}

/*
 * UpdateNetworkMap : receive node_ip, node_type - add the data to the network map
 */
//int UpdateNetworkMap(char* node_ip, node_closeness node_type){
//
//
//
//
//
//}

/*
 * CheckNodeExistence - checks if node exists in the network map, if it doesn't - adds it
 */
//TODO: Need to check closeness of the node and perhaps change it, need to add DEBUG method.
//int CheckNodeExistence(char* buf){
//
//	int retVal = -1;
//
//	//extract node-ip
//
//
//	//extract node
//
//	//check if exists in the network map
//
//
//	//if doesn't exist - add it
//	retVal = UpdateNetworkMap(node_ip,node_type);
//	if (retVal == -1)
//		return -1;
//	return 0;
//
//}





jstring
Java_com_example_adhocktest_SenderUDP_SendUdpJNI( JNIEnv* env,
                                                  jobject thiz, jstring ip,jint port, jstring message, jint is_broadcast)
{
	int sock_fd;

	char *_ip = (*env)->GetStringUTFChars(env, ip, 0);
	char *send_buf = (*env)->GetStringUTFChars(env, message, 0);

	////////////////
	/// create socket
	////////////////
	if (( sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		return (*env)->NewStringUTF(env,"Cannot create socket");
	}


//	__android_log_write(ANDROID_LOG_INFO, "GALPA",  "sock_fd values");
	__android_log_print(ANDROID_LOG_INFO, "GALPA",  "sock_fd values = %d", sock_fd);

	/////////////////
	// set socket options
	/////////////////
	int ret=setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, &is_broadcast, sizeof(is_broadcast));
	if (ret) {
		return (*env)->NewStringUTF(env,"Failed to set setsockopt()");
		close(sock_fd);
	}


	////////////////
	/// send
	////////////////
	struct sockaddr_in servaddr;

	memset((char*)&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);

	if ((inet_aton(_ip,&servaddr.sin_addr)) == 0) {
		close(sock_fd);
		return (*env)->NewStringUTF(env,"Cannot decode IP address");
	}
	int retval = sendto(sock_fd, send_buf, strlen(send_buf), 0, (struct sockaddr*)&servaddr, sizeof(servaddr));


	char str[100];
	if ( retval < 0) {
		sprintf(str, "sendto failed with %d", retval);
	} else {
		sprintf(str, "sendto() Success (retval=%d messge='%s' size=%d ip=%s.", retval,send_buf,strlen(send_buf),_ip);
	}
	close(sock_fd);
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

	__android_log_print(ANDROID_LOG_INFO, "GALPA",  "ADDRESS = %s", inet_ntoa(cli_addr.sin_addr));

	//if received successfully , close socket
	close(sockfd);

	return (*env1)->NewStringUTF(env1, buf);

}



int w00t(int x1,int x2){

	return x1+x2;
}



jstring
Java_com_example_adhocktest_Routing_CheckC(JNIEnv* env1,
        jobject thiz){


	int x;
	x=1;

	__android_log_print(ANDROID_LOG_INFO, "GALPA",  "x = 1 is now = %d", x);

	x = w00t(2,3);

	__android_log_print(ANDROID_LOG_INFO, "GALPA",  "x = w00t(2,3) is now = %d", x);

	return (*env1)->NewStringUTF(env1, "END");

}



