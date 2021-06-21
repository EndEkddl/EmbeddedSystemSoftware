package org.example.ndk;

import android.os.Bundle;
import android.app.Activity;
import android.content.Intent;
import android.widget.Button;
import android.view.Menu;
import android.view.View;
import android.view.View.OnClickListener;

public class NDKExam extends Activity {

	@Override
	protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        OnClickListener listener = new OnClickListener(){
        	@Override
        	public void onClick(View v){
        		Intent intent = new Intent(NDKExam.this, StartActivity.class);
        		startActivity(intent);
        		System.out.println("onClick in NDKExam");
        	}
        };
        
		System.out.println("NDKExam here!!");
        
        Button btn = (Button)findViewById(R.id.start_btn);
        btn.setOnClickListener(listener);
    
        
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.menu.main, menu);
		return true;
	}

}
