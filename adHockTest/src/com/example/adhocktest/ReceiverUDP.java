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
	private TextView tx_RX;
	private String[] ip_arr;
	private Handler handler = new Handler();
	ArrayAdapter<String> adapter;
	
	Routing _routing;
	
	public ReceiverUDP(TextView tx_RX, Routing routing){

		this.tx_RX = tx_RX;
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
				if (use_ndk) {
					final String rx_str = new String(RecvUdpJNI());
					Log.i("ReceiverUDP.java","String is **: "+rx_str);
					handler.post(new Runnable(){
						public void run() {
			            	if (rx_str.startsWith("HELLO_FROM<") == true) {
			            		_routing.processHello(rx_str);
			            	} else {
//			            		if(!rx_str.startsWith("ignore")){
//				            		tx_RX.clearComposingText();
//				            		tx_RX.setText( rx_str );
//			            		}
			            		MainActivity.fps_counter++;
			            		MainActivity.DataIn = MainActivity.stringToBytesUTFCustom(rx_str);
			            		
			            	}
			            }});
				} else {
					Log.i("ReceiverUDP.java","JAVA: Listening to socket");
					DatagramPacket receivePacket = new DatagramPacket(buffer,buffer.length);
					datagramSocket.receive(receivePacket);
					final String strRX = new String(receivePacket.getData(), receivePacket.getOffset(), receivePacket.getLength());
					handler.post(new Runnable(){
			            public void run() {
			            	tx_RX.setText(strRX);
			            }});
				}
			} catch (IOException e) {
				// TODO Auto-generated catch block
				Log.i("GAL","ioException");
				e.printStackTrace();
			} catch (NullPointerException n){
				Log.i("GAL","NullPointerException");
				n.printStackTrace();
			}
		}
	}
}