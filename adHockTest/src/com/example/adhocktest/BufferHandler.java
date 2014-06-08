package com.example.adhocktest;

import java.util.Date;
import android.util.Log;

public class BufferHandler extends Thread {


	public native void  RunSendingDaemonJNI();	// 
    static {
        System.loadLibrary("adhoc-jni");
    }
	
    public BufferHandler() {
    	
	}
	
	public void run(){
		Log.i("BufferHandler.java","Starting SendingDaemon");
		RunSendingDaemonJNI();
	}
}
