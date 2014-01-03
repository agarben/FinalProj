package com.example.adhocktest;

import java.io.DataOutputStream;
import java.io.IOException;

public class AdHocEnabler {

	String[] cmdGalaxy2 = { 
			"svc wifi disable", 			//
			"wifi load",
			"ifconfig eth0 ",
			"ifconfig eth0 up", 
			"iwconfig eth0 mode Ad-hoc",
			"iwconfig eth0 essid ", 
			"iwconfig eth0 commit" };

	String essid = "";
	String IP = "";

	public AdHocEnabler (String new_essid,String new_ip)
	{
		this.essid 	= new_essid;		// Prepare string to be executed later
		this.IP		= new_ip;
		cmdGalaxy2[2] 	+= this.IP;
		cmdGalaxy2[5] 	+= this.essid;
	}
	
	public void ActivateAdHoc() // in case we get more device types need to edit this
	{
		Process proc; // child process to execute shell commands on
		try
		{
			proc = Runtime.getRuntime().exec("su");
			DataOutputStream os = new DataOutputStream(proc.getOutputStream());
			for (String tmpCmd : this.cmdGalaxy2)
			{
				os.writeBytes(tmpCmd+"\n");
			}
			os.writeBytes("exit\n");
			os.flush();
		}catch (IOException e){
			e.printStackTrace();
		}
			
	}
}
