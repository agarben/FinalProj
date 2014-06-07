package com.example.adhocktest;


import java.io.IOException;
import java.net.InetAddress;
import java.util.HashMap;
import java.util.Map;
import java.util.regex.Pattern;

import android.net.wifi.WifiManager;
import android.os.Handler;
import android.util.Log;

public class Routing{
	public native int InitializeMap( String ip_to_set );	
	public native String RefreshNetworkMapJNI();
	
    static {
        System.loadLibrary("adhoc-jni");
    }
	private Handler handler = new Handler();

	private String my_ip;
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
		senderUDP = new SenderUDP(BROADCAST_IP,"HELLO_MSG:"); 
		InitializeMap(this.my_ip);
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
//        		updateIpCounter();
        		String NetworkMemberListFromC = RefreshNetworkMapJNI();
        		RefreshAdapter(NetworkMemberListFromC);
        		try {
					Thread.sleep(time_between_ip_broadcasts);
					handler.post(new Runnable(){
						public void run() {
							long fps = MainActivity.fps_counter/(time_between_ip_broadcasts/1000);
							MainActivity.RefreshFpsTextView(fps);
							MainActivity.fps_counter=0;
						}
			    	});
					senderUDP.sendMsg();
				} catch (InterruptedException e1) {
					// TODO Auto-generated catch block
					e1.printStackTrace();
				}
				catch (IOException e){
					e.printStackTrace();
				}
        	}
        }
	   });
	
	void processHello(String rx_msg) {
		_ip_time_map.put(rx_msg,5);
		_mActivity.adapterAdd(rx_msg);
	}

	void RefreshAdapter(String MembersListStr) {
		Pattern pat = Pattern.compile(Pattern.quote("()"));
		String[] MemberListArray = pat.split(MembersListStr);
        int index;
        int exist=-1;
	    Log.i("Routing.java","Printing all members - array length is "+MemberListArray.length+"["+MembersListStr+"]");
		int i;
		if (MembersListStr.length()>0){
			for (i=0; i<MemberListArray.length; i++) {   //add new nodes
			    Log.i("Routing.java","Member "+i+" is ["+MemberListArray[i]+"]");
			    index = _mActivity.adapter.getPosition(MemberListArray[i]);
			    Log.i("Routing.java","Member "+i+" index is "+index);
			    final String final_str = MemberListArray[i];
			    if (index == -1){
			    	handler.post(new Runnable(){
						public void run() {
					    	_mActivity.adapterAdd(final_str);
						}
			    	});
			    }
			}
		}
		
//		RefreshRemover();
		/// remove dead nodes
		index = _mActivity.adapter.getCount();
	    Log.i("Routing.java","Entering loop with adapter size = "+index);
		for (i=0;i<index;i++){
			exist=-1;
			final int final_i = i;
		    Log.i("Routing.java","final_i = "+final_i);
			for (String s_tmp : MemberListArray){
				if (s_tmp.equals(_mActivity.adapter.getItem(i))){
				    Log.i("Routing.java","found s_tmp ["+s_tmp+"] , adapter item #" +i +" ["+_mActivity.adapter.getItem(i)+"]");
					exist=1;
				}
			}
		
			if (exist!=1){
				i=0;
		    	index--;
			    Log.i("Routing.java","exist is ["+exist+"] , i is " +i +" index is "+index);
		    	handler.post(new Runnable(){
					public void run() {
				    	_mActivity.adapterRem(_mActivity.adapter.getItem(final_i));
				    
					}
		    	});
			}
		}
		Log.i("MainActivity.java","Routing: adapter count is "+_mActivity.adapter.getCount());
		if (_mActivity.adapter.getCount() == 0) {
			handler.post(new Runnable(){
				public void run() {
			    	_mActivity.SetTargetIp("XX.XX.XX.XX");
			    	_mActivity.StopBroadcastingVideo();
				}
	    	});
		}
			
	}
//	
//	void RefreshRemover(String MemberToCheck, int member_index, String[] MemberListArray) {
//		
//	
//		for (each in MemberListArray){
//	//		if member exit flag=1
//			final String final_str = MemberToCheck;
//		}
//	}
//	
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
 
