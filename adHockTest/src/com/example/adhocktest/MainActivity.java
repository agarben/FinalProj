package com.example.adhocktest;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.os.Handler;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
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
	private TextView txt_RX;
	private TextView tv_ip;
	private TextView tv_rem_ip;
	private Button b_send; 
	private Button b_exit;
	private Toast toast_my_ip;
	private Toast toast_sending;
	

	private Handler handler = new Handler(); // TODO: dont forget to delete
	
	////// spinner params
	Spinner ip_spinner;
	List<String> ip_array;
	ArrayAdapter<String> adapter;
	private String target_ip;

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
			Log.i("GALPA", "STARTUP FAILED TO RESET WIFI");
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
		b_send.setOnClickListener(new View.OnClickListener() {

			@Override
			public void onClick(View v) {
				String newTXmsg = GetTextTX();
				ClearTextTX();
				SenderUDP senderUDP = new SenderUDP(target_ip, newTXmsg);

				try {
					senderUDP.sendMsg();
				} catch (IOException e) {
					e.printStackTrace();
				}
				toast_sending.show();
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
	}

	///////////////////
	// Function implementations
	//////////////////
	public String GetTextTX() {
		EditText mEdit = (EditText) findViewById(R.id.txt_TX);
		return mEdit.getText().toString();
	}

	public void ClearTextTX() {
		EditText mEdit = (EditText) findViewById(R.id.txt_TX);
		mEdit.setText("");
	}

	public void StartListening() {
		ReceiverUDP receiverUDP = new ReceiverUDP(this.txt_RX, this.routing);
		receiverUDP.start();
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
		toast_sending = Toast.makeText(this, "Sending", duration);
		toast_my_ip.show();
		////// Init layout pointers and spinners
		initIpSpinner();
		initLayoutPointers();

		return retVal;
	}

	public void adapterAdd(String str_to_add) {

		str_to_add = str_to_add.replace("HELLO_FROM<", "");
		str_to_add = str_to_add.replace(">", "");

		if (adapter.getPosition(str_to_add) == -1) {
			if (!str_to_add.equals(this.my_ip)) {
				adapter.add(str_to_add);
				adapter.notifyDataSetChanged();
			}
		}
	}
	
	public void adapterRem(String str_to_rem) {
		str_to_rem = str_to_rem.replace("HELLO_FROM<", "");
		str_to_rem = str_to_rem.replace(">", "");

		final String processed_str = str_to_rem;
		handler.post(new Runnable(){
			public void run() {
				adapter.remove(processed_str);
				adapter.notifyDataSetChanged();
			}
		});
		//ip_array.remove(str_to_rem);
		Log.i("Routing","Need to remove "+str_to_rem);
	}
	
	public void initLayoutPointers(){
		b_send = (Button) findViewById(R.id.b_send);
		b_exit = (Button) findViewById(R.id.b_exit);
		
		txt_RX = (TextView) findViewById(R.id.txt_RX);
		tv_ip = (TextView) findViewById(R.id.tv_ip);
		tv_rem_ip = (TextView) findViewById(R.id.tv_rem_ip);

		tv_ip.setText("Local ip: " + my_ip);

	}
	
	public void initIpSpinner() {
		ip_array = new ArrayList<String>();
		ip_array.add("192.168.2.255");
		adapter = new ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, ip_array);
		ip_spinner = (Spinner) findViewById(R.id.spinner_select_ip);
		ip_spinner.setAdapter(adapter);
		ip_spinner.setOnItemSelectedListener(this);
	}

	public boolean resetWifi() {
		boolean retVal;
		
		wifi = (WifiManager) getSystemService(Context.WIFI_SERVICE);
		Log.i("GALPA", "preStartupConfiguration: Try to enable wifi");
		retVal = wifi.setWifiEnabled(true);
		if (!retVal) {
			Log.i("GALPA", "preStartupConfiguration: Failed to enable wifi");
			return retVal;
		}
		try {
			Thread.sleep(2000);
		} catch (InterruptedException ex) {
			Thread.currentThread().interrupt();
		}
		Log.i("GALPA", "preStartupConfiguration: Try to disable wifi");
		retVal = wifi.setWifiEnabled(false);
		if (!retVal) {
			Log.i("GALPA", "preStartupConfiguration: Failed to disable wifi");
			return retVal;
		}
		try {
			Thread.sleep(2000);
		} catch (InterruptedException ex) {
			Thread.currentThread().interrupt();
		}
		Log.i("GALPA", "preStartupConfiguration: Successfully reset wifi");
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

}
