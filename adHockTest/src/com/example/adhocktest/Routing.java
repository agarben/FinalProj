package com.example.adhocktest;


import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketException;
import java.net.UnknownHostException;

import android.net.wifi.WifiManager;
import android.util.Log;

public class Routing{
	
	
	public native String CheckC( );	
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
	

	
	public Routing(String ip_to_assign, MainActivity mActivity)
	{ 
		_mActivity = mActivity;
		this.my_ip = ip_to_assign;
		senderUDP = new SenderUDP(BROADCAST_IP,"HELLO_FROM<"+my_ip+">");
		
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
		_mActivity.adapterAdd(rx_msg);
	}
}


 
