package com.example.adhocktest;

import java.io.IOException;

import android.util.Log;

public class SenderUDP {

	public native void SendUdpJNI( String ip, String j_message, int is_broadcast);		
    static {
        System.loadLibrary("adhoc-jni");
    }
    
	//TODO: ORDER!!! 
	private String ip;
	private String msg;
	
	final int duration = 2;

	public SenderUDP(String new_ip, String new_msg)
	{
		this.ip = new_ip;
		this.msg = new_msg;
	}
	public SenderUDP(String new_ip, byte[] new_msg)
	{
		this.ip = new_ip;
		this.msg = new String(new_msg);
	}
	
	public String sendMsg() throws IOException // TODO: better understand the try throw mechanism
	{
		try {	
			// TODO: check data length		
			Log.i("SenderUDP.java","NDK: sending to IPAddress "+ip);
			SendUdpJNI(ip,msg,1); // Always broadcast
		} catch (StackOverflowError e) {
			Log.i("SenderUDP.java","CAUGHT 00");
			e.printStackTrace(); 
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
