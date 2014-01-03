package com.example.adhocktest;


import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketException;
import java.net.UnknownHostException;

import android.net.DhcpInfo;
import android.net.wifi.WifiManager;
import android.util.Log;

public class Routing{

	private int PORT = 8888;
	private String my_ip;
	private	DatagramPacket broadcast_ip_packet;
	private DatagramSocket broadcast_ip_socket;
	private WifiManager mWifi;
	private String BROADCAST_IP = "192.168.2.255";
	private InetAddress InetBroadcastAddress = null;
	private long time_between_ip_broadcasts = 10000; //ms
	private boolean use_ndk = false;
	
	public Routing(String ip_to_assign)
	{ 
		this.my_ip = ip_to_assign;

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
		} else {
			Log.i("GALPA","Implement ndk broadcast");
		}
	}
	
	/* The function stops/starts the broadcast IP thread: decision = true -> start
	 *                                                    decision = false -> stop
	 */
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
        		if (!use_ndk) {
	        		try{
	        		Log.i("GALPA","JAVA:Broadcasting my IP " +my_ip);		
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
        		} else {
        			Log.i("GALPA","Implement ndk broadcast");
        		}
        	}

        }
	   });
	
		 /* private InetAddress getBroadcastAddress() throws IOException {
			    DhcpInfo dhcp = mWifi.getDhcpInfo();
			    if (dhcp == null) {
			      Log.d(TAG, "Could not get dhcp info");
			      return null;
			    }
	
			    int broadcast = (dhcp.ipAddress & dhcp.netmask) | ~dhcp.netmask;
			    byte[] quads = new byte[4];
			    for (int k = 0; k < 4; k++)
			      quads[k] = (byte) ((broadcast >> k * 8) & 0xFF);
			    return InetAddress.getByAddress(quads);
			  }
			  */
}


 
