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
	public native int InitializeMap( String ip_to_set );	
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
//		//senderUDP = new SenderUDP(BROADCAST_IP,"HELLO_FROM<"+my_ip+">");
		senderUDP = new SenderUDP(BROADCAST_IP,"HELLO_MSG:"); 
		InitializeMap(this.my_ip);
		if (!use_ndk) {
			try {
				this.InetBroadcastAddress = InetAddress.getByName(BROADCAST_IP);
				Log.i("Routing.java","initialized broadcast ip InetAddress");
				this.broadcast_ip_socket = new DatagramSocket();
				Log.i("Routing.java","initialized broadcast_ip_socket");
				this.broadcast_ip_socket.setBroadcast(true);
				Log.i("Routing.java","set socket to broadcast");	
				
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
			Log.i("Routing.java", "device with IP: " + this.my_ip + " got an order to start broadcasting IP");
		}
		else{
		    broadcastMyIP.stop();
			Log.i("Routing.java", "device with IP: " + this.my_ip + " got an order to stop broadcasting IP");
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
						Log.i("Routing.java", "JAVA:Broadcasting my IP " + my_ip);
						broadcast_ip_socket.send(broadcast_ip_packet);
						Log.i("Routing.java","device with IP " + my_ip + " waits for " +time_between_ip_broadcasts/1000+" seconds before broadcasting again");
	        		Thread.sleep(time_between_ip_broadcasts);
	        		}	catch (IOException e){
	        				e.printStackTrace();
	        				Log.i("Routing.java","IOException while broadcasting IP thread");
	        		}	catch(InterruptedException e){
	        				e.printStackTrace();
	        				Log.i("Routing.java","InterruptedException while broadcasting IP thread");
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
	    Log.i("Routing.java","UpdateIpCounter()");
		for (Map.Entry<String, Integer> entry : _ip_time_map.entrySet())
		{
		    Log.i("Routing.java","Entering iterator");
			String key = entry.getKey();
			if (entry.getValue() == 0) {
				_mActivity.adapterRem(key);
			    Log.i("Routing.java","Removed from map :  Key= "+entry.getKey());
				_ip_time_map.remove(entry.getKey());
			    break;  //TODO: Think of a more dynamic solution, this will make us delay other entries' counts
			} else {
			    _ip_time_map.put(entry.getKey() , entry.getValue()-1 );
			    Log.i("Routing.java","Updated value for Key= "+entry.getKey()+". New Value= "+ entry.getValue());
			}
		}
	}
}
 
