package com.example.adhocktest;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.UnknownHostException;
import android.widget.Toast;

import android.util.Log;
import android.widget.Toast;

public class SenderUDP {

	public native String  SendUdpJNI( String ip, int port, String message, int is_broadcast);		
    static {
        System.loadLibrary("adhoc-jni");
    }
	
	
	//TODO: ORDER!!! 
	private String ip;
	private String msg;
	
	private DatagramSocket datagramSocket;
	private int receiverPort = 8888;
	private boolean use_ndk = true;
	
	final int duration = 2;

	public SenderUDP(String new_ip, String new_msg)
	{
		this.ip = new_ip;
		this.msg = new_msg;
	}
	
	public String sendMsg() throws IOException // TODO: better understand the try throw mechanism
	{
		// TODO: check data length
		String str = "";
		if (use_ndk) {
			Log.i("GALPA","NDK: sending to IPAddress "+ip);
			if (this.ip == "192.168.2.255") {
				str = SendUdpJNI(ip,receiverPort,msg,1);
			} else {
				str = SendUdpJNI(ip,receiverPort,msg,0);
			}
	 		Log.i("GALPA","NDK:SendUdpJNI returned: "+str);
		} else {
			InetAddress IPAddress = InetAddress.getByName(this.ip);
			DatagramPacket sendPacket;
			Log.i("GALPA","JAVA: sending to IPAddress "+IPAddress);
			datagramSocket = new DatagramSocket();
			
			datagramSocket.setBroadcast(true); // TODO: if .255 should be true
			
			sendPacket = new DatagramPacket(this.msg.getBytes(), this.msg.getBytes().length, IPAddress, receiverPort); // TODO: change 8888 to an integer value
			
			datagramSocket.send(sendPacket);
			datagramSocket.close();
		}
			
		return msg.getBytes().toString();
	}
	
	public void setTargetIp(String new_ip) {
		this.ip = new_ip;
	}
	public void setMessage(String new_msg) {
		this.msg = new_msg;
	}
	
}
