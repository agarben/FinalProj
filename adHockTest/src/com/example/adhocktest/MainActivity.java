package com.example.adhocktest;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.ImageFormat;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.graphics.YuvImage;
import android.hardware.Camera;
import android.hardware.Camera.CameraInfo;
import android.hardware.Camera.Parameters;
import android.hardware.Camera.PreviewCallback;
import android.hardware.Camera.Size;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

public class MainActivity extends Activity implements OnItemSelectedListener {
	String essid = "BENGAL";
	AdHocEnabler AHE;

	WifiManager wifi;
	private String subnet_prefix;
	private String my_ip;

	////// Layout items
	private TextView tv_ip;
	private TextView tv_rem_ip;
	private TextView tv_version;
	private static TextView tv_fps_in;
	
	private Button b_exit;
	private Button b_start_stop;
	private Toast toast_my_ip;
	
	// video Layout items
	static ImageView video_feed_view;
	private SurfaceView video_preview_view;
	private SurfaceHolder video_preview_surf_holder;
	private int cameraId = 0;
	private static Camera camera;
    boolean Pause = true;
    static boolean start = false;
    static boolean finish = true;
    static boolean CameraOn = false;
    static byte[] jdata;
    static byte[] DataIn;
	static byte[] DataInJpeg;  
	Size previewSize;
	int imgQuality = 20;
	int debug_video = 0;
	public static int fps_counter=0;
	public static long fps;
	
	private Handler handler = new Handler(); // TODO: dont forget to delete
	

	Date Date_start = new Date();
	Date Date_end = new Date();
	long time_diff_ms;
	
	////// spinner params
	public Spinner ip_spinner;
	List<String> ip_array;
	public ArrayAdapter<String> adapter;
	private String target_ip = "192.168.2.255";

	////// pointers to instances of other objects of the app
	private Routing routing;

	////// Toasts
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {

		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);
		// /////
		// pre-startup actions
		// /////
		if (!preStartupConfiguration()) {
			Log.i("MainActivity", "STARTUP FAILED TO RESET WIFI");
		}
		
		/////////////////////////
		// Enable Ad-hoc
		/////////////////////////
		AHE = new AdHocEnabler("BENGAL", my_ip);
		AHE.ActivateAdHoc();
		
		/////////////////////////
		// Layout items
		/////////////////////////
		initLayoutPointers();
		
		b_start_stop.setOnClickListener(new View.OnClickListener() {

			@Override
			public void onClick(View v) {
				
				if (b_start_stop.getText().equals("Start")) {
					StartBroadcastingVideo();
				} else {
					StopBroadcastingVideo();
				}
			}

		});
		
		b_exit.setOnClickListener(new View.OnClickListener() {

			@Override
			public void onClick(View v) {
				System.exit(0);
			}

		});
		

		//////////////////////////
		// Start listeners and broadcast threads
		//////////////////////////
		routing = new Routing(my_ip, this);
		routing.broadcastIP(true);
		StartListening();
		StartBufferHandler();
	}

	///////////////////
	// Function implementations
	//////////////////
	public void StartBufferHandler() {
		BufferHandler buffer_handler = new BufferHandler();
		buffer_handler.start();
	}
	
	public void StartListening() {
		ReceiverUDP receiverUDP = new ReceiverUDP(this.routing, false);  // MNG  receiver
		ReceiverUDP receiverUDP_MNG = new ReceiverUDP(this.routing, true); // DATA receiver 
		receiverUDP.start();
		receiverUDP_MNG.start();
	}

	@SuppressLint("ShowToast")
	public boolean preStartupConfiguration() {
		boolean retVal = true;

		retVal &= resetWifi();		
		
		//////// Generate IP
		subnet_prefix = "192.168.2.";
		my_ip = subnet_prefix + Integer.toString(Math.abs(wifi.getConnectionInfo().getMacAddress().hashCode() % 255));
		/////// Init toasts
		int duration = Toast.LENGTH_SHORT;
		toast_my_ip = Toast.makeText(this, my_ip, duration);
		toast_my_ip.show();
		////// Init layout pointers and spinners
		initIpSpinner();
		initLayoutPointers();

		return retVal;
	}

	public void adapterAdd(String str_to_add) {
			
			adapter.add(str_to_add);
			adapter.notifyDataSetChanged();
	}
	
	public void adapterRem(String str_to_rem) {

		Log.i("MainActivity.java","Routing: Need to remove "+str_to_rem+", adapter count = "+adapter.getCount());
		final String final_str = str_to_rem;
		handler.post(new Runnable(){
			public void run() {
				adapter.remove(final_str);
				adapter.notifyDataSetChanged();
			}
		});

	}
	
	public void initLayoutPointers(){
		b_exit = (Button) findViewById(R.id.b_exit);
		b_start_stop = (Button) findViewById(R.id.b_start_stop);
		
		tv_ip = (TextView) findViewById(R.id.tv_ip);
		tv_rem_ip = (TextView) findViewById(R.id.tv_rem_ip);
		tv_version = (TextView) findViewById(R.id.tv_version);
		tv_fps_in = (TextView) findViewById(R.id.textView2);

		tv_version.setText("Version 0.0.1");
		tv_ip.setText("Local ip: " + my_ip);
		
		video_feed_view = (ImageView)findViewById(R.id.videoFeed);
		video_feed_view.setScaleType(ScaleType.FIT_XY);
		video_preview_view = (SurfaceView)findViewById(R.id.VideoPreview);
		video_preview_surf_holder = video_preview_view.getHolder();
		video_preview_surf_holder.setType(video_preview_surf_holder.SURFACE_TYPE_PUSH_BUFFERS);
		video_preview_surf_holder.addCallback(surfaceCallback);
		

	}

	//Surface setup    
	  SurfaceHolder.Callback surfaceCallback = new SurfaceHolder.Callback() {
	      public void surfaceCreated( SurfaceHolder holder )
	      {
	    	  Pause = false;
	    	  String Model=Build.MODEL;
	    	  if (Model.equals("GT-S5830")) 
	    		  cameraId=0;
	    	  else 
    		  cameraId = findFrontFacingCamera();
	          camera = Camera.open(cameraId);
	          camera.setDisplayOrientation(90);
//	          imageCount = 0;

	          try {
	              camera.setPreviewDisplay( video_preview_surf_holder );
	          } catch ( Throwable t ) {
	              Log.e( "surfaceCallback", "Exception in setPreviewDisplay()", t );
	          }
	          Log.e( getLocalClassName(), "END: surfaceCreated" );
	      }
	      //What to do on a preview change
	      public void surfaceChanged( SurfaceHolder holder, int format, int width, int height )
	      {
		          if ( camera != null)
		          {
		              camera.setPreviewCallback( new PreviewCallback() {
		                  public void onPreviewFrame( byte[] data, Camera camera ) {
		                	 if (!Pause){
		                  	  if ( camera != null )
		                      {
		                    	  if (DataIn != null && fps != 0){
		                    		//load incoming image
									Bitmap myBitmap =  BitmapFactory.decodeByteArray(DataIn, 0, DataIn.length);
									if (myBitmap != null){
										Matrix mat = new Matrix();
										mat.postRotate(90);
										Bitmap myBitmapRotated = Bitmap.createBitmap(myBitmap, 0, 0, myBitmap.getWidth(), myBitmap.getHeight(), mat, true);
										video_feed_view.setImageBitmap(myBitmapRotated);
									}
		                    	  } 
		                    	
		                    	  Camera.Parameters parameters = camera.getParameters();
		                          int imageFormat = parameters.getPreviewFormat();
		                          //Compress to jpeg	
		                          if ( imageFormat == ImageFormat.NV21 )
		                          {
		                              jdata = NV21toJpeg(data);
		                          }
		                          
		                          else if ( imageFormat == ImageFormat.JPEG || imageFormat == ImageFormat.RGB_565 )
		                          {
		                        	  jdata=data;
		                          }
		                          //Send the current preview to other user
		                          if ( jdata != null )
		                          {
		                        	  if (start){
		                        		  		Date_start = new Date();
		                        				String jdata_str = bytesToStringUTFCustom(jdata);
		                        				Date_end = new Date();
		                        				time_diff_ms = (Date_end.getTime() - Date_start.getTime());
		                        				Log.i("Timers", "Converting bytesToString took "+time_diff_ms+"ms");
		                        				if (target_ip != "192.168.2.255") {
			                        				 SenderUDP senderUDP = new SenderUDP(target_ip, jdata_str);   
						                				try {
						                					Date_start = new Date();
						                					senderUDP.sendMsg();
					                        				Date_end = new Date();
					                        				time_diff_ms = (Date_end.getTime() - Date_start.getTime());
					                        				Log.i("Timers", "Sending time was "+time_diff_ms+"ms");
						                					Thread.sleep(2); // TODO: Check the sleep value value
						                				} catch (IOException e) {
						                					e.printStackTrace();
						                				} catch (InterruptedException e) {
															// TODO Auto-generated catch block
															e.printStackTrace();
														}
						                				debug_video++;
		                        				}	
				                				
		                        		//  }
//		                        		  AppService.ImageCntOut++;
		                        	  }
		                          }
		                      }
		                  }
		                }
		              });
		              //Setup camera's parameters
		              Parameters parameters = camera.getParameters();
		              if ( parameters != null )
		              {
		                Camera.Size previewSize=getSmallestPreviewSize(parameters);
		                  parameters.setPreviewFpsRange(16000, 16000); 
		                  parameters.setPreviewSize(previewSize.width,previewSize.height);
		                  camera.setParameters( parameters );
		                  camera.startPreview();
		                  }
		          }
		      }
	      public void surfaceDestroyed(SurfaceHolder holder) {
	          if ( camera != null )
	          {
	              camera.stopPreview();
	              camera.release();
	              camera = null;
	          }
	      }
	  };
	  
	  private int findFrontFacingCamera() {
			int cameraId = -1;
			int numberOfCameras = Camera.getNumberOfCameras();
			for (int i = 0; i < numberOfCameras; i++) {
				CameraInfo info = new CameraInfo();
				Camera.getCameraInfo(i, info);
				if (info.facing == CameraInfo.CAMERA_FACING_BACK) {
					cameraId = i;
					break;
				}
			}
			return cameraId;
		}
	
	
	  byte[] NV21toJpeg(byte[] data){
			 previewSize = camera.getParameters().getPreviewSize(); 
			 YuvImage yuvimage=new YuvImage(data, ImageFormat.NV21, previewSize.width, previewSize.height, null);
			 ByteArrayOutputStream baos = new ByteArrayOutputStream();
			 yuvimage.compressToJpeg(new Rect(0, 0, previewSize.width, previewSize.height), imgQuality, baos);
			 return baos.toByteArray();
		}	
	
	//Function that find the smallest preview size
	private Camera.Size getSmallestPreviewSize(Camera.Parameters parameters) {
	   Camera.Size result=null;
	    for (Camera.Size size : parameters.getSupportedPreviewSizes()) {
	    	if (result == null) {
	    		result=size;
	    	}
	    	else {
	    		int resultArea=result.width * result.height;
	    		int newArea=size.width * size.height;
	    		if (newArea < resultArea) {
		        	result=size;
		   			}
	    	}
	    }
	    return(result);  
	}
	
	public void initIpSpinner() {
		ip_array = new ArrayList<String>();
		adapter = new ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, ip_array);
		ip_spinner = (Spinner) findViewById(R.id.spinner_select_ip);
		ip_spinner.setAdapter(adapter);
		ip_spinner.setOnItemSelectedListener(this);
	}

	public boolean resetWifi() {
		boolean retVal;
		
		wifi = (WifiManager) getSystemService(Context.WIFI_SERVICE);
		Log.i("MainActivity", "preStartupConfiguration: Try to enable wifi");
		retVal = wifi.setWifiEnabled(true);
		if (!retVal) {
			Log.i("MainActivity", "preStartupConfiguration: Failed to enable wifi");
			return retVal;
		}
		try {
			Thread.sleep(2000);
		} catch (InterruptedException ex) {
			Thread.currentThread().interrupt();
		}
		Log.i("MainActivity", "preStartupConfiguration: Try to disable wifi");
		retVal = wifi.setWifiEnabled(false);
		if (!retVal) {
			Log.i("MainActivity", "preStartupConfiguration: Failed to disable wifi");
			return retVal;
		}
		try {
			Thread.sleep(2000);
		} catch (InterruptedException ex) {
			Thread.currentThread().interrupt();
		}
		Log.i("MainActivity", "preStartupConfiguration: Successfully reset wifi");
		return retVal;
	}
	
	@Override
	public void onItemSelected(AdapterView<?> arg0, View arg1, int arg2, long arg3) {
		int duration = Toast.LENGTH_SHORT;

		int position = ip_spinner.getSelectedItemPosition();
		target_ip = ip_array.get(position);
		tv_rem_ip.setText("Target ip: " + target_ip);
		final Toast toast_target_ip_change = Toast.makeText(this, "Target IP changed to " + target_ip, duration);
		toast_target_ip_change.show();
	}

	@Override
	public void onNothingSelected(AdapterView<?> arg0) {
		// // TODO Auto-generated method stub
	}

	public static byte[] stringToBytesUTFCustom(String str) {
		 byte[] b = new byte[str.length() << 1];
		 for(int i = 0; i < str.length(); i++) {
		  char strChar = str.charAt(i);
		  int bpos = i << 1;
		  b[bpos] = (byte) ((strChar&0xFF00)>>8);
		  b[bpos + 1] = (byte) (strChar&0x00FF); 
		 }
		 return b;
		}
	public static String bytesToStringUTFCustom(byte[] bytes) {
		 char[] buffer = new char[bytes.length >> 1];
		 for(int i = 0; i < buffer.length; i++) {
		  int bpos = i << 1;
		  char c = (char)(((bytes[bpos]&0x00FF)<<8) + (bytes[bpos+1]&0x00FF));
		  buffer[i] = c;
		 }
		 return new String(buffer);
		}
	
	public void SetTargetIp(String new_ip) {
		target_ip = new_ip;
		tv_rem_ip.setText("Target ip: " + target_ip);
	}
	public void StopBroadcastingVideo() {
		start = false;
		b_start_stop.setText("Start");
	}
	public void StartBroadcastingVideo() {
		if ( target_ip != "XX.XX.XX.XX") {
			start = true;
			b_start_stop.setText("Stop");
		} else {
			// TODO: Add toast error
		}
	}
	public static void RefreshFpsTextView(long fps_in) {
		Log.i("MainActivity.java", "FPS: " + fps_in);
		tv_fps_in.setText("FPS:"+fps_in);
		fps = fps_in;
		if (fps_in == 0) {
			video_feed_view.setImageResource(R.drawable.wooden_bg);
		}
	}
}
