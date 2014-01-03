package com.example.adhocktest;

import java.io.DataOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;


import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.app.Activity;
import android.content.Context;
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
	String essid="BENGAL";
	AdHocEnabler AHE;

	WifiManager wifi;
	private String subnet_prefix; 
	private String my_ip;
	
	//////
	// textViews
	private TextView txt_RX;
	private TextView tv_ip;
	private TextView tv_rem_ip;

	Spinner ip_spinner;
	String[] ip_array = { "192.168.2.0","192.168.2.255" , "192.168.2.207", "192.168.2.22","192.168.2.96" };
	private String target_ip;
	
//	
//	List<String> zoom = new ArrayList<>();
//	zoom.add("String 1");
//	zoom.add("String 2");
//	
	@Override
	protected void onCreate(Bundle savedInstanceState) {

		super.onCreate(savedInstanceState);
		
		setContentView(R.layout.activity_main);	 
		/////// determine self ip
		wifi = (WifiManager) getSystemService(Context.WIFI_SERVICE);
		subnet_prefix = "192.168.2.";
		my_ip = subnet_prefix+Integer.toString(Math.abs(wifi.getConnectionInfo().getMacAddress().hashCode()%255)); 
		
		int duration = Toast.LENGTH_SHORT;
		txt_RX = (TextView)findViewById(R.id.txt_RX);
		tv_ip = (TextView)findViewById(R.id.tv_ip);
		tv_rem_ip = (TextView)findViewById(R.id.tv_rem_ip);
				 
		final Toast toast1 = Toast.makeText(this, my_ip, duration);
		final Toast toast_send = Toast.makeText(this, "Sending", duration);

		AHE = new AdHocEnabler("BENGAL",my_ip);
		AHE.ActivateAdHoc();
		StartListening();
		
		//////////////////////////
		// Start broadcasting my ip 
		//////////////////////////
		Routing sendBCAST = new Routing(my_ip);
		sendBCAST.broadcastIP(true);
		
		toast1.show();
		tv_ip.setText("Local ip: "+my_ip);
        final Button b_send = (Button) findViewById(R.id.b_send);
        final Button b_exit = (Button) findViewById(R.id.b_exit);

        b_send.setOnClickListener(new View.OnClickListener() {
			
			@Override
			public void onClick(View v) {
				String newTXmsg = GetTextTX();
				ClearTextTX();
				SenderUDP senderUDP = new SenderUDP(target_ip,newTXmsg);
				
				try{
					senderUDP.sendMsg();
				}
				catch (IOException e){
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
        
        ArrayAdapter<String> adapter = new ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, ip_array);
        ip_spinner = (Spinner) findViewById(R.id.spinner_select_ip);
        ip_spinner.setAdapter(adapter);
        ip_spinner.setOnItemSelectedListener(this);
	}

	

	public String GetTextTX(){
		EditText mEdit = (EditText)findViewById(R.id.txt_TX);
		return mEdit.getText().toString();
	}
	public void ClearTextTX(){
		EditText mEdit = (EditText)findViewById(R.id.txt_TX);
		mEdit.setText("");
	}
	
	public void StartListening(){
		ReceiverUDP receiverUDP = new ReceiverUDP(this.txt_RX);
		receiverUDP.start();
	}



	@Override
	public void onItemSelected(AdapterView<?> arg0, View arg1, int arg2,
			long arg3) {
		int duration = Toast.LENGTH_SHORT;
		
		int position = ip_spinner.getSelectedItemPosition();
		target_ip = ip_array[position];
		tv_rem_ip.setText("Target ip: "+target_ip);
		final Toast toast_target_ip_change = Toast.makeText(this, "Target IP changed to "+target_ip, duration);
		toast_target_ip_change.show();
	}



	@Override
	public void onNothingSelected(AdapterView<?> arg0) {
		//// TODO Auto-generated method stub
		
	}
}
