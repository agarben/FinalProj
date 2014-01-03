package com.example.adhocktest;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.SocketException;

import android.os.Handler;
import android.util.Log;
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
	
	private TextView tx_RX;
	private Handler handler = new Handler();
	
	public ReceiverUDP(TextView tx_RX){

		this.tx_RX = tx_RX;
	}
	
	public void run(){
		byte[] buffer = new byte[8000];
		
		// open socket
		try {
			if (!use_ndk) {
				Log.i("GALPA","JAVA: Opening socket");
				datagramSocket = new DatagramSocket(port);
			}
		} 
		catch (SocketException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		
		if (use_ndk) {
			Log.i("GALPA","NDK:Opening socket and listening");
		} else {
			Log.i("GALPA","Java:Listening to socket");
		}
		while (1<2)
		{
			
			try {
				
				if (use_ndk) {
					
					final String rx_str = new String(RecvUdpJNI());
					handler.post(new Runnable(){
			            public void run() {
			            	tx_RX.setText( rx_str );
			            }});
				} else {
					Log.i("GALPA","JAVA: Listening to socket");
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