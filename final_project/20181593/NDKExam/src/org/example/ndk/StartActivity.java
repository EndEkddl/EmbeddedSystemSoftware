package org.example.ndk;

import org.example.ndk.R;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.widget.Toast;
import android.app.Activity;
import android.content.Intent;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;

public class StartActivity extends Activity{
	
	public native int openDevice();
	public native void startGame(int fd, Arg arg);
	public native void closeDevice(int fd);
	
	private int score=-1;
	
	private Handler mainHandler = new Handler() {
		public void handleMessage(Message msg){
			Bundle bundle;
			System.out.println("here!!!");

			switch(msg.what){
			case 0:
				bundle = msg.getData();				
				break;
			case -1:
				exitGame();
				break;
			case 1:
				bundle = msg.getData();
				score = bundle.getInt("cnt");
				break;
			default: break;
			}
			
		}
		
	};
	class Arg{
		int state, cnt;
	}
	
	class InputThread extends Thread{
		Handler handler;
		
		InputThread(Handler _handler){
			handler = _handler;
		}
		
		public void run(){
			int fd = openDevice();
			while(true){
				boolean exit = false;
				Arg arg = new Arg();
				Bundle bundle = new Bundle();
				Message msg = Message.obtain();
				
				startGame(fd, arg);
				
				switch(arg.state){
				case -1:
					exit = true;
					break;
				case 0:
					Log.i("MyTag", "input number : " + arg.cnt);
					msg.what = 0;
					msg.setData(bundle);
					break;
				case 1:
					bundle.putInt("cnt", arg.cnt);
					msg.what = 1;
					msg.setData(bundle);
					handler.sendMessage(msg);
					break;
				default: break;
				}
				if(exit) break;
			}
			closeDevice(fd);
			
			Message msg = Message.obtain();
			msg.what = -1;
			handler.sendMessage(msg);
		}
	}
	@Override
	protected void onCreate(Bundle savedInstanceState){
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_start2);
		System.loadLibrary("ndk-exam");
		
		InputThread iThread = new InputThread(mainHandler);
		iThread.setDaemon(true);
		iThread.start();
	    
	}
	
	private void exitGame(){
		System.out.println("exitGame here!!");
		Toast.makeText(getApplicationContext(), "You got the right answer in "+score+"tries.", Toast.LENGTH_LONG).show();
		finish();
	}
}
