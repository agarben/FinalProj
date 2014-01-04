package com.example.adhocktest;


import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Map.Entry;

import android.net.wifi.WifiManager;
import android.util.Log;

public class Routing{
	
	
	public native int InitializeMap( );	
    static {
        System.loadLibrary("adhoc-jni");
    }

	private int PORT = 8888;
	private String my_ip;
	private	DatagramPacket broadcast_ip_packet;
	private DatagramSocket broadcast_ip_socket;
	private WifiManager mWifi;
	private String BROADCAST_IP = "192.168.2.255";
	private InetAddress InetBroadcastAddress = null;
	private long time_between_ip_broadcasts = 1000; //ms
	private boolean use_ndk = true;
	private SenderUDP senderUDP;
	
	MainActivity _mActivity;

	private Map<String,Integer> _ip_time_map;

	
	public Routing(String ip_to_assign, MainActivity mActivity)
	{ 
		_mActivity = mActivity;
		_ip_time_map = new HashMap<String, Integer>();
		this.my_ip = ip_to_assign;
		senderUDP = new SenderUDP(BROADCAST_IP,"HELLO_FROM<"+my_ip+">");
		InitializeMap();
		if (!use_ndk) {

			try {
				this.InetBroadcastAddress = InetAddress.getByName(BROADCAST_IP);
				Log.i("GALPA","initialized broadcast ip InetAddress");
				this.broadcast_ip_socket = new DatagramSocket();
				Log.i("GALPA","initialized broadcast_ip_socket");
				this.broadcast_ip_socket.setBroadcast(true);
				Log.i("GALPA","set socket to broadcast");	
				
			}	catch (UnknownHostException e) {
				e.printStackTrace();
			} 
				catch (SocketException e) {
				e.printStackTrace();
			}
			this.broadcast_ip_packet = new DatagramPacket(this.my_ip.getBytes(), this.my_ip.getBytes().length, this.InetBroadcastAddress, PORT);	
		} 
	}
	
	/* The function stops/starts the broadcast IP thread: decision = true -> start
	 *                                                    decision = false -> stop
	 */
	@SuppressWarnings("deprecation")
	public void broadcastIP(boolean decision){
		
		if (decision){
			broadcastMyIP.start();
			Log.i("GALPA", "device with IP: " + this.my_ip + " got an order to start broadcasting IP");
		}
		else{
		    broadcastMyIP.stop();
			Log.i("GALPA", "device with IP: " + this.my_ip + " got an order to stop broadcasting IP");
		}
	}
	
	Thread broadcastMyIP = new Thread(new Runnable() {

        @Override
        public void run() {
             
        	while(true)
        	{
        		updateIpCounter();
        		
        		if (use_ndk) {
	        		try {
						Thread.sleep(time_between_ip_broadcasts);
    					senderUDP.sendMsg();
					} catch (InterruptedException e1) {
						// TODO Auto-generated catch block
						e1.printStackTrace();
					}
    				catch (IOException e){
    					e.printStackTrace();
    				}
        		} else {
        			
        			try{
						Log.i("GALPA", "JAVA:Broadcasting my IP " + my_ip);
						broadcast_ip_socket.send(broadcast_ip_packet);
						Log.i("GALPA","device with IP " + my_ip + " waits for " +time_between_ip_broadcasts/1000+" seconds before broadcasting again");
	        		Thread.sleep(time_between_ip_broadcasts);
	        		}	catch (IOException e){
	        				e.printStackTrace();
	        				Log.i("GALPA","IOException while broadcasting IP thread");
	        		}	catch(InterruptedException e){
	        				e.printStackTrace();
	        				Log.i("GALPA","InterruptedException while broadcasting IP thread");
	        		}
        		}
        	}

        }
	   });
	
	void processHello(String rx_msg) {
		_ip_time_map.put(rx_msg,5);
		_mActivity.adapterAdd(rx_msg);
	}


	void updateIpCounter() {
	    Log.i("Routing","UpdateIpCounter()");
		for (Map.Entry<String, Integer> entry : _ip_time_map.entrySet())
		{
			String key = entry.getKey();
			if (entry.getValue() == 0) {
				_mActivity.adapterRem(key);
			    Log.i("Routing","Removed from map :  Key= "+entry.getKey());
				_ip_time_map.remove(entry.getKey());
			} else {
			    Log.i("Routing","Before :  Key= "+entry.getKey()+" Value= "+ entry.getValue());
			    _ip_time_map.put(entry.getKey() , entry.getValue()-1 );
			    Log.i("Routing","After :  Key= "+entry.getKey()+" Value= "+ entry.getValue());
			}
		}
	}
}
 
