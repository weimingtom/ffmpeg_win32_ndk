package com.weikun.videodemo;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Bundle;
import android.app.Activity;
import android.util.Log;
import android.view.Menu;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;

public class MainActivity extends Activity implements SurfaceHolder.Callback, Runnable {

	private final String TAG = MainActivity.class.getSimpleName();

	private native int nativeInit();

	private native void nativeSurfaceInit(Object surface);

	private native void nativeSurfaceFinalize();

	private native void nativeVideoPlay();
	
	private native void nativeVideoStop();
	
	static {
		System.loadLibrary("ffmpegutils");
	}

	private AudioTrack mAudio;
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);

		findViewById(R.id.btn_play).setOnClickListener(new View.OnClickListener() {
			
			@Override
			public void onClick(View v) {
				new Thread(MainActivity.this).start();
			}
		});
		
		findViewById(R.id.btn_stop).setOnClickListener(new View.OnClickListener() {
			
			@Override
			public void onClick(View v) {
				nativeVideoStop();
			}
		});
		
		SurfaceView sv = (SurfaceView) this.findViewById(R.id.video_view);
		SurfaceHolder sh = sv.getHolder();
		sh.addCallback(this);
		
		int rate;
		int channels;
		int encoding;
		int bufSize;
		channels = AudioFormat.CHANNEL_OUT_STEREO;
		encoding = AudioFormat.ENCODING_PCM_16BIT;
		rate = 44100;
		bufSize = AudioTrack.getMinBufferSize(rate, channels, encoding);
		mAudio = new AudioTrack(AudioManager.STREAM_MUSIC, rate, channels,
				encoding, bufSize, AudioTrack.MODE_STREAM);
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.menu.main, menu);
		return true;
	}

	@Override
	public void surfaceChanged(SurfaceHolder holder, int format, int width,
			int height) {
		Log.i(TAG, "surfaceChanged");
		nativeSurfaceInit(holder.getSurface());
		
		int init = nativeInit();
		Log.i(TAG, "init result " + init);
	}

	@Override
	public void surfaceCreated(SurfaceHolder holder) {

	}

	@Override
	public void surfaceDestroyed(SurfaceHolder holder) {
		Log.i(TAG, "surfaceDestroyed");
		nativeSurfaceFinalize();
		if (mAudio != null) {
			mAudio.stop();
			mAudio.release();
			mAudio = null;
		}
	}
	
	public void log(String str) {
		Log.i(TAG, "log : " + str);
	}
	
	public void write(byte[] bytes) {
		//Log.i(TAG, "write : " + new String(bytes));
		int offset = 0;
		while (mAudio != null) {
			int ret = mAudio.write(bytes, offset, bytes.length - offset);
			if (ret == AudioTrack.ERROR_INVALID_OPERATION ||
				ret == AudioTrack.ERROR_BAD_VALUE) {
				break;
			}
			//FIXME:
			if (ret <= 0) {
				break;
			}
			Log.e(TAG, "write : " + ret + ", " + bytes.length);
			offset += ret;
			if (offset >= bytes.length) {
				break;
			}
		}
		Log.e(TAG, "write over : " + bytes.length);
	}
	
	@Override
	public void run() {
		if (mAudio != null) {
			mAudio.play();
		}
		nativeVideoPlay();
	}
}
