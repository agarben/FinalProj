package com.example.adhocktest;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.SocketException;

import android.os.Handler;
import android.util.Log;
import android.widget.ArrayAdapter;
import android.widget.TextView;

public class ReceiverUDP extends Thread{
	
	public native String  RecvUdpJNI( );	
    static {
        System.loadLibrary("adhoc-jni");
    }
    
	private int port = 8888;
	private DatagramSocket datagramSocket;
	public String ReceivedData;
	static byte[] data;
	private boolean use_ndk = true;
	
	//////////
	// Interaction with mainactiv
	//////////
	private String[] ip_arr;
	private Handler handler = new Handler();
	ArrayAdapter<String> adapter;
	
	Routing _routing;
	
	public ReceiverUDP(Routing routing){
		_routing  = routing;
	}
	
	
	public void run(){
		byte[] buffer = new byte[64000];
		// open socket
		
		try {
			if (!use_ndk) {
				Log.i("ReceiverUDP.java","JAVA: Opening socket");
				datagramSocket = new DatagramSocket(port);
			}
		} 
		catch (SocketException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		
		if (use_ndk) {
			Log.i("ReceiverUDP.java","NDK:Opening socket and listening");
		} else {
			Log.i("ReceiverUDP.java","Java:Listening to socket");
		}
		while (1<2)
		{	
			
			try {	
				if (use_ndk) {	 											// for the moment we only use NDK
					final String rx_str = new String(RecvUdpJNI());
					Log.i("ReceiverUDP.java","String is **: "+rx_str);
					handler.post(new Runnable(){
						public void run() {
			            	if (rx_str.startsWith("HELLO_FROM<") == true) { // TODO: And not and not FORWARD
			            		_routing.processHello(rx_str.substring(1));
			            	} else if (rx_str.startsWith("ignore")) {
			    				Log.i("ReceiverUDP.java","JAVA: Opening socket");
			            	} else if (rx_str.startsWith("~")) {
			            		MainActivity.fps_counter++;
			            		MainActivity.DataIn = MainActivity.stringToBytesUTFCustom(rx_str.substring(1));
			            	} else { 
			    				Log.i("Warning","Received a string that is not Hello message, ignore, or video data.");
			            	}
			            }});
				} else {
//					Log.i("ReceiverUDP.java","JAVA: Listening to socket");
//					DatagramPacket receivePacket = new DatagramPacket(buffer,buffer.length);
//					datagramSocket.receive(receivePacket);
//					final String strRX = new String(receivePacket.getData(), receivePacket.getOffset(), receivePacket.getLength());
//					handler.post(new Runnable(){
//			            public void run() {
//			            	tx_RX.setText(strRX);
//			            }});
				}
			} catch (NullPointerException n){
				Log.i("GAL","NullPointerException");
				n.printStackTrace();
			}
		}
	}
}