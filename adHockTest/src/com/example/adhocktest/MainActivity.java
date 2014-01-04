package com.example.adhocktest;

import java.io.DataOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.app.Activity;
import android.content.Context;
import android.util.Log;
import android.view.Menu;
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

	// ////
	// textViews
	private TextView txt_RX;
	private TextView tv_ip;
	private TextView tv_rem_ip;

	Spinner ip_spinner;
	// String[] ip_array = { "","" , "", "","" };
	List<String> ip_array;
	ArrayAdapter<String> adapter;
	private String target_ip;

	@Override
	protected void onCreate(Bundle savedInstanceState) {

		super.onCreate(savedInstanceState);
		
		setContentView(R.layout.activity_main);	 
		///////
		// pre-startup actions
		///////
		if (!preStartupConfiguration()){
			Log.i("GALPA","STARTUP FAILED TO RESET WIFI");				
		}
			
		/////// determine self ip
		//wifi = (WifiManager) getSystemService(Context.WIFI_SERVICE);	
		subnet_prefix = "192.168.2.";
		my_ip = subnet_prefix
				+ Integer.toString(Math.abs(wifi.getConnectionInfo()
						.getMacAddress().hashCode() % 255));

		int duration = Toast.LENGTH_SHORT;
		txt_RX = (TextView) findViewById(R.id.txt_RX);
		tv_ip = (TextView) findViewById(R.id.tv_ip);
		tv_rem_ip = (TextView) findViewById(R.id.tv_rem_ip);

		final Toast toast1 = Toast.makeText(this, my_ip, duration);
		final Toast toast_send = Toast.makeText(this, "Sending", duration);

		AHE = new AdHocEnabler("BENGAL", my_ip);
		AHE.ActivateAdHoc();
		StartListening();

		// ////////////////////////
		// Start broadcasting my ip
		// ////////////////////////
		Routing sendBCAST = new Routing(my_ip);
		sendBCAST.broadcastIP(true);

		toast1.show();
		tv_ip.setText("Local ip: " + my_ip);
		final Button b_send = (Button) findViewById(R.id.b_send);
		final Button b_exit = (Button) findViewById(R.id.b_exit);

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
				toast_send.show();
			}

		});
		b_exit.setOnClickListener(new View.OnClickListener() {

			@Override
			public void onClick(View v) {
				System.exit(0);
			}

		});

		ip_array = new ArrayList<String>();
		ip_array.add("192.168.2.255");
		adapter = new ArrayAdapter<String>(this,
				android.R.layout.simple_spinner_item, ip_array);
		ip_spinner = (Spinner) findViewById(R.id.spinner_select_ip);
		ip_spinner.setAdapter(adapter);
		ip_spinner.setOnItemSelectedListener(this);
	}

	public String GetTextTX() {
		EditText mEdit = (EditText) findViewById(R.id.txt_TX);
		return mEdit.getText().toString();
	}

	public void ClearTextTX() {
		EditText mEdit = (EditText) findViewById(R.id.txt_TX);
		mEdit.setText("");
	}

	public void StartListening() {
		ReceiverUDP receiverUDP = new ReceiverUDP(this.txt_RX, this);
		receiverUDP.start();
	}
	
	public boolean preStartupConfiguration(){
		
		boolean retVal;
		
		wifi = (WifiManager) getSystemService(Context.WIFI_SERVICE);
		Log.i("GALPA","preStartupConfiguration: Try to enable wifi");	
		retVal = wifi.setWifiEnabled(true);
		if (!retVal){
			Log.i("GALPA","preStartupConfiguration: Failed to enable wifi");			
			return retVal;
		}
		try {
		    Thread.sleep(2000);
		} catch(InterruptedException ex) {
		    Thread.currentThread().interrupt();
		}
		Log.i("GALPA","preStartupConfiguration: Try to disable wifi");			
		retVal = wifi.setWifiEnabled(false);
		if (!retVal){
			Log.i("GALPA","preStartupConfiguration: Failed to disable wifi");			
			return retVal;
		}
		try {
		    Thread.sleep(2000);
		} catch(InterruptedException ex) {
		    Thread.currentThread().interrupt();
		}
		Log.i("GALPA","preStartupConfiguration: Successfully reset wifi");		
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

	@Override
	public void onItemSelected(AdapterView<?> arg0, View arg1, int arg2,
			long arg3) {
		int duration = Toast.LENGTH_SHORT;

		int position = ip_spinner.getSelectedItemPosition();
		target_ip = ip_array.get(position);
		tv_rem_ip.setText("Target ip: " + target_ip);
		final Toast toast_target_ip_change = Toast.makeText(this,
				"Target IP changed to " + target_ip, duration);
		toast_target_ip_change.show();
	}

	@Override
	public void onNothingSelected(AdapterView<?> arg0) {
		// // TODO Auto-generated method stub

	}

}
