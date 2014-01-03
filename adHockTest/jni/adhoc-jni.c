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

	struct sockaddr_in my_addr, cli_addr;
	    int sockfd, i;
	    socklen_t slen=sizeof(cli_addr);
	    char buf[BUFLEN];

	    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
	    	return (*env1)->NewStringUTF(env1, "socket");


	    bzero(&my_addr, sizeof(my_addr));
	    my_addr.sin_family = AF_INET;
	    my_addr.sin_port = htons(PORT);
	    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	    if (bind(sockfd, (struct sockaddr* ) &my_addr, sizeof(my_addr))==-1)
	    	return (*env1)->NewStringUTF(env1, "bind");


	    while(1)
	    {
	        if (recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr*)&cli_addr, &slen)==-1)
	        	return (*env1)->NewStringUTF(env1, "errRecv");
	        close(sockfd);
	    	return (*env1)->NewStringUTF(env1, buf);


	    }

	    close(sockfd);

}

