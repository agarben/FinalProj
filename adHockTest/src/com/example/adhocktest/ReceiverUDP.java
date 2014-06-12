package com.example.adhocktest;

import java.util.Date;

import android.os.Handler;
import android.util.Log;
import android.widget.ArrayAdapter;

public class ReceiverUDP extends Thread{
	
	public native String  RecvUdpJNI(int _is_mng );	
    static {
        System.loadLibrary("adhoc-jni");
    }
    
	public String ReceivedData;
	static byte[] data;

	Date Date_start = new Date();
	Date Date_end = new Date();
	long time_diff_ms = 0;
	
	boolean _is_mng;
	//////////
	// Interaction with mainactiv
	//////////
	private String[] ip_arr;
	private Handler handler = new Handler();
	ArrayAdapter<String> adapter;
	
	Routing _routing;
	
	public ReceiverUDP(Routing routing, boolean is_mng) {
		_routing  = routing;
		_is_mng = is_mng;
	}
	
	
	public void run(){
		while (1<2)
		{	
			try {	
					Log.i("ReceiverUDP.java","NDK:Opening socket and listening");
					
					final String rx_str = new String(RecvUdpJNI((_is_mng) ? 1:0));
    				
					Log.i("ReceiverUDP.java","String_rx returned from JNI is: "+rx_str);
					handler.post(new Runnable(){
						public void run() {
			            	if (rx_str.startsWith("HELLO_FROM<") == true) { // TODO: And not and not FORWARD
			            		_routing.processHello(rx_str.substring(1));
			            	} else if (rx_str.startsWith("ignore")) {
			            	} else if (rx_str.startsWith("~")) {
			            		MainActivity.fps_counter++;
								Log.i("ReceiverUDP.java","Setting image: "+rx_str.substring(1));
			            		MainActivity.DataIn = MainActivity.stringToBytesUTFCustom(rx_str.substring(1));
			            	} else { 
			    				Log.i("Warning","Received a string that is not Hello message, ignore, or video data.");
			            	}
			            }});
			} catch (NullPointerException n){
				Log.i("GAL","NullPointerException");
				n.printStackTrace();
			}
		}
	}
}